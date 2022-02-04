#ifndef __DB_H__
#define __DB_H__

#include <stdint.h>
#include <libpq-fe.h>

#include <jwt.h>
#include <bool.h>
#include <error.h>
#include <snowflake.h>

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

typedef struct db_user_pers {
    uint64_t user_id;
    uint64_t file_id;
    uint16_t per_id;
    bool     is_owner;

    struct db_user_pers *next;
} db_user_pers_t;

void db_user_pers_drop(db_user_pers_t *pers);

typedef struct {
    uint16_t        everyone_can;
    db_user_pers_t *user_pers;
} db_file_pers_t;

void db_file_pers_drop(db_file_pers_t *pers);

void db_set_id_gen(snowflake_t *snf);

PGresult *db_get_file_types(PGconn *conn);
PGresult *db_get_permissions(PGconn *conn);

// [E]: create file on db
db_file_t *db_file_create(PGconn *conn, uint64_t owner, uint16_t everyone_can,
    const char *content, int type_id);
// [E]: get file from db
db_file_t *db_file_get(PGconn *conn, uint64_t file_id, bool get_all_history);

// [E]: save file to db, return 0 if failed otherwise return new version id
uint64_t db_file_save(PGconn *conn, uint64_t file_id, const uint64_t user_id,
    const char *content);

// [E]: delete file from db
bool db_file_delete(PGconn *conn, uint64_t file_id);
// [E]: set file permissions
bool db_file_set_per(PGconn *conn, uint64_t file_id, int per_id);
// [E]: set file permissions for an user
bool db_file_set_user_per(
    PGconn *conn, uint64_t file_id, uint64_t user_id, int per_id);

db_file_pers_t *db_file_get_pers(PGconn *conn, uint64_t file_id);
db_user_pers_t *db_file_get_user_per(PGconn *conn, uint64_t user_id);

// [E]: create new user
db_user_t *db_user_add(PGconn *conn, const char *username, const char *passwd,
    const char *email, const char *avatar_url);
db_user_t *db_user_get(PGconn *conn, uint64_t user_id, const char *username);
db_user_t *db_user_login(
    PGconn *conn, const char *username, const char *passwd);

#endif
