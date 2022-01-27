#include <error.h>

static error_t *__err_buff = NULL;

void raise_error(int code, const char *message, ...) {
    char buff[2048];

    va_list ap;
    va_start(ap, message);
    vsprintf(buff, message, ap);
    va_end(ap);

    error_t *err = malloc(sizeof(error_t));
    err->code    = code;
    err->message = malloc(strlen(buff) + 1);
    strcpy(err->message, buff);

    err->prev  = __err_buff;
    __err_buff = err;
}

error_t *get_error() {
    error_t *err = __err_buff;

    if (err) {
        __err_buff = err->prev;
    }

    return err;
}

void destroy_error(error_t *err) {
    if (err) {
        free(err->message);
        free(err);
    }
}
