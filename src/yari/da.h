#ifndef YR_DA_H
#define YR_DA_H

#include <string.h>

#ifndef YR_DA_REALLOC
#include <stdlib.h>
#define YR_DA_REALLOC realloc
#endif

#ifndef YR_DA_FREE
#include <stdlib.h>
#define YR_DA_FREE free
#endif

#ifndef YR_DA_ASSERT
#include <assert.h>
#define YR_DA_ASSERT assert
#endif

#define YR_ARRAY_LEN(array) (sizeof(array) / sizeof(array[0]))

/**
 * Dynamic array implementation
 * The dynamic array struct should be defined by the user as follows:
 * typedef struct {
 *     Type *data;
 *     size_t length;
 *     size_t capacity;
 *     ... // any additional fields
 * } DynamicArray;
 */

#ifndef YR_DA_INIT_CAPACITY
#define YR_DA_INIT_CAPACITY 4
#endif
#ifndef YR_DA_GROWTH_FACTOR
#define YR_DA_GROWTH_FACTOR 1.5
#endif
#ifndef YR_DA_SHRINK_FACTOR
#define YR_DA_SHRINK_FACTOR 2
#endif

#define yr_da_reserve(da, expected_capacity)                                              \
    do {                                                                                  \
        if ((size_t)(expected_capacity) > (da)->capacity) {                               \
            if ((da)->capacity == 0) {                                                    \
                if ((size_t)(expected_capacity) > YR_DA_INIT_CAPACITY)                    \
                    (da)->capacity = (size_t)(expected_capacity);                         \
                else                                                                      \
                    (da)->capacity = YR_DA_INIT_CAPACITY;                                 \
            } else {                                                                      \
                if ((size_t)(expected_capacity) > (da)->capacity * YR_DA_GROWTH_FACTOR)   \
                    (da)->capacity = (size_t)(expected_capacity);                         \
                else                                                                      \
                    (da)->capacity *= YR_DA_GROWTH_FACTOR;                                \
            }                                                                             \
            (da)->data = YR_DA_REALLOC((da)->data, (da)->capacity * sizeof(*(da)->data)); \
            YR_DA_ASSERT((da)->data != NULL);                                             \
        }                                                                                 \
    } while (0)

#define yr_da_append(da, item)                 \
    do {                                       \
        yr_da_reserve((da), (da)->length + 1); \
        (da)->data[(da)->length++] = (item);   \
    } while (0)

/* shrink the backing storage (in reverse of yr_da_reserve) once it gets
 * underfilled, never going below the initial capacity nor below what's
 * needed to hold the current length plus one spare slot */
#define yr_da_should_shrink(da) \
    ((da)->capacity > YR_DA_INIT_CAPACITY && (da)->length <= (da)->capacity / YR_DA_SHRINK_FACTOR)

#define yr_da_shrink(da)                                                              \
    do {                                                                              \
        size_t _new_cap = (size_t)((da)->capacity / YR_DA_GROWTH_FACTOR);             \
        if (_new_cap < YR_DA_INIT_CAPACITY) _new_cap = YR_DA_INIT_CAPACITY;           \
        if (_new_cap <= (da)->length) _new_cap = (da)->length + 1;                    \
        (da)->capacity = _new_cap;                                                    \
        (da)->data = YR_DA_REALLOC((da)->data, (da)->capacity * sizeof(*(da)->data)); \
        YR_DA_ASSERT((da)->data != NULL);                                             \
    } while (0)

#define yr_da_remove_unordered(da, idx)                   \
    do {                                                  \
        (da)->data[(idx)] = (da)->data[(da)->length - 1]; \
        (da)->length--;                                   \
        if (yr_da_should_shrink(da)) yr_da_shrink(da);    \
    } while (0)

#define yr_da_remove(da, idx, del)                                             \
    do {                                                                       \
        if ((da)->data && (idx) < (da)->length) {                              \
            memmove(&(da)->data[(idx)], &(da)->data[(idx) + (del)],            \
                    ((da)->length - (idx) - (del)) * sizeof(*(da)->data));     \
            (da)->length = (da)->length > (del) ? (da)->length - (del) : 0;    \
            memset(&(da)->data[(da)->length], 0, (del) * sizeof(*(da)->data)); \
            if (yr_da_should_shrink(da)) yr_da_shrink(da);                     \
        }                                                                      \
    } while (0)

#define yr_da_pop(da) ((da)->length > 0 ? &(da)->data[--(da)->length] : NULL)

#define yr_da_free(da)          \
    do {                        \
        YR_DA_FREE((da)->data); \
        (da)->data = NULL;      \
        (da)->length = 0;       \
        (da)->capacity = 0;     \
    } while (0)

#define yr_foreach(da, var) \
    for (__typeof__((da)->data) var = (da)->data; var < (da)->data + (da)->length; var++)

#define yr_foreach_idx(da, idx) \
    for (size_t idx = 0; idx < (da)->length; idx++)

#ifdef YARI_NO_PREFIX
#define da_reserve yr_da_reserve
#define da_append yr_da_append
#define da_remove_unordered yr_da_remove_unordered
#define da_pop yr_da_pop
#define da_free yr_da_free
#define foreach yr_foreach
#define foreach_idx yr_foreach_idx
#define ARRAY_LEN YR_ARRAY_LEN
#endif
#endif // YR_DA_H
