#include <db.h>

static snowflake_t *__snf = NULL;

void db_set_id_gen(snowflake_t *snf) {
    __snf = snf;
}

PGresult *db_get_file_types(PGconn *conn) {
    return PQexec(conn, "select * from types");
}

PGresult *db_get_permissions(PGconn *conn) {
    return PQexec(conn, "select * from permissions");
}

PGresult *db_file_get(PGconn *conn, uint64_t file_id) {
    char fid[30];
    sprintf(fid, "%ld", file_id);

    const char *params[1];
    params[0] = fid;

    PGresult *res = PQexecParams(conn, "select * from files where id = $1", 1,
        NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return NULL;
    }

    if (PQntuples(res) != 1) {
        PQclear(res);
        return NULL;
    }

    return res;
}

PGresult *db_file_save(PGconn *conn, uint64_t file_id, const uint64_t *user_id,
    const char *content);

PGresult *db_file_set_per(
    PGconn *conn, uint64_t file_id, uint64_t user_id, int per_id);

PGresult *db_file_get_pers(PGconn *conn, uint64_t file_id);

PGresult *db_file_get_user_per(
    PGconn *conn, uint64_t file_id, uint64_t user_id);

PGresult *db_user_add(PGconn *conn, const char *username, const char *passwd,
    const char *email, const char *avatar_url) {

    if (!__snf) {
        raise_error(801, "%s: not found id generator", __func__);
        return NULL;
    }

    uint64_t id = snowflake_lock_id(__snf);
    if (!username || !passwd) {
        raise_error(802, "%s: username or password is empty", __func__);
        return NULL;
    }

    char cmd[100];

    const char *params[4];
    params[0] = username;
    params[1] = passwd;
    params[2] = email;
    params[3] = avatar_url;

    sprintf(
        cmd, "insert into users values (%ld, $1, $2, $3, $4) returning *", id);

    PGresult *res = PQexecParams(conn, cmd, 4, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        raise_error(802, "%s: %s", __func__, PQresultErrorMessage(res));
        return NULL;
    }

    return res;
}

PGresult *db_user_get(PGconn *conn, uint64_t user_id, const char *username) {
    char uid[30];
    sprintf(uid, "%ld", user_id);

    const char *params[2];
    params[0] = uid;
    params[1] = username;

    PGresult *res =
        PQexecParams(conn, "select * from users where id = $1 or username = $2",
            2, NULL, params, NULL, NULL, 0);

    if (PQresultStatus(res) != PGRES_TUPLES_OK) {
        PQclear(res);
        return NULL;
    }

    if (PQntuples(res) != 1) {
        PQclear(res);
        return NULL;
    }

    return res;
}

PGresult *db_user_login(
    PGconn *conn, const char *username, const char *passwd) {
    PGresult *res = db_user_get(conn, -1, username);

    if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) != 1) {
        PQclear(res);
        return NULL;
    }

    const char *user_passwd = PQgetvalue(res, 0, 2);
    if (!user_passwd || strcmp(user_passwd, passwd) != 0) {
        PQclear(res);
        return NULL;
    }

    return res;
}
