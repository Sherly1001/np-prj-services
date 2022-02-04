#ifndef __CMD_H__
#define __CMD_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <json-c/json.h>

#include <bool.h>
#include <error.h>

typedef const char *cmd_type_t;

#define CMD_IS_TYPE_OF(type, of) (strcmp(type, of) == 0)

static const char cmd_types[][50] = {
    "insert, %s %s %ld %s",
    "remove, %s %s %ld %ld",
    "save, %s %s",
    "get, %s %u",
    "get-file-types, ",

    "get-user-pers, ",
    "get-file-pers, %s",
    "set-per, %s %ld",
    "set-user-per, %s %s %ld",
    "get-per-types, ",
};

#define CMD_INSERT         "insert"
#define CMD_REMOVE         "remove"
#define CMD_SAVE           "save"
#define CMD_GET            "get"
#define CMD_GET_FILE_TYPES "get-file-types"

#define CMD_GET_USER_PERS "get-user-pers"
#define CMD_GET_FILE_PERS "get-file-pers"
#define CMD_SET_PER       "set-per"
#define CMD_SET_USER_PER  "set-user-per"
#define CMD_GET_PER_TYPES "get-per-types"

#define CMD_ARG_IS_KIND_OF(kind, of) (strcmp(kind, of) == 0)

#define CMD_ARG_INT    "%ld"
#define CMD_ARG_FLOAT  "%f"
#define CMD_ARG_STRING "%s"
#define CMD_ARG_BOOL   "%u"

typedef struct cmd {
    json_object *_cmd_json_tokener;
    json_object *type;
    json_object *args;
} cmd_t;

// [E]: create new cmd from string return NULL if failed
cmd_t *cmd_from_string(const char *str);
// NO NEED to free() the string after using
const char *cmd_to_string(const cmd_t *cmd);

// [E]: create new cmd with type and args return NULL if failed
cmd_t *cmd_new(cmd_type_t type, ...) __attribute__((format(printf, 1, 2)));
void   cmd_destroy(cmd_t *cmd);
void   cmd_show(const cmd_t *cmd);

// [E]: return true if ok, raise error if failed
bool cmd_validate(const cmd_t *cmd);

// [E]: create new cmd_args with kind and value
json_object *cmd_args_new(const char *fmt, va_list ap);

#endif
