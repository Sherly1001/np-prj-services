#include <ws.h>

int file_info_cmp(const void *a, const void *b) {
    const struct file_info *fa = a;
    const struct file_info *fb = b;

    return fa->file->id - fb->file->id;
}

void file_info_drop(void *a) {
    struct file_info *f = a;
    db_file_drop(f->file);
    vec_drop(f->wsis);
}

void msg_drop(void *msg) {
    struct my_msg *m = msg;
    free(m->payload);
    m->payload = NULL;
    m->len     = 0;
}

void *get_all_payload(vec_t *vec, size_t *len_o, int *type_o) {
    void  *payload = NULL;
    size_t len     = 0;

    const struct my_msg *pmsg = vec_get(vec, 0);

    if (pmsg) {
        *type_o = pmsg->is_bin;
    } else {
        return NULL;
    }

    for (size_t idx = 0; idx < vec->len; ++idx) {
        pmsg = vec_get(vec, idx);
        len += pmsg->len;
    }

    payload = malloc(len + 1);
    *len_o  = len;

    len = 0;
    for (size_t idx = 0; idx < vec->len; ++idx) {
        pmsg = vec_get(vec, idx);
        memcpy(payload + len, pmsg->payload + LWS_PRE, pmsg->len);
        len += pmsg->len;
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
                vec_new_r(struct my_per_session_data *, NULL, NULL, NULL);
            vhd->files = vec_new_r(
                struct file_info, NULL, file_info_cmp, file_info_drop);
            break;

        case LWS_CALLBACK_PROTOCOL_DESTROY:
            vec_drop(vhd->files);
            vec_drop(vhd->pss_list);
            break;

        case LWS_CALLBACK_ESTABLISHED:
            vec_add(vhd->pss_list, &pss);
            pss->wsi     = wsi;
            pss->v_read  = vec_new_r(struct my_msg, NULL, NULL, msg_drop);
            pss->v_write = vec_new_r(struct my_msg, NULL, NULL, msg_drop);

            if (mws && mws->onopen) {
                mws->onopen(wsi);
            }
            break;

        case LWS_CALLBACK_CLOSED:
            vec_drop(pss->v_read);
            vec_drop(pss->v_write);
            vec_remove_by(vhd->pss_list, &pss);

            if (mws && mws->onclose) {
                mws->onclose(wsi);
            }
            break;

        case LWS_CALLBACK_SERVER_WRITEABLE:
            pmsg = vec_get(pss->v_write, 0);
            if (!pmsg) break;

            flags = lws_write_ws_flags(
                pmsg->is_bin ? LWS_WRITE_BINARY : LWS_WRITE_TEXT,
                pmsg->is_first, pmsg->is_last);
            n = lws_write(wsi, pmsg->payload + LWS_PRE, pmsg->len, flags);
            if (n < pmsg->len) return 1;

            vec_remove(pss->v_write, 0);
            if (pss->v_write->len > 0) lws_callback_on_writable(wsi);
            break;

        case LWS_CALLBACK_RECEIVE:
            msg.len      = len;
            msg.is_first = (bool)lws_is_first_fragment(wsi);
            msg.is_last  = (bool)lws_is_final_fragment(wsi);
            msg.is_bin   = (bool)lws_frame_is_binary(wsi);
            msg.payload  = malloc(LWS_PRE + len);
            memcpy(msg.payload + LWS_PRE, in, len);
            vec_add(pss->v_read, &msg);

            if (msg.is_last) {
                all_payload = get_all_payload(
                    pss->v_read, &all_payload_len, &all_payload_type);

                if (mws && mws->onmessage) {
                    mws->onmessage(
                        wsi, all_payload, all_payload_len, all_payload_type);
                }

                free(all_payload);
                vec_drop(pss->v_read);
                pss->v_read = vec_new_r(struct my_msg, NULL, NULL, msg_drop);
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

    if (len <= MY_PSS_SIZE) {
        amsg.is_first = true;
        amsg.is_last  = true;
        amsg.is_bin   = is_bin;
        amsg.len      = len;
        amsg.payload  = malloc(len + LWS_PRE);
        memcpy(amsg.payload + LWS_PRE, msg, len);
        vec_add(pss->v_write, &amsg);
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
    vec_add(pss->v_write, &amsg);

    amsg.is_first = false;
    for (index = MY_PSS_SIZE; index < len - MY_PSS_SIZE; index += MY_PSS_SIZE) {
        amsg.payload = malloc(MY_PSS_SIZE + LWS_PRE);
        memcpy(amsg.payload + LWS_PRE, msg + index, MY_PSS_SIZE);
        vec_add(pss->v_write, &amsg);
    }

    amsg.is_last = true;
    amsg.len     = len - index;
    amsg.payload = malloc(len - index + LWS_PRE);
    memcpy(amsg.payload + LWS_PRE, msg + index, len - index);
    vec_add(pss->v_write, &amsg);

    lws_callback_on_writable(wsi);

    return len;
}

size_t my_ws_send_all(struct lws *wsi, struct lws *except, const void *msg,
    size_t len, bool is_bin) {
    const struct lws_protocols *prl = lws_get_protocol(wsi);
    struct my_per_vhost_data   *vhd =
        lws_protocol_vh_priv_get(lws_get_vhost(wsi), prl);

    for (size_t i = 0; i < vhd->pss_list->len; ++i) {
        struct my_per_session_data *pss =
            vec_get_r(struct my_per_session_data *, vhd->pss_list, i);
        if (pss->wsi == except) continue;
        size_t n = my_ws_send(pss->wsi, msg, len, is_bin);
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
            pss->v_read    = vec_new_r(struct my_msg, NULL, NULL, msg_drop);
            pss->v_write   = vec_new_r(struct my_msg, NULL, NULL, msg_drop);

            if (!onrequest) break;
            if (lws_hdr_total_length(wsi, WSI_TOKEN_GET_URI)) {
                onrequest(wsi, pss->path, NULL, 0);
            } else if (lws_hdr_total_length(wsi, WSI_TOKEN_OPTIONS_URI)) {
                my_http_send(wsi, 200,
                    "Access-Control-Allow-Origin: *\r\n"
                    "Access-Control-Allow-Headers: content-type\r\n",
                    "");
            }
            break;

        case LWS_CALLBACK_HTTP_BODY:
            msg.len     = len;
            msg.payload = malloc(LWS_PRE + len);
            memcpy(msg.payload + LWS_PRE, in, len);
            vec_add(pss->v_read, &msg);
            break;

        case LWS_CALLBACK_HTTP_BODY_COMPLETION:
            if (onrequest) {
                body = get_all_payload(pss->v_read, &body_len, &body_type);
                onrequest(wsi, pss->path, body, body_len);
                free(body);
            }
            break;

        case LWS_CALLBACK_CLOSED_HTTP:
            free(pss->path);
            vec_drop(pss->v_read);
            vec_drop(pss->v_write);
            break;

        case LWS_CALLBACK_HTTP_WRITEABLE:
            pmsg = vec_get(pss->v_write, 0);
            if (!pmsg) break;

            flags = lws_write_ws_flags(
                LWS_WRITE_HTTP, pmsg->is_first, pmsg->is_last);
            n = lws_write(wsi, pmsg->payload + LWS_PRE, pmsg->len, flags);
            if (n < pmsg->len) return 1;

            vec_remove(pss->v_write, 0);
            if (pss->v_write->len > 0) {
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

size_t my_http_send(
    struct lws *wsi, int stt, const char *headers, const char *body) {
    struct my_http_ss *pss = lws_wsi_user(wsi);

    size_t body_len = strlen(body);

    char headers_[1024], *p = headers_;

    if (lws_add_http_header_status(
            wsi, stt, (unsigned char **)&p, (unsigned char *)headers_ + 1024)) {

        lws_callback_on_writable(wsi);
        return 0;
    }
    sprintf(p,
        "%s"
        "Content-Length: %ld\r\n"
        "\r\n",
        headers ? headers : "", body_len);

    struct my_msg amsg;
    amsg.is_first = true;
    amsg.is_last  = false;
    amsg.is_bin   = false;
    amsg.len      = strlen(headers_);
    amsg.payload  = malloc(LWS_PRE + amsg.len);
    memcpy(amsg.payload + LWS_PRE, headers_, amsg.len);
    vec_add(pss->v_write, &amsg);

    size_t index = 0;

    amsg.is_first = false;
    amsg.len      = MY_PSS_SIZE;
    for (index = 0; body_len > MY_PSS_SIZE && index < body_len - MY_PSS_SIZE;
         index += MY_PSS_SIZE) {
        amsg.payload = malloc(MY_PSS_SIZE + LWS_PRE);
        memcpy(amsg.payload + LWS_PRE, body + index, MY_PSS_SIZE);
        vec_add(pss->v_write, &amsg);
    }

    amsg.is_last = true;
    amsg.len     = body_len - index;
    amsg.payload = malloc(body_len - index + LWS_PRE);
    memcpy(amsg.payload + LWS_PRE, body + index, body_len - index);
    vec_add(pss->v_write, &amsg);

    lws_callback_on_writable(wsi);
    return strlen(headers_) + body_len;
}

size_t my_http_send_json(struct lws *wsi, int stt, struct json_object *json) {
    struct my_http_ss *pss = lws_wsi_user(wsi);

    const char *body =
        json_object_to_json_string_ext(json, JSON_C_TO_STRING_PLAIN);
    size_t body_len = strlen(body);

    char headers[1024], *p = headers;

    if (lws_add_http_header_status(
            wsi, stt, (unsigned char **)&p, (unsigned char *)headers + 1024)) {

        lws_callback_on_writable(wsi);
        return 0;
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
    vec_add(pss->v_write, &amsg);

    size_t index = 0;

    amsg.is_first = false;
    amsg.len      = MY_PSS_SIZE;
    for (index = 0; body_len > MY_PSS_SIZE && index < body_len - MY_PSS_SIZE;
         index += MY_PSS_SIZE) {
        amsg.payload = malloc(MY_PSS_SIZE + LWS_PRE);
        memcpy(amsg.payload + LWS_PRE, body + index, MY_PSS_SIZE);
        vec_add(pss->v_write, &amsg);
    }

    amsg.is_last = true;
    amsg.len     = body_len - index;
    amsg.payload = malloc(body_len - index + LWS_PRE);
    memcpy(amsg.payload + LWS_PRE, body + index, body_len - index);
    vec_add(pss->v_write, &amsg);

    lws_callback_on_writable(wsi);
    return body_len;
}
