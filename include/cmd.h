#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <json-c/json.h>

typedef const char* cmd_type_t;

#define CMD_IS_TYPE_OF(type, of) (strcmp(type, of) == 0)
#define CMD_INSERT "insert, %ld %ld %ld %s"
#define CMD_REMOVE "remove, %ld %ld %ld %ld"
#define CMD_GET    "get, %ld"

#define CMD_ARG_IS_KIND_OF(kind, of) (strcmp(kind, of) == 0)
#define CMD_ARG_INT     "%ld"
#define CMD_ARG_FLOAT   "%f"
#define CMD_ARG_STRING  "%s"
#define CMD_ARG_BOOL    "%u"

typedef struct cmd {
    json_object *_cmd_json_tokener;
    json_object *type;
    json_object *args;
} cmd_t;


// create new cmd from string return NULL if failed
cmd_t *cmd_from_string(const char *str);
const char *cmd_to_string(const cmd_t *cmd);


// create new cmd with type and args return NULL if failed
cmd_t *cmd_new(cmd_type_t type, ...)
    __attribute__ ((format (printf, 1, 2)));
void cmd_destroy(cmd_t *cmd);
void cmd_show(const cmd_t *cmd);


// create new cmd_args with kind and value
json_object *cmd_args_new(const char *fmt, va_list ap);
