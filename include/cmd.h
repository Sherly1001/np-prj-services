#include <stdlib.h>
#include <string.h>

typedef const char* cmd_type_t;
typedef const char* cmd_val_kind_t;

#define CMD_IS_TYPE_OF(type, of) (strcmp(type, of) == 0)
#define CMD_INSERT "insert, %d %d %d %s"
#define CMD_REMOVE "remove, %d %d %d %d"
#define CMD_GET    "get, %d"

#define CMD_IS_KIND_OF(kind, of) (strcmp(kind, of) == 0)
#define CMD_VAL_INT     "%ld"
#define CMD_VAL_FLOAT   "%f"
#define CMD_VAL_STRING  "%s"
#define CMD_VAL_BOOL    "%u"

union cmd_val_u {
    long i;
    double f;
    char *s;
    u_int8_t b;
};

typedef struct cmd_val {
    cmd_val_kind_t kind;
    union cmd_val_u val;
} cmd_val_t;

typedef struct cmd_args {
    cmd_val_t *vals;
    size_t len;
} cmd_args_t;

typedef struct cmd {
    char *type;
    cmd_args_t *args;
} cmd_t;


// create new cmd from string return NULL if failed
cmd_t *cmd_from_string(const char *str);
char *cmd_to_string(const cmd_t *cmd);


// create new cmd with type and args return NULL if failed
cmd_t *cmd_new(cmd_type_t type, ...)
    __attribute__ ((format (printf, 1, 2)));
void cmd_destroy(const cmd_t *cmd);
void cmd_show(const cmd_t *cmd);


// creaate new cmd_val with kind and value
cmd_val_t *cmd_val_new(cmd_val_kind_t kind, ...)
    __attribute__ ((format (printf, 1, 2)));
void cmd_val_destroy(cmd_val_t *cmd_val);
