#include "../include/cmd.h"


// create new cmd from string return NULL if failed
cmd_t *cmd_from_string(const char *str) {
    cmd_t *cmd = malloc(sizeof(cmd_t));
    struct json_object *parsed_json = NULL;
    struct json_object *_type = NULL;
    struct json_object *_args = NULL;

    cmd->_cmd_json_tokener = json_tokener_parse(str);
    parsed_json = cmd->_cmd_json_tokener;
    if (!parsed_json) return NULL;

    // 1.get cmd->type
    json_object_object_get_ex(parsed_json, "type", &_type);
    if((!_type) || (json_object_get_type(_type) != json_type_string)) {
        cmd_destroy(cmd);
        return NULL;
    }
    cmd->type = _type;

    // 2.get cmd->args
    json_object_object_get_ex(parsed_json, "args", &_args);
    if((!_args) || (json_object_get_type(_args) != json_type_array)) {
        cmd_destroy(cmd);
        return NULL;
    }
    cmd->args = _args;

    return cmd;
}

const char *cmd_to_string(const cmd_t *cmd) {
    if (!cmd) return NULL;
    if (!cmd->_cmd_json_tokener)return NULL;

    return json_object_to_json_string_ext(cmd->_cmd_json_tokener, JSON_C_TO_STRING_PLAIN);
    // NO NEED to free() the string after using
}


// create new cmd with type and args return NULL if failed
cmd_t *cmd_new(cmd_type_t type, ...) {
    cmd_t *cmd = malloc(sizeof(cmd_t));
    cmd->type = NULL;
    cmd->args = NULL;
    cmd->_cmd_json_tokener = json_object_new_object();
    json_object *tokener = cmd->_cmd_json_tokener;

    // 1. get type
    if (CMD_IS_TYPE_OF(type, CMD_INSERT)) {
        json_object_object_add(tokener, "type", json_object_new_string("insert"));
    } else if (CMD_IS_TYPE_OF(type, CMD_REMOVE)) {
        json_object_object_add(tokener, "type", json_object_new_string("remove"));
    } else if (CMD_IS_TYPE_OF(type, CMD_GET)) {
        json_object_object_add(tokener, "type", json_object_new_string("get"));
    } else {
        cmd_destroy(cmd);
        return NULL;
    }
    json_object_object_get_ex(tokener, "type", &cmd->type);

    // 2. get args 
    json_object *_args = NULL;
    va_list va;

    va_start(va, type);
    _args = cmd_args_new(type, va);
    va_end(va);

    if (!_args) {
        cmd_destroy(cmd);
        return NULL;
    }
    json_object_object_add(tokener,"args", _args);
    json_object_object_get_ex(tokener, "args", &cmd->args);

    return cmd;
}

void cmd_destroy(cmd_t *cmd) {
    json_object_put(cmd->_cmd_json_tokener);
    free(cmd);
    cmd = NULL;
}

void cmd_show(const cmd_t *cmd) {
    if (!cmd) return;

    if (!cmd->type) return;
    printf("%s", json_object_get_string(cmd->type));

    if (!cmd->args) {
        printf("\n");
        return;
    }
    printf(": %s\n", json_object_to_json_string_ext(cmd->args, JSON_C_TO_STRING_SPACED));
}

// create new cmd_args with kind and value
json_object *cmd_args_new(const char *fmt, va_list ap) {
    json_object *_args = json_object_new_array();

    char *_fmt = malloc(strlen(fmt) + 1);
    strcpy(_fmt, fmt);

    char* kind = strtok(_fmt, " ");
    kind = strtok(NULL, " "); // start from the second token

    while (kind) {
        if (CMD_ARG_IS_KIND_OF(kind, CMD_ARG_INT)) {
            json_object_array_add(_args, json_object_new_int64(va_arg(ap, long)));
        } else if (CMD_ARG_IS_KIND_OF(kind, CMD_ARG_STRING)) {
            json_object_array_add(_args, json_object_new_string(va_arg(ap, char *)));
        } else if (CMD_ARG_IS_KIND_OF(kind, CMD_ARG_FLOAT)) {
            json_object_array_add(_args, json_object_new_double(va_arg(ap, double)));
        } else if (CMD_ARG_IS_KIND_OF(kind, CMD_ARG_BOOL)) {
            json_object_array_add(_args, json_object_new_boolean((u_int8_t)va_arg(ap, int)));
        } else {
            free(_fmt);
            json_object_put(_args);
            return NULL;
        }
        kind = strtok(NULL, " ");
    }

    free(_fmt);
    return _args;
}