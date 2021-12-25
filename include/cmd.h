#include <stdlib.h>

typedef struct cmd_args {
    char **vals;
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
