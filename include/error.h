#ifndef __ERROR_H__
#define __ERROR_H__

#include <stdlib.h>
#include <string.h>

typedef struct error {
    int code;
    char *message;
    struct error *prev;
} error_t;

void raise_error(int code, const char *message);
error_t *get_error();
void destroy_error(error_t *err);

#endif
