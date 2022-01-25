#ifndef __VEC_H__
#define __VEC_H__

// clang-format off

#include <stdlib.h>
#include <string.h>

typedef void (*vec_elm_copy)(void*, void*);
typedef int (*vec_elm_cmp)(void*, void*);
typedef void (*vec_elm_free)(void*);

typedef struct {
    size_t elm_size;
    size_t len;
    size_t cap;
    void *arr;
    vec_elm_copy cpy;
    vec_elm_cmp cmp;
    vec_elm_free free;
} vec_t;

vec_t *vec_new(
    size_t elm_size,
    vec_elm_copy cpy,
    vec_elm_cmp cmp,
    vec_elm_free free
);
int vec_add(vec_t *vec, void *elm);
size_t vec_index_of(vec_t *vec, void *elm);
int vec_remove(vec_t *vec, size_t idx);
void *vec_get(vec_t *vec, size_t idx);
void vec_destroy(vec_t *vec);

#define vec_new_type(type, ...) vec_new(sizeof(type), __VA_ARGS__)
#define vec_get_type(type, vec, idx) (*((type*)vec_get(vec, idx)))

#define vec_add_type(type, vec, elm) ({ \
    type tmp = elm; \
    vec_add(vec, &tmp); \
})

#define vec_index_of_type(type, vec, elm) ({ \
    type tmp = elm; \
    vec_index_of(vec, &tmp); \
})

// clang-format on

#endif
