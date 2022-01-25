// clang-format off

#include "vec.h"

vec_t *vec_new(
    size_t elm_size,
    vec_elm_copy cpy,
    vec_elm_cmp cmp,
    vec_elm_free free
) {
    vec_t *vec = malloc(sizeof(vec_t));

    vec->len = 0;
    vec->cap = 0;
    vec->elm_size = elm_size;
    vec->arr = NULL;
    vec->cpy = cpy;
    vec->cmp = cmp;
    vec->free = free;

    return vec;
}

int vec_add(vec_t *vec, void *elm) {
    if (!vec) return 0;

    if (vec->len <= 0) {
        vec->len = 0;
        vec->cap = 1;
        vec->arr = malloc(vec->elm_size);
    } else if (vec->len >= vec->cap) {
        vec->cap *= 2;
        void *old_arr = vec->arr;
        vec->arr = malloc(vec->elm_size * vec->cap);
        memcpy(vec->arr, old_arr, vec->elm_size * vec->len);
        free(old_arr);
    }

    void *new_elm = vec->arr + vec->len * vec->elm_size;
    if (vec->cpy) {
        vec->cpy(new_elm, elm);
    } else {
        memcpy(new_elm, elm, vec->elm_size);
    }

    vec->len += 1;
    return 1;
}

int vec_remove(vec_t *vec, size_t idx) {
    if (!vec || !vec->arr || vec->len <= 0 || vec->len <= idx || idx < 0)
        return 0;

    for (size_t i = idx; i < vec->len - 1; ++i) {
        if (vec->cpy) {
            vec->cpy(
                &vec->arr[i * vec->elm_size],
                &vec->arr[(i + 1) * vec->elm_size]
            );
        } else {
            memcpy(
                &vec->arr[i * vec->elm_size],
                &vec->arr[(i + 1) * vec->elm_size],
                vec->elm_size
            );
        }
    }

    vec->len -= 1;

    return 1;
}

void *vec_get(vec_t *vec, size_t idx) {
    if (!vec) return NULL;
    if (idx >= vec->len) return NULL;
    return vec->arr + idx * vec->elm_size;
}

int raw_cmp(void *a, void *b, size_t size) {
    for (size_t i = 0; i < size; ++i) {
        char *ca = &a[i];
        char *cb = &b[i];
        if (*ca != *cb) return *ca - *cb;
    }

    return 0;
}

size_t vec_index_of(vec_t *vec, void *elm) {
    if (!vec || !vec->arr || vec->len <= 0) return -1;

    for (size_t i = 0; i < vec->len; ++i) {
        void *cur = vec->arr + i * vec->elm_size;
        if (vec->cmp) {
            if (!vec->cmp(cur, elm)) return i;
        } else {
            if (!raw_cmp(cur, elm, vec->elm_size)) return i;
        }
    }

    return -1;
}

void vec_destroy(vec_t *vec) {
    if (!vec) return;
    if (vec->arr) {
        if (vec->free) {
            for (size_t i = 0; i < vec->len; ++i) {
                void *cur = vec->arr + i * vec->elm_size;
                vec->free(cur);
            }
        }
        free(vec->arr);
    }
    free(vec);
}

// clang-format on
