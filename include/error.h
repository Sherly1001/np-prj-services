#ifndef __ERROR_H__
#define __ERROR_H__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct error {
    int           code;
    char         *message;
    struct error *prev;
} error_t;

error_t *get_error();

void raise_error(int code, const char *message, ...)
    __attribute__((format(printf, 2, 3)));
void destroy_error(error_t *err);

#endif
