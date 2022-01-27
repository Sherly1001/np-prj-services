#include <stdio.h>
#include <signal.h>
#include <libwebsockets.h>

#include <ws.h>
#include <cmd.h>
#include <error.h>
#include <dotenv.h>

void onopen(struct lws *wsi);
void onclose(struct lws *wsi);
void onmessage(struct lws *wsi, const void *msg, size_t len, bool is_bin);
void onrequest(struct lws *wsi, const char *path, const char *body, size_t len);
struct my_ws ws = {onopen, onclose, onmessage};

static struct lws_protocols protocols[] = {
    MY_HTTP_PROTOCOL(onrequest),
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

PGconn *conn;

int main(int argc, const char **argv) {
    pthread_mutex_t snf_mut = PTHREAD_MUTEX_INITIALIZER;
    snowflake_t     snf     = {.worker = 1, .process = 1, .pmutex = &snf_mut};

    load_env();
    db_set_id_gen(&snf);

    conn = PQconnectdb(getenv("DB_URL"));
    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "database connection refused\n");
        exit(1);
    }

    struct lws_context              *context;
    struct lws_context_creation_info info;

    const char *p;
    int         logs = LLL_USER | LLL_ERR | LLL_WARN;

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

void onrequest(
    struct lws *wsi, const char *path, const char *body, size_t len) {

    lwsl_warn("new request: %p: %s, %lu bytes", wsi, path, len);

    body && (*(char *)&body[len] = '\0');

    int   code = 0;
    char *stt, message[2048];
    message[0] = '\0';

    struct json_object *obj   = json_object_new_object();
    struct json_object *jbody = body ? json_tokener_parse(body) : NULL;
    struct json_object *data  = NULL;

    if (body) {
        if (!jbody) {
            code = 422;
            stt  = "error";
            sprintf(message, "the body is not json");
        } else if (strcmp(path, "/users/login") == 0) {
            do {
                struct json_object *username, *passwd;
                json_object_object_get_ex(jbody, "username", &username);
                json_object_object_get_ex(jbody, "passwd", &passwd);

                if (!username ||
                    json_object_get_type(username) != json_type_string) {
                    code = 422;
                    stt  = "error";
                    sprintf(message, "missing username");
                    break;
                }

                if (!passwd ||
                    json_object_get_type(passwd) != json_type_string) {
                    code = 422;
                    stt  = "error";
                    sprintf(message, "missing passwd");
                    break;
                }

                db_user_t *user =
                    db_user_login(conn, json_object_get_string(username),
                        json_object_get_string(passwd));

                if (!user) {
                    code = 401;
                    stt  = "error";
                    sprintf(message, "username and password not matched");
                    break;
                }

                char *token = jwt_encode(user->id, "sher");

                code = 200;
                stt  = "ok";
                data = json_object_new_string(token);

                free(token);
                db_user_drop(user);
            } while (false);
        } else if (strcmp(path, "/users/signin") == 0) {
            do {
                struct json_object *username, *passwd, *email, *avatar_url;
                json_object_object_get_ex(jbody, "username", &username);
                json_object_object_get_ex(jbody, "passwd", &passwd);
                json_object_object_get_ex(jbody, "email", &email);
                json_object_object_get_ex(jbody, "avatar_url", &avatar_url);

                if (!username ||
                    json_object_get_type(username) != json_type_string) {
                    code = 422;
                    stt  = "error";
                    sprintf(message, "missing username");
                    break;
                }

                if (!passwd ||
                    json_object_get_type(passwd) != json_type_string) {
                    code = 422;
                    stt  = "error";
                    sprintf(message, "missing passwd");
                    break;
                }

                if (email && json_object_get_type(email) != json_type_string) {
                    code = 422;
                    stt  = "error";
                    sprintf(message, "email is not string");
                    break;
                }

                if (avatar_url &&
                    json_object_get_type(avatar_url) != json_type_string) {
                    code = 422;
                    stt  = "error";
                    sprintf(message, "avatar_url is not string");
                    break;
                }

                db_user_t *user =
                    db_user_add(conn, json_object_get_string(username),
                        json_object_get_string(passwd),
                        email ? json_object_get_string(email) : NULL,
                        avatar_url ? json_object_get_string(avatar_url) : NULL);

                if (!user) {
                    error_t *err = get_error();

                    code = 422;
                    stt  = "error";
                    sprintf(message, "%s", err->message);
                    destroy_error(err);
                    break;
                }

                char *token = jwt_encode(user->id, "sher");

                code = 200;
                stt  = "ok";
                data = json_object_new_string(token);

                free(token);
                db_user_drop(user);
            } while (false);
        } else {
            code = 404;
            stt  = "error";
            sprintf(message, "resource not found");
        }
    } else {
        if (strcmp(path, "/users/getinfo") == 0) {
            do {
                char auth[1024];
                lws_hdr_copy(wsi, auth, 1023, WSI_TOKEN_HTTP_AUTHORIZATION);

                uint64_t uid;
                if (!jwt_decode(auth, "sher", &uid)) {
                    code = 401;
                    stt  = "error";
                    sprintf(message, "unauthorized");
                    break;
                }

                lwsl_err("%lu\n", uid);
                code = 200;
                stt  = "ok";
                sprintf(message, "%lu", uid);
            } while (false);
        } else {
            code = 404;
            stt  = "error";
            sprintf(message, "resource not found");
        }
    }

    json_object_object_add(obj, "stt", json_object_new_string(stt));
    if (message[0]) {
        json_object_object_add(obj, "message", json_object_new_string(message));
    } else if (data) {
        json_object_object_add(obj, "data", data);
    }
    my_http_send_json(wsi, code, obj);
    json_object_put(obj);
    json_object_put(jbody);
}
