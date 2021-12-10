#include <libwebsockets.h>

#define RING_DEPTH 4096

struct my_msg {
    void *payload;
    size_t len;
    char binary;
    char first;
    char final;
};

struct my_per_session_data {
    struct lws_ring *ring;
    uint32_t msglen;
    uint32_t tail;
    uint8_t completed:1;
    uint8_t flow_controlled:1;
    uint8_t write_consume_pending:1;
};

int my_ws_callback(
    struct lws *wsi,
    enum lws_callback_reasons reason,
    void *user,
    void *in,
    size_t len
);
