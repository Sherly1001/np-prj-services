#include <stdio.h>
#include <signal.h>
#include <libwebsockets.h>

#include "ws-protocol.h"

static struct lws_protocols protocols[] = {
    { "http", lws_callback_http_dummy, 0, 0, 0, NULL, 0 },
    { "cce", my_ws_callback, sizeof(struct my_per_session_data), 2048, 0, NULL, 0 },
    LWS_PROTOCOL_LIST_TERM,
};

static const struct lws_retry_bo retry = {
    .secs_since_valid_ping = 3,
    .secs_since_valid_hangup = 10,
};

int interrupted = 0;
void sigint_handler(int sig) {
    interrupted = 1;
}

int main(int argc, const char **argv) {
    struct lws_context *context;
    struct lws_context_creation_info info;

    const char *p;
    int logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

    signal(SIGINT, sigint_handler);

    int port = 8080;
    if ((p = lws_cmdline_option(argc, argv, "-p")))
        port = atoi(p);

    if ((p = lws_cmdline_option(argc, argv, "-d")))
        logs = atoi(p);

    lws_set_log_level(logs, NULL);

    memset(&info, 0, sizeof(info));
    info.port = port;
    info.protocols = protocols;
    info.retry_and_idle_policy = &retry;
    info.options = LWS_SERVER_OPTION_VALIDATE_UTF8 |
        LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

    context = lws_create_context(&info);
    if (!context) {
        lwsl_err("lws init failed\n");
        return 1;
    }

    lwsl_user("listening at port %d\n", port);

    int n = 0;
    while (n >= 0 && !interrupted) {
        n = lws_service(context, 3);
    }

    lws_context_destroy(context);
}
