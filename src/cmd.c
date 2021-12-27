#include <cmd.h>
#include <stdarg.h>

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
cmd_t *cmd_new(cmd_type_t type, ...) {
    // TODO: cmd must be malloced here
    return NULL;
}

void cmd_destroy(const cmd_t *cmd) {
    // TODO: free cmd and cmd_args here
}

void cmd_show(const cmd_t *cmd) {
    // TODO: show cmd in format: `type: [arg1, arg2, ...]`
}

// creaate new cmd_val with kind and value
cmd_val_t *cmd_val_new(cmd_val_kind_t kind, ...) {
    cmd_val_t *cmd_val = malloc(sizeof(cmd_val_t));
    cmd_val->kind = kind;

    va_list ap;
    va_start(ap, kind);

    char *s = NULL;

    if (CMD_IS_KIND_OF(kind, CMD_VAL_INT)) {
        cmd_val->val.i = va_arg(ap, long);
    } else if (CMD_IS_KIND_OF(kind, CMD_VAL_FLOAT)) {
        cmd_val->val.f = va_arg(ap, double);
    } else if (CMD_IS_KIND_OF(kind, CMD_VAL_STRING)) {
        s = va_arg(ap, char *);
        cmd_val->val.s = malloc(strlen(s) + 1);
        strcpy(cmd_val->val.s, s);
    } else if (CMD_IS_KIND_OF(kind, CMD_VAL_BOOL)) {
        cmd_val->val.b = va_arg(ap, int);
    } else {
        free(cmd_val);
        return NULL;
    }

    va_end(ap);

    return cmd_val;
}

void cmd_val_destroy(cmd_val_t *cmd_val) {
    if (!cmd_val) return;
    if (CMD_IS_KIND_OF(cmd_val->kind, CMD_VAL_STRING)) {
        free(cmd_val->val.s);
    }
    free(cmd_val);
}
