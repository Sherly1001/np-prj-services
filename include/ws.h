#ifndef __WS_H__
#define __WS_H__

#include <libwebsockets.h>

#include <bool.h>
#include <vec.h>
#include <db.h>

#define MY_RING_DEPTH 4096
#define MY_PSS_SIZE   2048

struct my_msg {
    void  *payload;
    size_t len;
    bool   is_first : 1;
    bool   is_last  : 1;
    bool   is_bin   : 1;
};

struct my_per_session_data {
    struct lws      *wsi;
    struct lws_ring *read_ring;
    struct lws_ring *write_ring;
    uint32_t         read_tail;
    uint32_t         write_tail;

    db_user_t *user;
    db_file_t *file;
};

struct files_info {
    db_file_t *file;
    vec_t     *wsis;
};

struct my_per_vhost_data {
    vec_t *pss_list;
    vec_t *files;
};

typedef void (*onopen_t)(struct lws *wsi);
typedef void (*onclose_t)(struct lws *wsi);
typedef void (*onmessage_t)(
    struct lws *wsi, const void *msg, size_t len, bool is_bin);

struct my_ws {
    onopen_t    onopen;
    onclose_t   onclose;
    onmessage_t onmessage;
};

int my_ws_callback(struct lws *wsi, enum lws_callback_reasons reason,
    void *user, void *in, size_t len);

size_t my_ws_send(struct lws *wsi, const void *msg, size_t len, bool is_bin);

size_t my_ws_send_all(struct lws *wsi, struct lws *except, const void *msg,
    size_t len, bool is_bin);

#define MY_WS_PROTOCOL(ws)                                                     \
    {                                                                          \
        "cce", my_ws_callback, sizeof(struct my_per_session_data),             \
            MY_PSS_SIZE, 0, &ws, 0                                             \
    }

#endif
