// clang-format off
#ifndef __VEC_H__
#define __VEC_H__

#include <stdlib.h>
#include <string.h>

typedef void (*vec_elm_cpy_t)(void *, const void *);
typedef int (*vec_elm_cmp_t)(const void *, const void *);
typedef void (*vec_elm_drop_t)(void *);

typedef struct {
    size_t elm_size;
    size_t len;
    size_t cap;
    void  *arr;

    vec_elm_cpy_t  elm_cpy;
    vec_elm_cmp_t  elm_cmp;
    vec_elm_drop_t elm_drop;
} vec_t;

vec_t *vec_new(
    size_t          elm_size,
    vec_elm_cpy_t   elm_cpy,
    vec_elm_cmp_t   elm_cmp,
    vec_elm_drop_t  elm_drop
);
int    vec_add(vec_t *vec, void *elm);
int    vec_remove(vec_t *vec, size_t idx);
size_t vec_index_of(vec_t *vec, void *elm);
void  *vec_get(vec_t *vec, size_t idx);
void   vec_drop(vec_t *vec);

#define vec_remove_by(vec, elm) vec_remove(vec, vec_index_of(vec, elm))

#define vec_new_r(type, ...) vec_new(sizeof(type), __VA_ARGS__)

#define vec_get_r(type, vec, idx) (*((type *)vec_get(vec, idx)))

#define vec_add_r(type, vec, elm)                                              \
    ({                                                                         \
        type tmp = elm;                                                        \
        vec_add(vec, &tmp);                                                    \
    })

#define vec_index_of_r(type, vec, elm)                                         \
    ({                                                                         \
        type tmp = elm;                                                        \
        vec_index_of(vec, &tmp);                                               \
    })

#define vec_remove_by_r(type, vec, elm)                                        \
    ({                                                                         \
        type tmp = elm;                                                        \
        vec_remove_by(vec, &tmp);                                              \
    })

#endif
// clang-format on
