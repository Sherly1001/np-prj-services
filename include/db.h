#ifndef __DB_H__
#define __DB_H__

#include <stdint.h>
#include <libpq-fe.h>

#include <error.h>
#include <snowflake.h>

void db_set_id_gen(snowflake_t *snf);

PGresult *db_get_file_types(PGconn *conn);
PGresult *db_get_permissions(PGconn *conn);

PGresult *db_file_get(PGconn *conn, uint64_t file_id);
PGresult *db_file_save(PGconn *conn, uint64_t file_id, const uint64_t *user_id,
    const char *content);
PGresult *db_file_set_per(
    PGconn *conn, uint64_t file_id, uint64_t user_id, int per_id);
PGresult *db_file_get_pers(PGconn *conn, uint64_t file_id);
PGresult *db_file_get_user_per(
    PGconn *conn, uint64_t file_id, uint64_t user_id);

// raise error if failed
PGresult *db_user_add(PGconn *conn, const char *username, const char *passwd,
    const char *email, const char *avatar_url);
PGresult *db_user_get(PGconn *conn, uint64_t user_id, const char *username);
PGresult *db_user_login(PGconn *conn, const char *username, const char *passwd);

#endif
