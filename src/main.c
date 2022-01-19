#include <stdio.h>
#include <signal.h>
#include <libwebsockets.h>

#include <ws.h>
#include <cmd.h>
#include <error.h>

void onopen(struct lws *wsi);
void onclose(struct lws *wsi);
void onmessage(struct lws *wsi, const void *msg, size_t len, bool is_bin);
struct my_ws ws = {onopen, onclose, onmessage};

static struct lws_protocols protocols[] = {
    {"http", lws_callback_http_dummy, 0, 0, 0, NULL, 0},
    MY_WS_PROTOCOL(ws),
    LWS_PROTOCOL_LIST_TERM,
};

struct lws_protocol_vhost_options pvo_opt = {NULL, NULL, "default", ""};

static struct lws_protocol_vhost_options pvo = {NULL, &pvo_opt, "cce", ""};

static const struct lws_retry_bo retry = {
    .secs_since_valid_ping   = 3,
    .secs_since_valid_hangup = 10,
};

int  interrupted = 0;
void sigint_handler() {
    interrupted = 1;
}

int main(int argc, const char **argv) {
    struct lws_context              *context;
    struct lws_context_creation_info info;

    const char *p;
    int         logs = LLL_USER | LLL_ERR | LLL_WARN | LLL_NOTICE;

    signal(SIGINT, sigint_handler);

    int port = 8080;
    if ((p = lws_cmdline_option(argc, argv, "-p"))) port = atoi(p);

    if ((p = lws_cmdline_option(argc, argv, "-d"))) logs = atoi(p);

    lws_set_log_level(logs, NULL);

    memset(&info, 0, sizeof(info));
    info.port      = port;
    info.pvo       = &pvo;
    info.protocols = protocols;

    info.retry_and_idle_policy = &retry;
    info.options =
        LWS_SERVER_OPTION_VALIDATE_UTF8 |
        LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;

    context = lws_create_context(&info);
    if (!context) {
        lwsl_err("lws init failed\n");
        return 1;
    }

    lwsl_user("listening at port %d\n", port);

    int n = 0;
    while (n >= 0 && !interrupted) {
        n = lws_service(context, 0);
    }

    lws_context_destroy(context);
}

void onopen(struct lws *wsi) {
    char client_name[50];
    char client_ip[50];
    int  fd = lws_get_socket_fd(wsi);
    lws_get_peer_addresses(wsi, fd, client_name, 50, client_ip, 50);
    lwsl_warn("got new connection from: %p: %s%s", wsi, client_name, client_ip);
}

void onclose(struct lws *wsi) {
    char client_name[50];
    char client_ip[50];
    int  fd = lws_get_socket_fd(wsi);
    lws_get_peer_addresses(wsi, fd, client_name, 50, client_ip, 50);
    lwsl_warn("connection closed: %p: %s%s", wsi, client_name, client_ip);
}

void onmessage(struct lws *wsi, const void *msg, size_t len, bool is_bin) {
    char rep[1024];
    sprintf(rep, "you sent %ld bytes", len);

    lwsl_err("got %ld, bin: %d", len, is_bin);

    if (!is_bin) {
        char *str = malloc(len + 1);
        memcpy(str, msg, len);
        str[len] = '\0';

        cmd_t *cmd = cmd_from_string(str);
        if (cmd) {
            cmd_show(cmd);

            const char *cmd_s = cmd_to_string(cmd);
            my_ws_send_all(wsi, wsi, cmd_s, strlen(cmd_s), false);

            cmd_destroy(cmd);
        } else {
            error_t *err = get_error();
            if (err) {
                strcpy(rep, err->message);
                destroy_error(err);
            }
        }

        free(str);
    }

    my_ws_send(wsi, rep, strlen(rep), false);
}
