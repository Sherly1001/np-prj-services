#include <ws.h>

void file_info_drop(void *ff) {
    struct file_info *f = ff;
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
    void    *payload  = NULL;
    uint32_t old_tail = *tail;
    size_t   len      = 0;

    const struct my_msg *pmsg;

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
                vec_new_type(struct file_info, NULL, NULL, file_info_drop);
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
    struct my_per_session_data *pss = lws_wsi_user(wsi);

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

int my_http_callback(struct lws *wsi, enum lws_callback_reasons reason,
    void *user, void *in, size_t len) {
    const struct lws_protocols *prl = lws_get_protocol(wsi);

    onrequest_t onrequest = prl ? prl->user : NULL;

    struct my_http_ss   *pss = user;
    struct my_msg        msg;
    const struct my_msg *pmsg;

    void  *body;
    size_t body_len;
    int    body_type;

    int    flags;
    size_t n;

    switch (reason) {
        case LWS_CALLBACK_HTTP:
            pss->path = malloc(len + 1);
            memcpy(pss->path, in, len);
            pss->path[len] = '\0';
            pss->read_ring = lws_ring_create(
                sizeof(struct my_msg), MY_RING_DEPTH, msg_destroy);
            pss->write_ring = lws_ring_create(
                sizeof(struct my_msg), MY_RING_DEPTH, msg_destroy);

            if (lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI) && onrequest) {
                onrequest(wsi, pss->path, NULL, 0);
            }
            break;

        case LWS_CALLBACK_HTTP_BODY:
            msg.len     = len;
            msg.payload = malloc(LWS_PRE + len);
            memcpy(msg.payload + LWS_PRE, in, len);

            if (!lws_ring_insert(pss->read_ring, &msg, 1)) {
                return -1;
            }
            break;

        case LWS_CALLBACK_HTTP_BODY_COMPLETION:
            if (onrequest) {
                body = get_all_payload(
                    pss->read_ring, &pss->read_tail, &body_len, &body_type);
                onrequest(wsi, pss->path, body, body_len);
                free(body);
            }
            break;

        case LWS_CALLBACK_CLOSED_HTTP:
            free(pss->path);
            lws_ring_destroy(pss->read_ring);
            lws_ring_destroy(pss->write_ring);
            break;

        case LWS_CALLBACK_HTTP_WRITEABLE:
            pmsg = lws_ring_get_element(pss->write_ring, &pss->write_tail);
            if (!pmsg) break;

            flags = lws_write_ws_flags(
                LWS_WRITE_HTTP, pmsg->is_first, pmsg->is_last);
            n = lws_write(wsi, pmsg->payload + LWS_PRE, pmsg->len, flags);
            if (n < pmsg->len) return 1;

            lws_ring_consume(pss->write_ring, &pss->write_tail, NULL, 1);
            if (lws_ring_get_element(pss->write_ring, &pss->write_tail)) {
                lws_callback_on_writable(wsi);
            } else {
                lws_close_free_wsi(wsi, LWS_CLOSE_STATUS_NORMAL, NULL);
            }
            break;

        default:
            break;
    }
    return 0;
}

size_t my_http_send_json(struct lws *wsi, int stt, struct json_object *json) {
    struct my_http_ss *pss = lws_wsi_user(wsi);

    const char *body =
        json_object_to_json_string_ext(json, JSON_C_TO_STRING_PLAIN);
    size_t body_len = strlen(body);
    size_t rs       = 0;

    char headers[1024], *p = headers;

    if (lws_add_http_header_status(
            wsi, stt, (unsigned char **)&p, (unsigned char *)headers + 1024)) {
        rs = 0;
        goto __send_exit;
    }
    sprintf(p,
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: %ld\r\n"
        "\r\n",
        body_len);

    struct my_msg amsg;
    amsg.is_first = true;
    amsg.is_last  = false;
    amsg.is_bin   = false;
    amsg.len      = strlen(headers);
    amsg.payload  = malloc(LWS_PRE + amsg.len);
    memcpy(amsg.payload + LWS_PRE, headers, amsg.len);

    if (lws_ring_insert(pss->write_ring, &amsg, 1) < 1) {
        rs = 0;
        goto __send_exit;
    }

    size_t index = 0, n;

    amsg.is_first = false;
    amsg.len      = MY_PSS_SIZE;
    for (index = 0; body_len > MY_PSS_SIZE && index < body_len - MY_PSS_SIZE;
         index += MY_PSS_SIZE) {
        amsg.payload = malloc(MY_PSS_SIZE + LWS_PRE);
        memcpy(amsg.payload + LWS_PRE, body + index, MY_PSS_SIZE);
        n = lws_ring_insert(pss->write_ring, &amsg, 1);
        if (n < 1) {
            rs = index;
            goto __send_exit;
        }
    }

    amsg.is_last = true;
    amsg.len     = body_len - index;
    amsg.payload = malloc(body_len - index + LWS_PRE);
    memcpy(amsg.payload + LWS_PRE, body + index, body_len - index);
    n = lws_ring_insert(pss->write_ring, &amsg, 1);
    if (n < 1) {
        rs = index;
        goto __send_exit;
    }

    rs = body_len;

__send_exit:
    lws_callback_on_writable(wsi);
    free((char *)body);
    return rs;
}
