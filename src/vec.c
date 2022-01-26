// clang-format off
#include "vec.h"

static void vec_elm_cpy(vec_t *vec, void *dst, const void *src) {
    if (vec->elm_cpy) {
        vec->elm_cpy(dst, src);
    } else {
        memcpy(dst, src, vec->elm_size);
    }
}

static int vec_elm_cmp(vec_t *vec, const void *a, const void *b) {
    if (vec->elm_cmp) {
        return vec->elm_cmp(a, b);
    }

    for (size_t i = 0; i < vec->elm_size; ++i) {
        const char *ca = a + i;
        const char *cb = b + i;
        if (*ca != *cb) return *ca - *cb;
    }

    return 0;
}

vec_t *vec_new(
    size_t          elm_size,
    vec_elm_cpy_t   cpy,
    vec_elm_cmp_t   cmp,
    vec_elm_drop_t  drop
) {

    vec_t *vec = malloc(sizeof(vec_t));

    vec->len      = 0;
    vec->cap      = 0;
    vec->elm_size = elm_size;
    vec->arr      = NULL;
    vec->elm_cpy  = cpy;
    vec->elm_cmp  = cmp;
    vec->elm_drop = drop;

    return vec;
}

int vec_add(vec_t *vec, void *elm) {
    if (!vec) return 0;

    if (vec->len == -2lu) {
        return 0;
    } else if (vec->len <= 0) {
        vec->len = 0;
        vec->cap = 1;
        vec->arr = malloc(vec->elm_size);
    } else if (vec->len >= vec->cap) {
        vec->cap *= 2;
        void *old_arr = vec->arr;
        vec->arr      = malloc(vec->elm_size * vec->cap);
        memcpy(vec->arr, old_arr, vec->elm_size * vec->len);
        free(old_arr);
    }

    void *new_elm = vec->arr + vec->len * vec->elm_size;
    vec_elm_cpy(vec, new_elm, elm);

    vec->len += 1;
    return 1;
}

int vec_remove(vec_t *vec, size_t idx) {
    if (!vec || !vec->arr || vec->len == -1lu || idx == -1lu || vec->len <= idx)
        return 0;

    if (vec->elm_drop) {
        vec->elm_drop(vec_get(vec, idx));
    }

    for (size_t i = idx; i < vec->len - 1; ++i) {
        vec_elm_cpy(vec, vec->arr + i * vec->elm_size,
            vec->arr + (i + 1) * vec->elm_size);
    }

    vec->len -= 1;

    return 1;
}

void *vec_get(vec_t *vec, size_t idx) {
    if (!vec) return NULL;
    if (idx >= vec->len) return NULL;
    return vec->arr + idx * vec->elm_size;
}

size_t vec_index_of(vec_t *vec, void *elm) {
    if (!vec || !vec->arr || vec->len == -1lu) return -1lu;

    void *cur;
    for (size_t i = 0; i < vec->len; ++i) {
        cur = vec->arr + i * vec->elm_size;
        if (!vec_elm_cmp(vec, cur, elm)) return i;
    }

    return -1lu;
}

void vec_drop(vec_t *vec) {
    if (!vec) return;
    if (vec->arr) {
        if (vec->elm_drop) {
            for (size_t i = 0; i < vec->len; ++i) {
                vec->elm_drop(vec_get(vec, i));
            }
        }
        free(vec->arr);
    }
    free(vec);
}
// clang-format on
