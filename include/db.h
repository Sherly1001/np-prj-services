#ifndef __DB_H__
#define __DB_H__

#include <stdint.h>
#include <libpq-fe.h>

#include <error.h>
#include <snowflake.h>
#include <ws.h> // for bool type

typedef struct {
    uint64_t id;
    char    *username;
    char    *hash_passwd;
    char    *email;
    char    *avatar_url;
} db_user_t;

void db_user_drop(db_user_t *user);

typedef struct db_content_version {
    uint64_t id;
    uint64_t file_id;
    uint64_t update_by;
    char    *content;

    struct db_content_version *prev;
} db_content_version_t;

void db_content_version_drop(db_content_version_t *cnt);

typedef struct {
    uint64_t id;
    uint16_t type_id;
    uint64_t owner;
    uint16_t everyone_can;
    uint64_t current_version;

    db_content_version_t *contents;
} db_file_t;

void db_file_drop(db_file_t *file);

void db_set_id_gen(snowflake_t *snf);

PGresult *db_get_file_types(PGconn *conn);
PGresult *db_get_permissions(PGconn *conn);

db_file_t *db_file_create(PGconn *conn, uint64_t owner, uint16_t everyone_can,
    const char *content, int type_id);
db_file_t *db_file_get(PGconn *conn, uint64_t file_id, bool get_all_history);

bool db_file_save(PGconn *conn, uint64_t file_id, const uint64_t user_id,
    const char *content);

PGresult *db_file_set_per(
    PGconn *conn, uint64_t file_id, uint64_t user_id, int per_id);
PGresult *db_file_get_pers(PGconn *conn, uint64_t file_id);
PGresult *db_file_get_user_per(
    PGconn *conn, uint64_t file_id, uint64_t user_id);

// raise error if failed
db_user_t *db_user_add(PGconn *conn, const char *username, const char *passwd,
    const char *email, const char *avatar_url);
db_user_t *db_user_get(PGconn *conn, uint64_t user_id, const char *username);
db_user_t *db_user_login(
    PGconn *conn, const char *username, const char *passwd);

#endif
