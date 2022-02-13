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

PGconn     *conn       = NULL;
const char *secret_key = NULL;

int main(int argc, const char **argv) {
    pthread_mutex_t snf_mut = PTHREAD_MUTEX_INITIALIZER;
    snowflake_t     snf     = {.worker = 1, .process = 1, .pmutex = &snf_mut};

    load_env();
    db_set_id_gen(&snf);

    secret_key = getenv("SECRET_KEY");

    const char *db_url = getenv("DB_URL");
    if (!db_url) {
        fprintf(stderr, "missing env DB_URL\n");
        exit(1);
    }

    conn = PQconnectdb(db_url);
    if (PQstatus(conn) != CONNECTION_OK) {
        fprintf(stderr, "database connection refused\n");
        exit(1);
    }

    int port = 8080;

    const char *port_s = getenv("PORT");
    if (port_s) {
        port = atoi(port_s);
    }

    port_s = lws_cmdline_option(argc, argv, "-p");
    if (port_s) {
        port = atoi(port_s);
    }

    struct lws_context              *context;
    struct lws_context_creation_info info;

    int logs = LLL_USER | LLL_ERR | LLL_WARN;

    signal(SIGINT, sigint_handler);

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

size_t ws_send_res(struct lws *wsi, struct json_object *res) {
    const char *res_s =
        json_object_to_json_string_ext(res, JSON_C_TO_STRING_PLAIN);
    return my_ws_send(wsi, res_s, strlen(res_s), false);
}

size_t ws_broadcast_res(
    struct lws *wsi, struct lws *except, struct json_object *res) {
    const char *res_s =
        json_object_to_json_string_ext(res, JSON_C_TO_STRING_PLAIN);
    return my_ws_send_all(wsi, except, res_s, strlen(res_s), false);
}

void onopen(struct lws *wsi) {
    struct my_per_session_data *pss = lws_wsi_user(wsi);

    char path[1024], token[1024];
    path[0]  = '\0';
    token[0] = '\0';

    lws_hdr_copy(wsi, path, 1023, WSI_TOKEN_GET_URI);
    lws_get_urlarg_by_name(wsi, "token", token, 1023);

    uint64_t uid = 0;
    if (jwt_decode(token, secret_key, &uid)) {
        pss->user = db_user_get(conn, uid, NULL);
    } else {
        pss->user = NULL;
    }

    lwsl_warn("new connection: %p: user: %s: path: %s", wsi,
        pss->user ? pss->user->username : NULL, path);

    struct json_object *res  = json_object_new_object();
    struct json_object *acpt = json_object_new_object();
    struct json_object *user = NULL;

    if (pss->user) {
        user = json_object_new_object();
        char uid[21];
        sprintf(uid, "%lu", pss->user->id);
        json_object_object_add(user, "id", json_object_new_string(uid));
        json_object_object_add(
            user, "username", json_object_new_string(pss->user->username));
        json_object_object_add(user, "email",
            pss->user->email ? json_object_new_string(pss->user->email) : NULL);
        json_object_object_add(user, "avatar_url",
            pss->user->avatar_url
                ? json_object_new_string(pss->user->avatar_url)
                : NULL);
    }

    char ws_id[21];
    sprintf(ws_id, "%lu", (uint64_t)wsi);
    json_object_object_add(acpt, "ws_id", json_object_new_string(ws_id));
    json_object_object_add(acpt, "user", user);
    json_object_object_add(res, "accept", acpt);

    ws_send_res(wsi, res);
    json_object_put(res);
}

void onclose(struct lws *wsi) {
    struct my_per_session_data *pss = lws_wsi_user(wsi);

    lwsl_warn("connection closed: %p: user: %s", wsi,
        pss->user ? pss->user->username : NULL);

    db_user_drop(pss->user);
}

void onmessage(struct lws *wsi, const void *msg, size_t len, bool is_bin) {
    struct my_per_session_data *pss = lws_wsi_user(wsi);
    struct my_per_vhost_data   *vhd =
        lws_protocol_vh_priv_get(lws_get_vhost(wsi), lws_get_protocol(wsi));

    lwsl_err("got %ld, bin: %d: user: %s", len, is_bin,
        pss->user ? pss->user->username : NULL);

    if (is_bin) return;

    char *msg_s = malloc(len + 1);
    strncpy(msg_s, msg, len);

    struct json_object *res = json_object_new_object();

    cmd_t *cmd = cmd_from_string(msg_s);
    if (!cmd) {
        error_t *err = get_error();
        json_object_object_add(
            res, "error", json_object_new_string(err->message));
        ws_send_res(wsi, res);
        destroy_error(err);
        goto __onmsg_drops;
    }

    cmd_show(cmd);
    const char *type = json_object_get_string(cmd->type);
    if (CMD_IS_TYPE_OF(type, CMD_GET_FILE_TYPES)) {
        struct json_object *arr = json_object_new_array();
        struct json_object *arr_elm;

        PGresult *db_res = db_get_file_types(conn);
        int       rows   = PQntuples(db_res);
        for (int i = 0; i < rows; ++i) {
            arr_elm = json_object_new_array();

            json_object_array_add(
                arr_elm, json_object_new_int(atoi(PQgetvalue(db_res, i, 0))));
            json_object_array_add(
                arr_elm, json_object_new_string(PQgetvalue(db_res, i, 1)));

            json_object_array_add(arr, arr_elm);
        }
        PQclear(db_res);

        json_object_object_add(res, CMD_GET_FILE_TYPES, arr);
        ws_send_res(wsi, res);
    } else if (CMD_IS_TYPE_OF(type, CMD_GET_PER_TYPES)) {
        struct json_object *arr = json_object_new_array();
        struct json_object *arr_elm;

        PGresult *db_res = db_get_permissions(conn);
        int       rows   = PQntuples(db_res);
        for (int i = 0; i < rows; ++i) {
            arr_elm = json_object_new_array();

            json_object_array_add(
                arr_elm, json_object_new_int(atoi(PQgetvalue(db_res, i, 0))));
            json_object_array_add(
                arr_elm, json_object_new_string(PQgetvalue(db_res, i, 1)));

            json_object_array_add(arr, arr_elm);
        }
        PQclear(db_res);

        json_object_object_add(res, CMD_GET_PER_TYPES, arr);
        ws_send_res(wsi, res);
    } else if (CMD_IS_TYPE_OF(type, CMD_GET)) {
        uint64_t file_id = atol(
            json_object_get_string(json_object_array_get_idx(cmd->args, 0)));
        bool get_all =
            json_object_get_boolean(json_object_array_get_idx(cmd->args, 1));

        struct file_info fi = {
            .file = &(db_file_t){.id = file_id},
            .wsis = NULL,
        };

        struct file_info *pfi =
            vec_get(vhd->files, vec_index_of(vhd->files, &fi));

        if (!pfi) {
            fi.file = db_file_get(conn, file_id, false);
            if (!fi.file) {
                error_t *err = get_error();

                struct json_object *res_err = json_object_new_object();
                json_object_object_add(
                    res_err, "error", json_object_new_string(err->message));
                json_object_object_add(res, CMD_GET, res_err);

                ws_send_res(wsi, res);
                destroy_error(err);
                goto __onmsg_drops;
            }

            fi.wsis = vec_new_r(struct lws *, NULL, NULL, NULL);
            vec_add(vhd->files, &fi);
            pfi = vec_get(vhd->files, vec_index_of(vhd->files, &fi));
        }

        vec_add(pfi->wsis, &wsi);
        pss->file = pfi->file;

        db_file_t *file = pfi->file;
        if (get_all) {
            file = db_file_get(conn, file_id, true);
            if (!file) {
                error_t *err = get_error();

                struct json_object *res_err = json_object_new_object();
                json_object_object_add(
                    res_err, "error", json_object_new_string(err->message));
                json_object_object_add(res, CMD_GET, res_err);

                ws_send_res(wsi, res);
                destroy_error(err);
                goto __onmsg_drops;
            }
        }

        char fid[21], vid[21], uid[21];
        sprintf(fid, "%lu", file_id);

        struct json_object *file_res = json_object_new_object();
        struct json_object *contents = json_object_new_array();
        struct json_object *version  = NULL;

        for (db_content_version_t *ver = file->contents; ver; ver = ver->prev) {
            version = json_object_new_object();

            sprintf(vid, "%lu", ver->id);
            sprintf(uid, "%lu", ver->update_by);

            json_object_object_add(
                version, "ver_id", json_object_new_string(vid));
            json_object_object_add(version, "update_by",
                ver->update_by != 0 ? json_object_new_string(uid) : NULL);
            json_object_object_add(
                version, "content", json_object_new_string(ver->content));

            json_object_array_add(contents, version);
        }

        json_object_object_add(
            file_res, "file_id", json_object_new_string(fid));
        json_object_object_add(
            file_res, "everyone_can", json_object_new_int(file->everyone_can));
        json_object_object_add(
            file_res, "file_type", json_object_new_int(file->type_id));
        json_object_object_add(file_res, "contents", contents);

        json_object_object_add(res, CMD_GET, file_res);
        ws_send_res(wsi, res);

        if (get_all) {
            db_file_drop(file);
        }
    } else {
        my_ws_send_all(wsi, wsi, msg_s, strlen(msg_s), false);
    }

__onmsg_drops:
    free(msg_s);
    cmd_destroy(cmd);
    json_object_put(res);
}

void onrequest(
    struct lws *wsi, const char *path, const char *body, size_t len) {

    lwsl_warn("new request: %p: %lu bytes, %s", wsi, len, path);

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

                char *token = jwt_encode(user->id, secret_key);

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

                char *token = jwt_encode(user->id, secret_key);

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
                if (!jwt_decode(auth, secret_key, &uid)) {
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
