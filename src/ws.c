#include <ws.h>

void msg_destroy(void *msg) {
    struct my_msg *m = msg;
    free(m->payload);
    m->payload = NULL;
    m->len = 0;
}

void *get_all_payload(
    struct lws_ring *ring,
    uint32_t *tail,
    size_t *len_o,
    int *type_o
) {
    void *payload = NULL;
    const struct my_msg *pmsg;
    uint32_t old_tail = *tail;
    size_t len = 0;

    if ((pmsg = lws_ring_get_element(ring, &old_tail))) {
        *type_o = pmsg->is_bin;
    } else {
        return NULL;
    }

    old_tail = *tail;
    while ((pmsg = lws_ring_get_element(ring, &old_tail))) {
        lws_ring_consume(ring, &old_tail, NULL, 1);
        len += pmsg->len;
    }

    payload = malloc(len);
    *len_o = len;

    len = 0;
    while ((pmsg = lws_ring_get_element(ring, tail))) {
        memcpy(payload + len, pmsg->payload + LWS_PRE, pmsg->len);
        len += pmsg->len;
        lws_ring_consume(ring, tail, NULL, 1);
    }

    return payload;
}

int my_ws_callback(
    struct lws *wsi,
    enum lws_callback_reasons reason,
    void *user,
    void *in,
    size_t len
) {
    const struct lws_protocols *prl = lws_get_protocol(wsi);
    struct my_ws *mws = prl ? prl->user : NULL;
    struct my_per_session_data *pss = user;
    struct my_per_vhost_data *vhd = lws_protocol_vh_priv_get(
        lws_get_vhost(wsi), prl);

    const struct my_msg *pmsg;
    struct my_msg msg;
    void *all_payload;
    size_t all_payload_len;
    int all_payload_type;

    int flags;
    size_t n;

    switch (reason) {
    case LWS_CALLBACK_PROTOCOL_INIT:
        vhd = lws_protocol_vh_priv_zalloc(lws_get_vhost(wsi),
            prl, sizeof(struct my_per_vhost_data));
        vhd->ctx = lws_get_context(wsi);
        vhd->prl = prl;
        vhd->vhost = lws_get_vhost(wsi);
        break;

    case LWS_CALLBACK_PROTOCOL_DESTROY:
        break;

    case LWS_CALLBACK_ESTABLISHED:
        lws_ll_fwd_insert(pss, pss_list, vhd->pss_list);
        pss->wsi = wsi;
        pss->read_ring = lws_ring_create(
            sizeof(struct my_msg), MY_RING_DEPTH, msg_destroy);
        pss->write_ring = lws_ring_create(
            sizeof(struct my_msg), MY_RING_DEPTH, msg_destroy);

        if (mws && mws->onopen) {
            mws->onopen(wsi, pss);
        }
        break;

    case LWS_CALLBACK_CLOSED:
        lws_ll_fwd_remove(struct my_per_session_data, pss_list, pss, vhd->pss_list);
        lws_ring_destroy(pss->read_ring);
        lws_ring_destroy(pss->write_ring);

        if (mws && mws->onclose) {
            mws->onclose(wsi, pss);
        }
        break;

    case LWS_CALLBACK_SERVER_WRITEABLE:
        pmsg = lws_ring_get_element(pss->write_ring, &pss->write_tail);
        if (!pmsg) break;

        flags = lws_write_ws_flags(pmsg->is_bin ? LWS_WRITE_BINARY : LWS_WRITE_TEXT,
            pmsg->is_first, pmsg->is_last);
        n = lws_write(wsi, pmsg->payload + LWS_PRE, pmsg->len, flags);
        if (n < pmsg->len) return 1;

        lws_ring_consume(pss->write_ring, &pss->write_tail, NULL, 1);
        if (lws_ring_get_element(pss->write_ring, &pss->write_tail))
            lws_callback_on_writable(wsi);
        break;

    case LWS_CALLBACK_RECEIVE:
        msg.len = len;
        msg.is_first = lws_is_first_fragment(wsi);
        msg.is_last = lws_is_final_fragment(wsi);
        msg.is_bin = lws_frame_is_binary(wsi);
        msg.payload = malloc(LWS_PRE + len);
        if (!msg.payload) {
            lwsl_err("malloc fail");
            break;
        }
        memcpy(msg.payload + LWS_PRE, in, len);

        if (!lws_ring_insert(pss->read_ring, &msg, 1)) {
            return -1;
        }

        if (msg.is_last) {
            all_payload = get_all_payload(
                pss->read_ring, &pss->read_tail,
                &all_payload_len, &all_payload_type);

            if (mws && mws->onmessage) {
                mws->onmessage(wsi, pss, all_payload,
                    all_payload_len, all_payload_type);
            }

            free(all_payload);
        }

        break;

    default:
        // lwsl_err("reason: %d\n", reason);
        break;
    }

    return 0;
}

int my_ws_send(
    struct lws *wsi,
    struct my_per_session_data *pss,
    void *msg,
    size_t len,
    int is_bin
) {
    struct my_msg amsg;
    size_t n;

    if (len <= MY_PSS_SIZE) {
        amsg.is_first = 1;
        amsg.is_last = 1;
        amsg.is_bin = is_bin;
        amsg.len = len;
        amsg.payload = malloc(len + LWS_PRE);
        memcpy(amsg.payload + LWS_PRE, msg, len);
        n = lws_ring_insert(pss->write_ring, &amsg, 1);
        if (n < 1) return n;
        lws_callback_on_writable(wsi);
        return len;
    }

    size_t index;

    amsg.is_first = 1;
    amsg.is_last = 0;
    amsg.is_bin = is_bin;
    amsg.len = MY_PSS_SIZE;
    amsg.payload = malloc(MY_PSS_SIZE + LWS_PRE);
    memcpy(amsg.payload + LWS_PRE, msg, MY_PSS_SIZE);
    n = lws_ring_insert(pss->write_ring, &amsg, 1);
    if (n < 1) return n;

    amsg.is_first = 0;
    for (index = MY_PSS_SIZE; index < len - MY_PSS_SIZE; index += MY_PSS_SIZE) {
        amsg.payload = malloc(MY_PSS_SIZE + LWS_PRE);
        memcpy(amsg.payload + LWS_PRE, msg + index, MY_PSS_SIZE);
        n = lws_ring_insert(pss->write_ring, &amsg, 1);
        if (n < 1) return index;
    }

    amsg.is_last = 1;
    amsg.len = len - index;
    amsg.payload = malloc(len - index + LWS_PRE);
    memcpy(amsg.payload + LWS_PRE, msg + index, len - index);
    n = lws_ring_insert(pss->write_ring, &amsg, 1);
    if (n < 1) return index;

    lws_callback_on_writable(wsi);

    return len;
}

int my_ws_send_all(
    struct lws *wsi,
    struct lws *except,
    void *msg,
    size_t len,
    int is_bin
) {
    const struct lws_protocols *prl = lws_get_protocol(wsi);
    struct my_per_vhost_data *vhd = lws_protocol_vh_priv_get(
        lws_get_vhost(wsi), prl);

    lws_start_foreach_llp(struct my_per_session_data **, ppss, vhd->pss_list) {
        if ((*ppss)->wsi != except) {
            size_t n = my_ws_send((*ppss)->wsi, *ppss, msg, len, is_bin);
            if (n < len) return n;
        }
    } lws_end_foreach_llp(ppss, pss_list);

    return len;
}
