#include <db.h>

static snowflake_t *__snf = NULL;

void db_set_id_gen(snowflake_t *snf) {
    __snf = snf;
}

PGresult *db_exec(PGconn *conn, const char *cmd, int num_params,
    const char **params, ExecStatusType res_type, int err_code,
    const char *func) {
    PGresult *res =
        PQexecParams(conn, cmd, num_params, NULL, params, NULL, NULL, 0);
    if (PQresultStatus(res) != res_type) {
        if (err_code != 0)
            raise_error(err_code, "%s: %s", func, PQresultErrorMessage(res));
        PQclear(res);
        return NULL;
    }

    return res;
}

PGresult *db_get_file_types(PGconn *conn) {
    return PQexec(conn, "select * from types");
}

PGresult *db_get_permissions(PGconn *conn) {
    return PQexec(conn, "select * from permissions");
}

db_file_t *db_file_create(PGconn *conn, uint64_t owner, uint16_t everyone_can,
    const char *content, int type_id) {

    if (!__snf) {
        raise_error(1001, "%s: not found id generator", __func__);
        return NULL;
    }

    uint64_t file_id = snowflake_lock_id(__snf);
    uint64_t ver_id  = snowflake_lock_id(__snf);

    char ids[5][21];
    sprintf(ids[0], "%ld", file_id);
    sprintf(ids[1], "%ld", ver_id);
    sprintf(ids[2], "%d", type_id);
    sprintf(ids[3], "%ld", owner);
    sprintf(ids[4], "%d", everyone_can);

    char cmd[1024 + strlen(content)];

    const char *params[5] = {
        ids[1],
        owner <= 0 ? NULL : ids[3],
        content,
    };

    PGresult *res =
        db_exec(conn, "insert into content_versions values ($1, null, $2, $3)",
            3, params, PGRES_COMMAND_OK, 901, __func__);
    if (!res) return NULL;
    PQclear(res);

    params[0] = ids[0];
    params[1] = ids[2];
    params[2] = owner <= 0 ? NULL : ids[3];
    params[3] = ids[4];
    params[4] = ids[1];

    res = db_exec(conn, "insert into files values ($1, $2, $3, $4, $5)", 5,
        params, PGRES_COMMAND_OK, 902, __func__);
    if (!res) return NULL;
    PQclear(res);

    params[0] = ids[0];
    params[1] = ids[1];

    res =
        db_exec(conn, "update content_versions set file_id = $1 where id = $2",
            2, params, PGRES_COMMAND_OK, 903, __func__);
    if (!res) return NULL;
    PQclear(res);

    db_content_version_t *contents = malloc(sizeof(db_content_version_t));
    contents->content              = malloc(sizeof(content) + 1);
    strcpy(contents->content, content);
    contents->id        = ver_id;
    contents->update_by = owner;
    contents->prev      = NULL;

    db_file_t *file       = malloc(sizeof(db_file_t));
    file->id              = file_id;
    file->owner           = owner;
    file->everyone_can    = everyone_can;
    file->type_id         = type_id;
    file->current_version = ver_id;
    file->contents        = contents;

    return file;
}

db_file_t *db_file_get(PGconn *conn, uint64_t file_id, bool get_all_history) {
    char ids[2][21];
    sprintf(ids[0], "%ld", file_id);
    sprintf(ids[1], "%d", get_all_history ? 1000 : 1);

    const char *params[] = {
        ids[0],
        ids[1],
    };

    PGresult *res = db_exec(conn, "select * from files where id = $1", 1,
        params, PGRES_TUPLES_OK, 904, __func__);
    if (!res) return NULL;

    if (PQntuples(res) != 1) {
        raise_error(905, "%s: file not found", __func__);
        PQclear(res);
        return NULL;
    }

    db_file_t *file       = malloc(sizeof(db_file_t));
    file->id              = atol(PQgetvalue(res, 0, 0));
    file->type_id         = atoi(PQgetvalue(res, 0, 1));
    file->owner           = atol(PQgetvalue(res, 0, 2));
    file->everyone_can    = atoi(PQgetvalue(res, 0, 3));
    file->current_version = atol(PQgetvalue(res, 0, 4));
    file->contents        = NULL;

    params[0] = ids[0];
    params[1] = ids[1];

    PQclear(res);
    res = db_exec(conn,
        "select * from content_versions where file_id = $1 order by id desc "
        "limit $2",
        2, params, PGRES_TUPLES_OK, 905, __func__);
    if (!res) return NULL;

    db_content_version_t **ctns = &file->contents;

    int rows = PQntuples(res);
    for (int i = 0; i < rows; ++i) {
        db_content_version_t *contents = malloc(sizeof(db_content_version_t));

        contents->id        = atol(PQgetvalue(res, i, 0));
        contents->file_id   = atol(PQgetvalue(res, i, 1));
        contents->update_by = atol(PQgetvalue(res, i, 2));
        contents->content   = malloc(strlen(PQgetvalue(res, i, 3)) + 1);
        contents->prev      = NULL;
        strcpy(contents->content, PQgetvalue(res, i, 3));

        *ctns = contents;
        ctns  = &contents->prev;
    }

    return file;
}

bool db_file_save(PGconn *conn, uint64_t file_id, const uint64_t user_id,
    const char *content) {

    if (!__snf) {
        raise_error(1001, "%s: not found id generator", __func__);
        return false;
    }

    uint64_t ver_id = snowflake_lock_id(__snf);

    char ids[3][21];
    sprintf(ids[0], "%ld", ver_id);
    sprintf(ids[1], "%ld", file_id);
    sprintf(ids[2], "%ld", user_id);

    const char *params[] = {
        ids[0],
        ids[1],
        user_id <= 0 ? NULL : ids[2],
        content,
    };

    PGresult *res =
        db_exec(conn, "insert into content_versions values ($1, $2, $3, $4)", 4,
            params, PGRES_COMMAND_OK, 906, __func__);
    if (!res) return false;
    PQclear(res);

    res = db_exec(conn, "update files set current_version = $1 where id = $2",
        2, params, PGRES_COMMAND_OK, 907, __func__);
    if (!res) return false;
    PQclear(res);

    return true;
}

bool db_file_delete(PGconn *conn, uint64_t file_id) {
    char fid[21];
    sprintf(fid, "%ld", file_id);
    const char *params[] = {fid};

    PGresult *res = db_exec(conn, "delete from files where id = $1", 1, params,
        PGRES_COMMAND_OK, 908, __func__);
    if (!res) return false;

    if (atoi(PQcmdTuples(res)) != 1) {
        raise_error(909, "%s: file %ld not exist", __func__, file_id);
        PQclear(res);
        return false;
    }

    PQclear(res);
    return true;
}

PGresult *db_file_set_per(
    PGconn *conn, uint64_t file_id, uint64_t user_id, int per_id);

PGresult *db_file_get_pers(PGconn *conn, uint64_t file_id);

PGresult *db_file_get_user_per(
    PGconn *conn, uint64_t file_id, uint64_t user_id);

db_user_t *db_user_add(PGconn *conn, const char *username, const char *passwd,
    const char *email, const char *avatar_url) {

    if (!__snf) {
        raise_error(1001, "%s: not found id generator", __func__);
        return NULL;
    }

    uint64_t id = snowflake_lock_id(__snf);
    if (!username || !passwd) {
        raise_error(801, "%s: username or password is empty", __func__);
        return NULL;
    }

    char id_s[21];
    sprintf(id_s, "%ld", id);

    const char *params[] = {
        id_s,
        username,
        passwd,
        email,
        avatar_url,
    };

    PGresult *res = db_exec(conn,
        "insert into users values ($1, $2, $3, $4, $5) returning *", 5, params,
        PGRES_TUPLES_OK, 802, __func__);
    if (!res) return NULL;

    db_user_t *user = malloc(sizeof(db_user_t));
    user->id        = id;

    user->username = malloc(sizeof(username) + 1);
    strcpy(user->username, username);

    user->hash_passwd = malloc(sizeof(passwd) + 1);
    strcpy(user->hash_passwd, passwd);

    if (email) {
        user->email = malloc(sizeof(email) + 1);
        strcpy(user->email, email);
    } else {
        user->email = NULL;
    }

    if (avatar_url) {
        user->avatar_url = malloc(sizeof(avatar_url) + 1);
        strcpy(user->avatar_url, avatar_url);
    } else {
        user->avatar_url = NULL;
    }

    return user;
}

db_user_t *db_user_get(PGconn *conn, uint64_t user_id, const char *username) {
    char uid[21];
    sprintf(uid, "%ld", user_id);

    const char *params[] = {
        uid,
        username,
    };

    PGresult *res =
        db_exec(conn, "select * from users where id = $1 or username = $2", 2,
            params, PGRES_TUPLES_OK, 0, NULL);
    if (!res) return NULL;

    if (PQntuples(res) != 1) {
        PQclear(res);
        return NULL;
    }

    db_user_t *user = malloc(sizeof(db_user_t));
    user->id        = atol(PQgetvalue(res, 0, 0));

    user->username = malloc(sizeof(PQgetvalue(res, 0, 1)) + 1);
    strcpy(user->username, PQgetvalue(res, 0, 1));

    if (!PQgetisnull(res, 0, 2)) {
        user->hash_passwd = malloc(sizeof(PQgetvalue(res, 0, 2)) + 1);
        strcpy(user->hash_passwd, PQgetvalue(res, 0, 2));
    } else {
        user->hash_passwd = NULL;
    }

    if (!PQgetisnull(res, 0, 3)) {
        user->email = malloc(sizeof(PQgetvalue(res, 0, 3)) + 1);
        strcpy(user->email, PQgetvalue(res, 0, 3));
    } else {
        user->email = NULL;
    }

    if (!PQgetisnull(res, 0, 4)) {
        user->avatar_url = malloc(sizeof(PQgetvalue(res, 0, 4)) + 1);
        strcpy(user->avatar_url, PQgetvalue(res, 0, 4));
    } else {
        user->avatar_url = NULL;
    }

    PQclear(res);
    return user;
}

db_user_t *db_user_login(
    PGconn *conn, const char *username, const char *passwd) {
    db_user_t *res = db_user_get(conn, -1, username);
    if (!res || strcmp(res->hash_passwd, passwd) != 0) {
        db_user_drop(res);
        return NULL;
    }
    return res;
}

void db_user_drop(db_user_t *user) {
    if (!user) return;
    free(user->username);
    free(user->hash_passwd);
    free(user->email);
    free(user->avatar_url);
    free(user);
}

void db_file_drop(db_file_t *file) {
    if (!file) return;
    db_content_version_drop(file->contents);
    free(file);
}

void db_content_version_drop(db_content_version_t *cv) {
    if (!cv) return;
    db_content_version_drop(cv->prev);
    free(cv->content);
    free(cv);
}
