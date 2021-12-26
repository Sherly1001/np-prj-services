#include <stdlib.h>

enum cmd_kind {
    CMD_INT,
    CMD_FLOAT,
    CMD_STRING,
    CMD_BOOL,
};

union cmd_val_u {
    long i;
    double f;
    char *s;
    short b;
};

typedef struct cmd_val {
    enum cmd_kind kind;
    union cmd_val_u val;
} cmd_val_t;

typedef struct cmd_args {
    cmd_val_t *vals;
    size_t len;
} cmd_args_t;

typedef struct cmd {
    char *cmd;
    cmd_args_t *args;
} cmd_t;


// create new cmd from string return NULL if failed
cmd_t *cmd_from_string(const char *str);
char *cmd_to_string(const cmd_t *cmd);


// create new cmd with type and args return NULL if failed
cmd_t *cmd_new(const char *type, ...);
void cmd_destroy(const cmd_t *cmd);
void cmd_show(const cmd_t *cmd);


// creaate new cmd_val with kind and value
cmd_val_t *cmd_val_new(enum cmd_kind kind, ...);
void cmd_val_destroy(cmd_val_t *cmd_val);
