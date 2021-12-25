#include <cmd.h>

// create new cmd from string return NULL if failed
cmd_t *cmd_from_string(const char *str) {
    // TODO: cmd must be malloced here
    return NULL;
}

char *cmd_to_string(const cmd_t *cmd) {
    // TODO: return string must be malloced here
    return  NULL;
}


// create new cmd with type and args return NULL if failed
cmd_t *cmd_new(const char *type, ...) {
    // TODO: cmd must be malloced here
    return NULL;
}

void cmd_destroy(const cmd_t *cmd) {
    // TODO: free cmd and cmd_args here
}

void cmd_show(const cmd_t *cmd) {
    // TODO: show cmd in format: `type: [arg1, arg2, ...]`
}
