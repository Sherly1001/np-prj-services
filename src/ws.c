#include <ws.h>

void files_info_drop(void *ff) {
    struct files_info *f = ff;
    db_file_drop(f->file);
    vec_destroy(f->wsis);
}

void msg_destroy(void *msg) {
    struct my_msg *m = msg;
    free(m->payload);
    m->payload = NULL;
    m->len     = 0;
}

void *get_all_payload(
    struct lws_ring *ring, uint32_t *tail, size_t *len_o, int *type_o) {
    void                *payload = NULL;
    const struct my_msg *pmsg;
    uint32_t             old_tail = *tail;
    size_t               len      = 0;

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
    *len_o  = len;

    len = 0;
    while ((pmsg = lws_ring_get_element(ring, tail))) {
        memcpy(payload + len, pmsg->payload + LWS_PRE, pmsg->len);
        len += pmsg->len;
        lws_ring_consume(ring, tail, NULL, 1);
    }

    return payload;
}

int my_ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
    void *user, void *in, size_t len) {
    const struct lws_protocols *prl = lws_get_protocol(wsi);
    struct my_ws               *mws = prl ? prl->user : NULL;
    struct my_per_session_data *pss = user;
    struct my_per_vhost_data   *vhd =
        lws_protocol_vh_priv_get(lws_get_vhost(wsi), prl);

    const struct my_msg *pmsg;
    struct my_msg        msg;
    void                *all_payload;
    size_t               all_payload_len;
    int                  all_payload_type;

    int    flags;
    size_t n;

    switch (reason) {
        case LWS_CALLBACK_PROTOCOL_INIT:
            vhd = lws_protocol_vh_priv_zalloc(
                lws_get_vhost(wsi), prl, sizeof(struct my_per_vhost_data));
            vhd->pss_list =
                vec_new_type(struct my_per_session_data *, NULL, NULL, NULL);
            vhd->files =
                vec_new_type(struct files_info, NULL, NULL, files_info_drop);
            break;

        case LWS_CALLBACK_PROTOCOL_DESTROY:
            vec_destroy(vhd->files);
            vec_destroy(vhd->pss_list);
            break;

        case LWS_CALLBACK_ESTABLISHED:
            vec_add(vhd->pss_list, &pss);
            pss->wsi       = wsi;
            pss->read_ring = lws_ring_create(
                sizeof(struct my_msg), MY_RING_DEPTH, msg_destroy);
            pss->write_ring = lws_ring_create(
                sizeof(struct my_msg), MY_RING_DEPTH, msg_destroy);

            if (mws && mws->onopen) {
                mws->onopen(wsi);
            }
            break;

        case LWS_CALLBACK_CLOSED:
            lws_ring_destroy(pss->read_ring);
            lws_ring_destroy(pss->write_ring);
            vec_remove(vhd->pss_list, vec_index_of(vhd->pss_list, &pss));

            if (mws && mws->onclose) {
                mws->onclose(wsi);
            }
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE:
            pmsg = lws_ring_get_element(pss->write_ring, &pss->write_tail);
            if (!pmsg) break;

            flags = lws_write_ws_flags(
                pmsg->is_bin ? LWS_WRITE_BINARY : LWS_WRITE_TEXT,
                pmsg->is_first, pmsg->is_last);
            n = lws_write(wsi, pmsg->payload + LWS_PRE, pmsg->len, flags);
            if (n < pmsg->len) return 1;

            lws_ring_consume(pss->write_ring, &pss->write_tail, NULL, 1);
            if (lws_ring_get_element(pss->write_ring, &pss->write_tail))
                lws_callback_on_writable(wsi);
            break;

        case LWS_CALLBACK_RECEIVE:
            msg.len      = len;
            msg.is_first = (bool)lws_is_first_fragment(wsi);
            msg.is_last  = (bool)lws_is_final_fragment(wsi);
            msg.is_bin   = (bool)lws_frame_is_binary(wsi);
            msg.payload  = malloc(LWS_PRE + len);
            if (!msg.payload) {
                lwsl_err("malloc fail");
                break;
            }
            memcpy(msg.payload + LWS_PRE, in, len);

            if (!lws_ring_insert(pss->read_ring, &msg, 1)) {
                return -1;
            }

            if (msg.is_last) {
                all_payload = get_all_payload(pss->read_ring, &pss->read_tail,
                    &all_payload_len, &all_payload_type);

                if (mws && mws->onmessage) {
                    mws->onmessage(
                        wsi, all_payload, all_payload_len, all_payload_type);
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

size_t my_ws_send(struct lws *wsi, const void *msg, size_t len, bool is_bin) {
    struct my_per_vhost_data *vhd =
        lws_protocol_vh_priv_get(lws_get_vhost(wsi), lws_get_protocol(wsi));
    struct my_per_session_data *pss = NULL;

    for (size_t i = 0; i < vhd->pss_list->len; ++i) {
        struct my_per_session_data **ps = vec_get(vhd->pss_list, i);
        if ((*ps)->wsi == wsi) {
            pss = *ps;
            break;
        }
    }

    if (!pss) return -1;

    struct my_msg amsg;
    size_t        n;

    if (len <= MY_PSS_SIZE) {
        amsg.is_first = true;
        amsg.is_last  = true;
        amsg.is_bin   = is_bin;
        amsg.len      = len;
        amsg.payload  = malloc(len + LWS_PRE);
        memcpy(amsg.payload + LWS_PRE, msg, len);
        n = lws_ring_insert(pss->write_ring, &amsg, 1);
        if (n < 1) return n;
        lws_callback_on_writable(wsi);
        return len;
    }

    size_t index;

    amsg.is_first = true;
    amsg.is_last  = false;
    amsg.is_bin   = is_bin;
    amsg.len      = MY_PSS_SIZE;
    amsg.payload  = malloc(MY_PSS_SIZE + LWS_PRE);
    memcpy(amsg.payload + LWS_PRE, msg, MY_PSS_SIZE);
    n = lws_ring_insert(pss->write_ring, &amsg, 1);
    if (n < 1) return n;

    amsg.is_first = false;
    for (index = MY_PSS_SIZE; index < len - MY_PSS_SIZE; index += MY_PSS_SIZE) {
        amsg.payload = malloc(MY_PSS_SIZE + LWS_PRE);
        memcpy(amsg.payload + LWS_PRE, msg + index, MY_PSS_SIZE);
        n = lws_ring_insert(pss->write_ring, &amsg, 1);
        if (n < 1) return index;
    }

    amsg.is_last = true;
    amsg.len     = len - index;
    amsg.payload = malloc(len - index + LWS_PRE);
    memcpy(amsg.payload + LWS_PRE, msg + index, len - index);
    n = lws_ring_insert(pss->write_ring, &amsg, 1);
    if (n < 1) return index;

    lws_callback_on_writable(wsi);

    return len;
}

size_t my_ws_send_all(struct lws *wsi, struct lws *except, const void *msg,
    size_t len, bool is_bin) {
    const struct lws_protocols *prl = lws_get_protocol(wsi);
    struct my_per_vhost_data   *vhd =
        lws_protocol_vh_priv_get(lws_get_vhost(wsi), prl);

    for (size_t i = 0; i < vhd->pss_list->len; ++i) {
        struct my_per_session_data **ppss = vec_get(vhd->pss_list, i);
        if ((*ppss)->wsi == except) continue;
        size_t n = my_ws_send((*ppss)->wsi, msg, len, is_bin);
        if (n < len) return n;
    }

    return len;
}
