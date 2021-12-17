#include "ws-protocol.h"

void msg_destroy(void *msg) {
    struct my_msg *m = msg;
    free(m->payload);
    m->payload = NULL;
    m->len = 0;
}

int my_ws_callback(
    struct lws *wsi,
    enum lws_callback_reasons reason,
    void *user,
    void *in,
    size_t len
) {
    struct my_per_session_data *pss = user;
    const struct my_msg *pmsg;
    struct my_msg msg;

    int flags, n, m;

    switch (reason) {
    case LWS_CALLBACK_PROTOCOL_INIT:
        break;

    case LWS_CALLBACK_ESTABLISHED:
        lwsl_warn("LWS_CALLBACK_ESTABLISHED\n");
        pss->ring = lws_ring_create(sizeof(struct my_msg), RING_DEPTH, msg_destroy);
        if (!pss->ring) return 1;
        pss->tail = 0;
        break;

    case LWS_CALLBACK_SERVER_WRITEABLE:
        if (pss->write_consume_pending) {
            lws_ring_consume_single_tail(pss->ring, &pss->tail, 1);
            pss->write_consume_pending = 0;
        }

        pmsg = lws_ring_get_element(pss->ring, &pss->tail);
        if (!pmsg) {
            break;
        }

        flags = lws_write_ws_flags(
                    pmsg->binary ? LWS_WRITE_BINARY : LWS_WRITE_TEXT,
                    pmsg->first, pmsg->final);

        m = lws_write(wsi, (unsigned char*)pmsg->payload + LWS_PRE, pmsg->len, flags);
        if (m < pmsg->len) {
            lwsl_err("ERROR %d writing to ws socket\n", m);
            return -1;
        }

        lwsl_warn("wrote %d: flags: 0x%x, first: %d, final: %d\n",
                m, flags, pmsg->first, pmsg->final);

        pss->write_consume_pending = 1;
        lws_callback_on_writable(wsi);

        if (pss->flow_controlled &&
                lws_ring_get_count_free_elements(pss->ring) > RING_DEPTH - 5) {
            lws_rx_flow_control(wsi, 1);
            pss->flow_controlled = 0;
        }
        break;

    case LWS_CALLBACK_RECEIVE:
        lwsl_user("LWS_CALLBACK_RECEIVE:\n%4ld (rpp %5ld, first %d, "
            "last %d, bin %d, msglen %d (+ %ld = %ld))\n",
            len, lws_remaining_packet_payload(wsi),
            lws_is_first_fragment(wsi),
            lws_is_final_fragment(wsi),
            lws_frame_is_binary(wsi), pss->msglen, len,
            pss->msglen + len);

        msg.first = lws_is_first_fragment(wsi);
        msg.final = lws_is_final_fragment(wsi);
        msg.binary = lws_frame_is_binary(wsi);
        n = lws_ring_get_count_free_elements(pss->ring);

        if (!n) {
            lwsl_warn("dropping\n");
            break;
        }

        if (msg.final) {
            pss->msglen = 0;
        } else {
            pss->msglen += len;
        }

        msg.len = len;
        msg.payload = malloc(LWS_PRE + len);
        if (!msg.payload) {
            lwsl_err("malloc fail\n");
            break;
        }

        memcpy((char*)msg.payload + LWS_PRE, in, len);
        if (!lws_ring_insert(pss->ring, &msg, 1)) {
            msg_destroy(&msg);
            lwsl_warn("dropping\n");
            break;
        }
        lws_callback_on_writable(wsi);

        if (n < 3 && !pss->flow_controlled) {
            pss->flow_controlled = 1;
            lws_rx_flow_control(wsi, 0);
        }
        break;

    case LWS_CALLBACK_CLOSED:
        lwsl_warn("LWS_CALLBACK_CLOSED\n");
        lws_ring_destroy(pss->ring);
        break;

    default:
        // lwsl_err("reason: %d\n", reason);
        break;
    }

    return 0;
}
