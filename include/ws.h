#include <libwebsockets.h>

#define MY_RING_DEPTH 4096
#define MY_PSS_SIZE 2048

struct my_msg {
    void *payload;
    size_t len;
    unsigned short is_first:1;
    unsigned short is_last:1;
    unsigned short is_bin:1;
};

struct my_per_session_data {
    struct my_per_session_data *pss_list;
    struct lws *wsi;
    struct lws_ring *read_ring;
    struct lws_ring *write_ring;
    uint32_t read_tail;
    uint32_t write_tail;
};

struct my_per_vhost_data {
    struct lws_context *ctx;
    struct lws_vhost *vhost;
    const struct lws_protocols *prl;
    struct my_per_session_data *pss_list;
};

typedef void (*onopen_t)(struct lws *wsi);
typedef void (*onclose_t)(struct lws *wsi);
typedef void (*onmessage_t)(
    struct lws *wsi,
    void *msg,
    uint64_t size,
    int type
);

struct my_ws {
    onopen_t onopen;
    onclose_t onclose;
    onmessage_t onmessage;
};

int my_ws_callback(
    struct lws *wsi,
    enum lws_callback_reasons reason,
    void *user,
    void *in,
    size_t len
);

int my_ws_send(
    struct lws *wsi,
    void *msg,
    size_t len,
    int is_bin
);

int my_ws_send_all(
    struct lws *wsi,
    struct lws *except,
    void *msg,
    size_t len,
    int is_bin
);

#define MY_WS_PROTOCOL(ws) { \
    "cce", my_ws_callback, sizeof(struct my_per_session_data), \
    MY_PSS_SIZE, 0, &ws, 0 \
}
