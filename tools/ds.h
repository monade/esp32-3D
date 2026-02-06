/**
 * ds - A simple and lightweight stb-style data structures and utilities library in C.
 * https://github.com/mceck/c-stb
 *
 * - Dynamic arrays
 * - String builder
 * - Linked lists
 * - Hash maps
 * - Logging
 * - File utilities
 *
 * #define DS_NO_PREFIX to disable the `ds_` prefix for all functions and types.
 */
#ifndef DS_H_
#define DS_H_
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <errno.h>
#include <stdarg.h>
#include <assert.h>
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include "windows.h"
#include <io.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#endif
#ifndef DS_ALLOC
#define DS_ALLOC malloc
#endif // DS_ALLOC
#ifndef DS_REALLOC
#define DS_REALLOC realloc
#endif // DS_REALLOC
#ifndef DS_FREE
#define DS_FREE free
#endif // DS_FREE

typedef enum {
    DS_LOG_DEBUG,
    DS_LOG_INFO,
    DS_LOG_WARN,
    DS_LOG_ERROR
} DsLogLevel;

extern void (*ds_log_handler)(int level, const char *fmt, ...);
void ds_simple_log_handler(int level, const char *fmt, ...);
void ds_color_log_handler(int level, const char *fmt, ...);
void ds_set_log_handler(void (*handler)(int level, const char *fmt, ...));
void ds_set_log_level(int level);

#define ds_log(lvl, fmt, ...) ds_log_handler((lvl), (fmt)__VA_OPT__(, ) __VA_ARGS__)
#define ds_log_info(FMT, ...) ds_log(DS_LOG_INFO, FMT, __VA_ARGS__)
#define ds_log_debug(FMT, ...) ds_log(DS_LOG_DEBUG, FMT, __VA_ARGS__)
#define ds_log_warn(FMT, ...) ds_log(DS_LOG_WARN, FMT, __VA_ARGS__)
#define ds_log_error(FMT, ...) ds_log(DS_LOG_ERROR, FMT, __VA_ARGS__)

#define DS_TODO(msg)                                                                         \
    do {                                                                                     \
        ds_log(DS_LOG_WARN, "TODO: %s\nat %s::%s::%d\n", msg, __FILE__, __func__, __LINE__); \
        abort();                                                                             \
    } while (0)
#define DS_UNREACHABLE()                                                                      \
    do {                                                                                      \
        ds_log(DS_LOG_ERROR, "UNREACHABLE CODE: %s::%s::%d\n", __FILE__, __func__, __LINE__); \
        abort();                                                                              \
    } while (0)
#define DS_UNUSED(x) (void)(x)

#define DS_ARRAY_LEN(arr) (sizeof(arr) / sizeof((arr)[0]))

#ifndef DS_DA_INIT_CAPACITY
/**
 * Initial capacity for dynamic arrays.
 */
#define DS_DA_INIT_CAPACITY 32
#endif // DS_DA_INIT_CAPACITY

/**
 * Dynamic array declaration
 * It will create a dynamic array of the specified type.
 * Example:
 *   `ds_da_declare(my_array, int);`
 *
 * This will create a dynamic array of integers like this:
```c
typedef struct {
    int *data;
    size_t length;
    size_t capacity;
} my_array;
```
 */
#define ds_da_declare(name, type) \
    typedef struct {              \
        type *data;               \
        size_t length;            \
        size_t capacity;          \
    } name

/**
 * Reserve space in a dynamic array.
 */
#define ds_da_reserve_with_init_capacity(da, expected_capacity, min_capacity)          \
    do {                                                                               \
        if ((size_t)(expected_capacity) > (da)->capacity) {                            \
            if ((da)->capacity == 0) {                                                 \
                if ((size_t)(expected_capacity) > (min_capacity))                      \
                    (da)->capacity = (size_t)(expected_capacity);                      \
                else                                                                   \
                    (da)->capacity = (min_capacity);                                   \
            } else {                                                                   \
                if ((size_t)(expected_capacity) > (da)->capacity * 2)                  \
                    (da)->capacity = (size_t)(expected_capacity);                      \
                else                                                                   \
                    (da)->capacity *= 2;                                               \
            }                                                                          \
            (da)->data = DS_REALLOC((da)->data, (da)->capacity * sizeof(*(da)->data)); \
            assert((da)->data != NULL);                                                \
        }                                                                              \
    } while (0)

#define ds_da_reserve_min(da, expected_capacity) ds_da_reserve_with_init_capacity((da), (expected_capacity), 1)
#define ds_da_reserve(da, expected_capacity) ds_da_reserve_with_init_capacity((da), (expected_capacity), DS_DA_INIT_CAPACITY)

/**
 * Append an item to a dynamic array.
 * Example:
 ```c
 ds_da_declare(my_array, int);
 ...
    my_array a = {0};
    ds_da_append(&a, 42);
 ```
 */
#define ds_da_append(da, item)                 \
    do {                                       \
        ds_da_reserve((da), (da)->length + 1); \
        (da)->data[(da)->length++] = (item);   \
    } while (0)

/**
 * Append multiple items to a dynamic array.
 * It will copy the items from the source array to the destination array.
 * Example:
 ```c
 ds_da_declare(my_array, int);
 ...
    my_array a = {0};
    ds_da_append_many(&a, (int[]){1, 2, 3}, 3);
 ```
 */
#define ds_da_append_many(da, items, size)          \
    do {                                            \
        if ((size) == 0) break;                     \
        ds_da_reserve((da), (da)->length + (size)); \
        memcpy(&(da)->data[(da)->length], (items),  \
               (size) * sizeof(*(da)->data));       \
        (da)->length += (size);                     \
    } while (0)

/**
 * Pop an item from a dynamic array.
 */
#define ds_da_pop(da)               \
    ({                              \
        assert((da)->data != NULL); \
        (da)->data[--(da)->length]; \
    })

/**
 * Remove a range of data from a dynamic array.
 * Example:
 *   `ds_da_remove(&a, 1, 2); // [1,2,3,4,5] -> [1,4,5]`
 */
#define ds_da_remove(da, idx, del)                                             \
    do {                                                                       \
        if ((da)->data && (idx) < (da)->length) {                              \
            memmove(&(da)->data[(idx)], &(da)->data[(idx) + (del)],            \
                    ((da)->length - (idx) - (del)) * sizeof(*(da)->data));     \
            (da)->length = (da)->length > (del) ? (da)->length - (del) : 0;    \
            memset(&(da)->data[(da)->length], 0, (del) * sizeof(*(da)->data)); \
        }                                                                      \
    } while (0)

/**
 * Insert an item at a specific index in a dynamic array.
 * Example:
 *   `ds_da_insert(&a, 1, 42); // [1,2,3] -> [1,42,2,3]`
 */
#define ds_da_insert(da, idx, item)                            \
    do {                                                       \
        if ((idx) > (da)->length) (idx) = (da)->length;        \
        ds_da_reserve((da), (da)->length + 1);                 \
        memmove(&(da)->data[(idx) + 1], &(da)->data[(idx)],    \
                ((da)->length - (idx)) * sizeof(*(da)->data)); \
        (da)->data[(idx)] = (item);                            \
        (da)->length++;                                        \
    } while (0)

/**
 * Prepend an item to a dynamic array.
 * Example:
 ```c
 ds_da_declare(my_array, int);
 ...
    my_array a = {0};
    ds_da_prepend(&a, 42);
 ```
 */
#define ds_da_prepend(da, item) ds_da_insert((da), 0, (item))
/**
 * Get a pointer to the last item of a dynamic array, or NULL if the array is empty.
 */
#define ds_da_last(da) ((da)->length > 0 ? &(da)->data[(da)->length - 1] : NULL)

/**
 * Get a pointer to the first item of a dynamic array, or NULL if the array is empty.
 */
#define ds_da_first(da) ((da)->length > 0 ? &(da)->data[0] : NULL)

/**
 * Reset a dynamic array. It will not free the underlying memory.
 */
#define ds_da_zero(da)      \
    do {                    \
        (da)->length = 0;   \
        (da)->capacity = 0; \
        (da)->data = NULL;  \
    } while (0)

/**
 * Free a dynamic array.
 */
#define ds_da_free(da)                       \
    do {                                     \
        if ((da)->data) DS_FREE((da)->data); \
        ds_da_zero(da);                      \
    } while (0)

/**
 * Iterate over a dynamic array.
 * It will define `var` pointer to the current item.
 * Example:
 *   `ds_da_foreach(&a, item) { printf("%d\n", *item); }`
 */
#define ds_da_foreach(da, var) \
    for (__typeof__((da)->data) var = (da)->data; var < (da)->data + (da)->length; var++)

#define ds_da_foreach_idx(da, idx) \
    for (size_t idx = 0; idx < (da)->length; idx++)

/**
 * Find an item in a dynamic array. It returns a pointer to the item, or NULL if not found.
 * Example:
 *   `int *x = ds_da_find(&a, e == 0);`
 */
#define ds_da_find(da, expr)                              \
    ({                                                    \
        __typeof__(*(da)->data) *_r = NULL;               \
        for (size_t _i = 0; _i < (da)->length; _i++) {    \
            __typeof__(*(da)->data) *e = &(da)->data[_i]; \
            if (expr) {                                   \
                _r = &(da)->data[_i];                     \
                break;                                    \
            }                                             \
        }                                                 \
        _r;                                               \
    })

/**
 * Get the index of an item in a dynamic array.
 * It returns the index of the item, or -1 if not found.
 */
#define ds_da_index_of(da, expr)                          \
    ({                                                    \
        int _r = -1;                                      \
        for (size_t _i = 0; _i < (da)->length; _i++) {    \
            __typeof__(*(da)->data) *e = &(da)->data[_i]; \
            if (expr) {                                   \
                _r = (int)_i;                             \
                break;                                    \
            }                                             \
        }                                                 \
        _r;                                               \
    })

/**
 * Dynamic string.
 */
ds_da_declare(DsString, char);

void _ds_str_append(DsString *str, ...);

/**
 * Append formatted string to a dynamic string builder.
 * Example:
 *   `ds_str_appendf(&str, "Hello, %s!", "World");`
 */
void ds_str_appendf(DsString *str, const char *fmt, ...);

/**
 * Prepend formatted string to a dynamic string builder.
 * Example:
 *   `ds_str_prependf(&str, "Hello, %s!", "World");`
 */
void ds_str_prependf(DsString *str, const char *fmt, ...);

/**
 * Append strings to a dynamic string builder.
 * Example:
 *   `ds_str_append(&str, "Hello, ", "World", ...);`
 */
#define ds_str_append(str, ...) _ds_str_append((str)__VA_OPT__(, ) __VA_ARGS__, NULL)

/**
 * Insert a string at a specific index in the string builder.
 */
void ds_str_insert(DsString *dstr, const char *str, size_t index);

/**
 * Prepend a string to the string builder.
 */
#define ds_str_prepend(dstr, str) ds_str_insert((dstr), (str), 0)

/**
 * Check if a substring is included in the string builder.
 */
bool ds_str_include(const DsString *str, const char *substr);

/**
 * Trim leading whitespace from the string builder.
 */
DsString *ds_str_ltrim(DsString *str);

/**
 * Trim trailing whitespace from the string builder.
 */
DsString *ds_str_rtrim(DsString *str);

/**
 * Trim whitespace from the string builder.
 */
#define ds_str_trim(str) (ds_str_rtrim(ds_str_ltrim(str)))

#ifndef DS_HM_LOAD_FACTOR
/**
 * Load factor for hash maps and sets.
 */
#define DS_HM_LOAD_FACTOR 0.75f
#endif

#ifndef DS_HM_INIT_CAPACITY
/**
 * Initial capacity for hash maps and sets.
 */
#define DS_HM_INIT_CAPACITY 32
#endif

size_t _ds_hash_pointer(const void *key);

size_t _ds_hash_int(int key);

size_t _ds_hash_long(long key);

size_t _ds_hash_float(double key);

size_t _ds_hash_string(const char *key);

int _ds_eq_int(int a, int b);

int _ds_eq_long(long a, long b);

int _ds_eq_float(double a, double b);

int _ds_eq_str(const char *a, const char *b);

int _ds_eq_ptr(const void *a, const void *b);

#define _ds_eq_fn(a, b) _Generic((a), \
    char *: _ds_eq_str,               \
    const char *: _ds_eq_str,         \
    int: _ds_eq_int,                  \
    char: _ds_eq_int,                 \
    uint8_t: _ds_eq_int,              \
    uint16_t: _ds_eq_int,             \
    uint32_t: _ds_eq_int,             \
    uint64_t: _ds_eq_long,            \
    size_t: _ds_eq_long,              \
    long: _ds_eq_long,                \
    float: _ds_eq_float,              \
    double: _ds_eq_float,             \
    default: _ds_eq_ptr)(a, b)

#define _ds_hash_fn(key) _Generic((key), \
    char *: _ds_hash_string,             \
    const char *: _ds_hash_string,       \
    int: _ds_hash_int,                   \
    char: _ds_hash_int,                  \
    uint8_t: _ds_hash_int,               \
    uint16_t: _ds_hash_int,              \
    uint32_t: _ds_hash_int,              \
    uint64_t: _ds_hash_long,             \
    size_t: _ds_hash_long,               \
    long: _ds_hash_long,                 \
    float: _ds_hash_float,               \
    double: _ds_hash_float,              \
    default: _ds_hash_pointer)(key)

#define _ds_hm_hfn(hm, key_v) ((hm)->hfn ? (hm)->hfn(key_v) : _ds_hash_fn(key_v))
#define _ds_hm_eqfn(hm, a, b) ((hm)->eqfn ? (hm)->eqfn(a, b) : _ds_eq_fn(a, b))

/**
 * Declare a hash map.
 * Example:
 *   `ds_hm_declare(my_map, int, const char *);`
 *
```c
This will create a hash map of int keys and const char values like this:
// Key-Value pair structure
typedef struct {
    int key;
    const char *value;
} my_map_Kv;
// Dynamic array of key-value pairs
typedef struct {
    my_map_Kv *data;
    size_t length;
    size_t capacity;
} my_map_Da;
// Hash map structure
typedef struct {
    my_map_Da *table;
    size_t (*hfn)(int);
    int (*eqfn)(int, int);
    size_t size;
} my_map;
```
 * You can customize the hash function and equality function for your specific key type.
 * `my_map hm = {.hfn = my_hash_function, .eqfn = my_eq_function};`
 * The hash function should follow the signature `size_t hash_function(key_t key);`
 * The equality function should follow the signature `int eq_function(key_t a, key_t b);`
 */
#define ds_hm_declare(name, key_t, val_t)   \
    typedef struct {                        \
        key_t key;                          \
        val_t value;                        \
    } name##_Kv;                            \
    ds_da_declare(name##_Da, name##_Kv);    \
    ds_da_declare(name##_Table, name##_Da); \
    typedef struct {                        \
        name##_Table table;                 \
        size_t (*hfn)(key_t);               \
        int (*eqfn)(key_t, key_t);          \
        size_t length;                      \
    } name

#define _ds_hm_resize(hm)                                                                             \
    do {                                                                                              \
        size_t new_capacity = (hm)->table.length == 0 ? DS_HM_INIT_CAPACITY : (hm)->table.length * 2; \
        __typeof__((hm)->table) new_table = {0};                                                      \
        ds_da_reserve(&new_table, new_capacity);                                                      \
        new_capacity = new_table.length = new_table.capacity;                                         \
        memset(new_table.data, 0, new_capacity * sizeof(*new_table.data));                            \
        for (size_t _i = 0; _i < (hm)->table.length; _i++) {                                          \
            for (size_t _j = 0; _j < (hm)->table.data[_i].length; _j++) {                             \
                __typeof__((hm)->table.data[_i].data[_j]) kv = (hm)->table.data[_i].data[_j];         \
                size_t _h = _ds_hm_hfn((hm), kv.key) % new_capacity;                                  \
                if (new_table.data[_h].capacity == 0) ds_da_reserve_min(&new_table.data[_h], 1);      \
                ds_da_append(&new_table.data[_h], kv);                                                \
            }                                                                                         \
        }                                                                                             \
        ds_da_free(&(hm)->table);                                                                     \
        (hm)->table = new_table;                                                                      \
    } while (0)

/**
 * Set a key-value pair in the hash map.
 * Example:
```c
ds_hm_declare(my_map, int, const char *);
...
    my_map hm = {0};
    ds_hm_set(&hm, 42, "Hello");
```
 */
#define ds_hm_set(hm, key_v, val_v)                                                              \
    do {                                                                                         \
        if ((hm)->table.length == 0 || (hm)->length >= (hm)->table.length * DS_HM_LOAD_FACTOR) { \
            _ds_hm_resize(hm);                                                                   \
        }                                                                                        \
        __typeof__(*(hm)->table.data[0].data) kv = {.key = (key_v), .value = (val_v)};           \
        size_t _hash = _ds_hm_hfn((hm), (kv).key) % (hm)->table.length;                          \
        size_t _i;                                                                               \
        for (_i = 0; _i < (hm)->table.data[_hash].length; _i++) {                                \
            if (_ds_hm_eqfn((hm), (hm)->table.data[_hash].data[_i].key, (kv).key)) {             \
                (hm)->table.data[_hash].data[_i].value = (kv).value;                             \
                break;                                                                           \
            }                                                                                    \
        }                                                                                        \
        if (_i == (hm)->table.data[_hash].length) {                                              \
            ds_da_append(&(hm)->table.data[_hash], kv);                                          \
            (hm)->length++;                                                                      \
        }                                                                                        \
    } while (0)

/**
 * Try to get a value from the hash map.
 * Returns NULL if the key is not found else it returns a pointer to the value.
 * Example:
 ```c
 ds_hm_declare(my_map, int, const char *);
 ...
    const char **value = ds_hm_try(&hm, 42);
    printf("%s\n", *value);
 ```
 */
#define ds_hm_try(hm, key_v)                                                            \
    ({                                                                                  \
        __typeof__(&(hm)->table.data[0].data[0].value) _val = NULL;                     \
        if ((hm)->table.length) {                                                       \
            size_t _hash = _ds_hm_hfn((hm), (key_v)) % (hm)->table.length;              \
            for (size_t _i = 0; _i < (hm)->table.data[_hash].length; _i++) {            \
                if (_ds_hm_eqfn((hm), (hm)->table.data[_hash].data[_i].key, (key_v))) { \
                    _val = &(hm)->table.data[_hash].data[_i].value;                     \
                    break;                                                              \
                }                                                                       \
            }                                                                           \
        }                                                                               \
        _val;                                                                           \
    })

#define ds_hm_has(hm, key_v) (ds_hm_try((hm), (key_v)) != NULL)

/**
 * Get a value from the hash map. It will return the value associated with the key
 * The result will be {0} if the key is not found
 * You should use ds_hm_try if you are not really sure the key is present
 * Example:
 ```c
 const char *value = ds_hm_get(&hm, 42);
 ```
 */
#define ds_hm_get(hm, key_v)                                                        \
    ({                                                                              \
        __typeof__((hm)->table.data[0].data[0].value) _val = {0};                   \
        size_t _hash = _ds_hm_hfn((hm), (key_v)) % (hm)->table.length;              \
        for (size_t _i = 0; _i < (hm)->table.data[_hash].length; _i++) {            \
            if (_ds_hm_eqfn((hm), (hm)->table.data[_hash].data[_i].key, (key_v))) { \
                _val = (hm)->table.data[_hash].data[_i].value;                      \
                break;                                                              \
            }                                                                       \
        }                                                                           \
        _val;                                                                       \
    })

/**
 * Remove a value from the hash map and return a pointer to it, or NULL if not found.
 * Example:
 ```c
 const char **value = ds_hm_remove(&hm, 42);
 ```
 */
#define ds_hm_remove(hm, key_v)                                                     \
    ({                                                                              \
        __typeof__(&(hm)->table.data[0].data[0].value) _val = NULL;                 \
        size_t _hash = _ds_hm_hfn((hm), (key_v)) % (hm)->table.length;              \
        for (size_t _i = 0; _i < (hm)->table.data[_hash].length; _i++) {            \
            if (_ds_hm_eqfn((hm), (hm)->table.data[_hash].data[_i].key, (key_v))) { \
                _val = &(hm)->table.data[_hash].data[_i].value;                     \
                ds_da_remove(&(hm)->table.data[_hash], _i, 1);                      \
                (hm)->length--;                                                     \
            }                                                                       \
        }                                                                           \
        _val;                                                                       \
    })

/**
 * Loop over all key-value pairs in the hash map.
 * It will define `key` and `value` variables containing the current key and value in the current scope (may conflict with existing variables).
 * Example:
```c
{
    ds_hm_foreach(&hm, key, value) {
        printf("Key: %d, Value: %s\n", key, value);
    }
}
```
 */
#define ds_hm_foreach(hm, key_var, val_var)                                                                    \
    __typeof__((hm)->table.data[0].data[0].key) key_var;                                                       \
    __typeof__((hm)->table.data[0].data[0].value) val_var;                                                     \
    for (size_t _i_##key_var = 0; _i_##key_var < (hm)->table.length; _i_##key_var++)                           \
        for (size_t _j_##key_var = 0; _j_##key_var < (hm)->table.data[_i_##key_var].length &&                  \
                                      ((key_var) = (hm)->table.data[_i_##key_var].data[_j_##key_var].key,      \
                                      (val_var) = (hm)->table.data[_i_##key_var].data[_j_##key_var].value, 1); \
             _j_##key_var++)

/**
 * Free the hash map.
 * It will not free the keys or values themselves.
 * You should free the keys and values separately if needed.
 */
#define ds_hm_free(hm)                                       \
    do {                                                     \
        for (size_t _i = 0; _i < (hm)->table.length; _i++) { \
            ds_da_free(&(hm)->table.data[_i]);               \
        }                                                    \
        ds_da_free(&(hm)->table);                            \
    } while (0)

#define da_mh_clear ds_hm_free

/**
 * Declare an hash set.
 * Example:
 *   `ds_hm_declare(my_map, int, const char *);`
 *
```c
This will create an hash set of int keys and const char values like this:
// Key-Value pair structure
typedef struct {
    int key;
    const char *value;
} my_map_Kv;
// Dynamic array of key-value pairs
typedef struct {
    my_map_Kv *data;
    size_t length;
    size_t capacity;
} my_map_Da;
// Hash set structure
typedef struct {
    my_map_Da *table;
    size_t (*hfn)(int);
    int (*eqfn)(int, int);
    size_t length;
} my_map;
```
 * You can customize the hash function and equality function for your specific key type.
 * `my_map hm = {.hfn = my_hash_function, .eqfn = my_eq_function};`
 * The hash function should follow the signature `size_t hash_function(key_t key);`
 * The equality function should follow the signature `int eq_function(key_t a, key_t b);`
 */
#define ds_hs_declare(name, val_t)          \
    ds_da_declare(name##_Da, val_t);        \
    ds_da_declare(name##_Table, name##_Da); \
    typedef struct {                        \
        name##_Table table;                 \
        size_t (*hfn)(val_t);               \
        int (*eqfn)(val_t, val_t);          \
        size_t length;                      \
    } name

/**
 * Check if a value exists in the set.
 * Returns true if the key is found, false otherwise.
 * Example:
 ```c
 ds_hs_declare(my_map, int);
 ...
    bool has = ds_hs_has(&hm, 42);
    printf("%s\n", has ? "found" : "not found");
 ```
 */
#define ds_hs_has(hm, val)                                                        \
    ({                                                                            \
        bool _has = false;                                                        \
        if ((hm)->table.length) {                                                 \
            size_t _hash = _ds_hm_hfn((hm), (val)) % (hm)->table.length;          \
            for (size_t _i = 0; _i < (hm)->table.data[_hash].length; _i++) {      \
                if (_ds_hm_eqfn((hm), (hm)->table.data[_hash].data[_i], (val))) { \
                    _has = true;                                                  \
                    break;                                                        \
                }                                                                 \
            }                                                                     \
        }                                                                         \
        _has;                                                                     \
    })

#define _ds_hs_resize(set)                                                                              \
    do {                                                                                                \
        size_t new_capacity = (set)->table.length == 0 ? DS_HM_INIT_CAPACITY : (set)->table.length * 2; \
        __typeof__((set)->table) new_table = {0};                                                       \
        ds_da_reserve(&new_table, new_capacity);                                                        \
        new_capacity = new_table.length = new_table.capacity;                                           \
        memset(new_table.data, 0, new_capacity * sizeof(*new_table.data));                              \
        for (size_t _i = 0; _i < (set)->table.length; _i++) {                                           \
            for (size_t _j = 0; _j < (set)->table.data[_i].length; _j++) {                              \
                __typeof__((set)->table.data[_i].data[_j]) v = (set)->table.data[_i].data[_j];          \
                size_t _h = _ds_hm_hfn((set), v) % new_capacity;                                        \
                if (new_table.data[_h].capacity == 0) ds_da_reserve_min(&new_table.data[_h], 1);        \
                ds_da_append(&new_table.data[_h], v);                                                   \
            }                                                                                           \
        }                                                                                               \
        ds_da_free(&(set)->table);                                                                      \
        (set)->table = new_table;                                                                       \
    } while (0)

/**
 * Add a value to the hash set.
 * Example:
```c
ds_hs_declare(my_set, int);
...
    my_set set = {0};
    ds_hs_add(&set, 42);
```
 */
#define ds_hs_add(set, val)                                                                         \
    do {                                                                                            \
        if ((set)->table.length == 0 || (set)->length >= (set)->table.length * DS_HM_LOAD_FACTOR) { \
            _ds_hs_resize(set);                                                                     \
        }                                                                                           \
        size_t _hash = _ds_hm_hfn((set), (val)) % (set)->table.length;                              \
        size_t _i;                                                                                  \
        for (_i = 0; _i < (set)->table.data[_hash].length; _i++) {                                  \
            if (_ds_hm_eqfn((set), (set)->table.data[_hash].data[_i], (val))) {                     \
                (set)->table.data[_hash].data[_i] = (val);                                          \
                break;                                                                              \
            }                                                                                       \
        }                                                                                           \
        if (_i == (set)->table.data[_hash].length) {                                                \
            ds_da_append(&(set)->table.data[_hash], val);                                           \
            (set)->length++;                                                                        \
        }                                                                                           \
    } while (0)

/**
 * Remove a value from the hash set and return a pointer to it, or NULL if not found.
 * Example:
 ```c
 bool has = ds_hs_remove(&hm, 42);
 ```
 */
#define ds_hs_remove(set, val)                                                  \
    ({                                                                          \
        bool _has = false;                                                      \
        size_t _hash = _ds_hm_hfn((set), (val)) % (set)->table.length;          \
        for (size_t _i = 0; _i < (set)->table.data[_hash].length; _i++) {       \
            if (_ds_hm_eqfn((set), (set)->table.data[_hash].data[_i], (val))) { \
                _has = true;                                                    \
                ds_da_remove(&(set)->table.data[_hash], _i, 1);                 \
                (set)->length--;                                                \
            }                                                                   \
        }                                                                       \
        _has;                                                                   \
    })

/**
 * Loop over all the values in the set.
 * It will define `value` variable containing the current value in the current scope (may conflict with existing variables).
 * Example:
```c
{
    ds_hs_foreach(&set, value) {
        printf("Value: %d\n", value);
    }
}
```
 */
#define ds_hs_foreach(set, val_var)                                                                        \
    __typeof__((set)->table.data[0].data[0]) val_var;                                                      \
    for (size_t _i_##val_var = 0; _i_##val_var < (set)->table.length; _i_##val_var++)                      \
        for (size_t _j_##val_var = 0; _j_##val_var < (set)->table.data[_i_##val_var].length &&             \
                                      ((val_var) = (set)->table.data[_i_##val_var].data[_j_##val_var], 1); \
             _j_##val_var++)

/**
 * Concatenate two seconds hash set into the first one.
 */
#define ds_hs_cat(set, hs2)        \
    do {                           \
        ds_hs_foreach((hs2), _v) { \
            ds_hs_add((set), _v);  \
        }                          \
    } while (0)

/**
 * Concatenate a dynamic array into a hash set.
 */
#define ds_hs_cat_da(set, da)      \
    do {                           \
        ds_da_foreach((da), _v) {  \
            ds_hs_add((set), *_v); \
        }                          \
    } while (0)

/**
 * Remove the elements of the second hash set from the first one.
 */
#define ds_hs_sub(set, hs2)          \
    do {                             \
        ds_hs_foreach((hs2), _v) {   \
            ds_hs_remove((set), _v); \
        }                            \
    } while (0)

/**
 * Remove the elements of a dynamic array from a hash set.
 */
#define ds_hs_sub_da(set, da)         \
    do {                              \
        ds_da_foreach((da), _v) {     \
            ds_hs_remove((set), *_v); \
        }                             \
    } while (0)

/**
 * Convert a hash set to a dynamic array.
 */
#define ds_hs_to_da(set, da)        \
    do {                            \
        ds_da_zero((da));           \
        ds_hs_foreach((set), _v) {  \
            ds_da_append((da), _v); \
        }                           \
    } while (0)

/**
 * Convert a dynamic array to a hash set.
 */
#define ds_da_to_hs(da, set)       \
    do {                           \
        ds_hm_free(set);           \
        ds_da_foreach((da), _v) {  \
            ds_hs_add((set), *_v); \
        }                          \
    } while (0)

/**
 * Free the hash set.
 * It will not free the keys or values themselves.
 * You should free the keys and values separately if needed.
 */
#define ds_hs_free ds_hm_free
#define ds_hs_clear ds_hs_free

/**
 * Declare a linked list.
 * The linked list will store elements of the specified type.
 * Example:
 * `ds_ll_declare(my_list, int);`
 *
 * This will create a linked list type called `my_list` that stores `int` values like this:
```c
typedef struct my_list_Node {
    int val;
    struct my_list_Node *next;
} my_list_Node;

typedef struct {
    my_list_Node *head;
    my_list_Node *tail;
    size_t size;
} my_list;
```
 */
#define ds_ll_declare(name, type) \
    typedef struct name##_Node {  \
        type val;                 \
        struct name##_Node *next; \
    } name##_Node;                \
    typedef struct {              \
        name##_Node *head;        \
        name##_Node *tail;        \
        size_t size;              \
    } name

/**
 * Push a new value onto the front of the linked list.
 * Example:
 ```c
 ds_ll_declare(my_list, int);
 ...
     my_list l = {0};
     ds_ll_push(&l, 42);
 ```
 */
#define ds_ll_push(ll, value)                       \
    do {                                            \
        __typeof__((ll)->head) _n = (ll)->head;     \
        (ll)->head = DS_ALLOC(sizeof(*(ll)->head)); \
        assert((ll)->head != NULL);                 \
        (ll)->head->val = (value);                  \
        (ll)->head->next = _n;                      \
        if (!(ll)->tail) {                          \
            (ll)->tail = (ll)->head;                \
        }                                           \
        (ll)->size++;                               \
    } while (0)

/**
 * Append a new value to the end of the linked list.
 * Example:
 ```c
 ds_ll_declare(my_list, int);
 ...
    my_list l = {0};
    ds_ll_append(&l, 42);
 ```
 */
#define ds_ll_append(ll, v)                                        \
    do {                                                           \
        __typeof__((ll)->head) _n = DS_ALLOC(sizeof(*(ll)->head)); \
        assert(_n != NULL);                                        \
        if (!(ll)->tail) {                                         \
            (ll)->head = (ll)->tail = _n;                          \
        } else {                                                   \
            (ll)->tail->next = _n;                                 \
            (ll)->tail = _n;                                       \
        }                                                          \
        (ll)->tail->val = (v);                                     \
        (ll)->size++;                                              \
    } while (0)

/**
 * Pop the front value from the linked list. Returns the popped value.
 * Example:
 * ```
 * int value = ds_ll_pop(&my_list);
 * ```
 */
#define ds_ll_pop(ll)                             \
    ({                                            \
        assert((ll)->head != NULL);               \
        __typeof__((ll)->head) _old = (ll)->head; \
        (ll)->head = (ll)->head->next;            \
        if (!(ll)->head) {                        \
            (ll)->tail = NULL;                    \
        }                                         \
        (ll)->size--;                             \
        _old;                                     \
    })

/**
 * Free the linked list.
 */
#define ds_ll_free(ll)          \
    while ((ll)->head) {        \
        DS_FREE(ds_ll_pop(ll)); \
    }

/**
 * Read the entire contents of a file into a string builder.
 * Example:
```c
DsString str = {0};
ds_read_entire_file("path/to/file.txt", &str);
```
 */
bool ds_read_entire_file(const char *path, DsString *str);

/**
 * Write the entire contents of a string builder to a file.
 * Example:
```c
DsString str = {0};
ds_read_entire_file("path/to/file.txt", &str);
ds_write_entire_file("path/to/output.txt", &str);
```
 */
bool ds_write_entire_file(const char *path, const DsString *str);

typedef struct {
    const char *data;
    size_t length;
} DsStringIterator;

#define ds_str_iter(str) \
    (DsStringIterator) { .data = (str)->data, .length = (str)->length }

#define ds_cstr_iter(str) \
    (DsStringIterator) { .data = (str), .length = strlen(str) }

#define ds_str_iter_empty \
    (DsStringIterator){.data = NULL, .length = 0}

/**
 * Move a string iterator by a delimiter.
 * Returns the next part of the string and updates the iterator.
 * It will not allocate any memory or modify the original string.
 * The part will not be null-terminated, so you should use the length to access it.
 * Example:
 ```c
 DsStringIterator next = ds_cstr_iter("path/to/file.txt");
 while (next.length > 0) {
    // next point to the remaining string
    DsStringIterator part = ds_s_split(&next, '/');
    // part will be {data: "path/to/file.txt", length: 4} -> {data: "to/file.txt", length: 2} -> {data: "file.txt", length: 8}
    // Do something with part
 }
```
 */
DsStringIterator ds_s_split(DsStringIterator *it, char sep);

DsStringIterator ds_s_ltrim(DsStringIterator *it);
DsStringIterator ds_s_rtrim(DsStringIterator *it);
DsStringIterator ds_s_trim(DsStringIterator *it);

/**
 * Check if a string starts with a given prefix.
 * ds_starts_with("hello world", "hello") -> true
 */
#define ds_starts_with_s(str, prefix, len) \
    (strncmp((str), (prefix), (len)) == 0)
#define ds_starts_with(str, prefix) \
    ds_starts_with_s((str), (prefix), strlen(prefix))

/**
 * Check if a string ends with a given suffix.
 * ds_ends_with("hello world", "world") -> true
 */
bool ds_ends_with_sn(const char *str, const char *suffix, size_t str_len, size_t len);
#define ds_ends_with(str, suffix) \
    ds_ends_with_sn((str), (suffix), strlen(str), strlen(suffix))
#define ds_ends_with_s(str, suffix, len) \
    ds_ends_with_sn((str), (suffix), strlen(str), (len))
#define ds_ends_with_n(str, suffix, str_len) \
    ds_ends_with_sn((str), (suffix), (str_len), strlen(suffix))

/**
 * Create a directory and all parent directories if they don't exist.
 * Example:
 * `ds_mkdir_p("path/to/directory");`
 */
bool ds_mkdir_p(const char *path);

/** Arena allocator */
typedef struct DsRegion {
    size_t size;
    size_t capacity;
    struct DsRegion *next;
    uintptr_t items[];
} DsRegion;

typedef struct DsRRegion {
    size_t size;
    struct DsRRegion *next;
    uintptr_t items[];
} DsRRegion;

/**
 * Arena allocator structure.
 * Better for allocating many small objects with similar lifetimes.
 */
typedef struct {
    DsRegion *start, *end;
} DsArena;

/**
 * Full region arena allocator structure.
 * Better for allocating dynamic sized objects and bigger objects.
 */
typedef struct {
    DsRRegion *start, *end;
} DsRArena;

typedef struct {
    DsRegion *region;
    size_t size;
} DsArenaSnapshot;

#define DS_MIN_ALLOC_REGION (16 * 1024)
/**
 * Allocate memory from the arena.
 * Example:
```c
DsArena arena = {0};
int *arr = ds_a_malloc(&arena, 10 * sizeof(int));
```
 */
void *ds_a_malloc(DsArena *a, size_t size);
void *ds_a_rmalloc(DsRArena *a, size_t size);
/**
 * Reallocate memory from the arena.
 * Example:
```c
DsArena arena = {0};
int *arr = ds_a_malloc(&arena, 10 * sizeof(int));
arr = ds_a_realloc(&arena, arr, 10 * sizeof(int), 20 * sizeof(int));
```
 */
void *ds_a_realloc(DsArena *a, void *ptr, size_t old_size, size_t new_size);
void *ds_a_rrealloc(DsRArena *a, void *ptr, size_t new_size);
/**
 * Free all memory allocated from the arena.
 * Example:
```c
DsArena arena = {0};
int *arr = ds_a_malloc(&arena, 10 * sizeof(int));
ds_a_free(&arena);
```
 */
void ds_a_free(DsArena *a);
void ds_a_rfree(DsRArena *a);
void ds_a_rfree_one(DsRArena *a, void *ptr);

/**
 * Take a snapshot of the arena state.
 * You can restore the arena to this state later.
 * Example:
```c
DsArena arena = {0};
int *arr1 = ds_a_malloc(&arena, 10 * sizeof(int));
DsArenaSnapshot snap = ds_a_snapshot(&arena);
int *arr2 = ds_a_malloc(&arena, 20 * sizeof(int));
ds_a_restore(&arena, snap);
// arr2 is now invalid, and the arena is back to the state after arr1 allocation
```
 */
DsArenaSnapshot ds_a_snapshot(DsArena *a);
/**
 * Restore the arena to a previous snapshot.
 * All memory allocated after the snapshot will be freed.
 * Example:
```c
DsArena arena = {0};
int *arr1 = ds_a_malloc(&arena, 10 * sizeof(int));
DsArenaSnapshot snap = ds_a_snapshot(&arena);
int *arr2 = ds_a_malloc(&arena, 20 * sizeof(int));
ds_a_restore(&arena, snap);
// arr2 is now invalid, and the arena is back to the state after arr1 allocation
```
 */
void ds_a_restore(DsArena *a, DsArenaSnapshot snapshot);

extern DsArena ds_tmp_allocator;
/**
 * Temporary allocations using a global arena allocator.
 */
void *ds_tmp_alloc(size_t size);
/**
 * Temporary reallocations using a global arena allocator.
 */
void *ds_tmp_realloc(void *ptr, size_t old_size, size_t new_size);
/**
 * Free all temporary allocations.
 */
void ds_tmp_free();
/**
 * Duplicate a string using the temporary allocator.
 */
char* ds_tmp_strndup(const char* str, size_t len);
#define ds_tmp_strdup(str) ds_tmp_strndup((str), strlen(str))
/**
 * Formatted string allocation using the temporary allocator.
 */
char* ds_tmp_sprintf(const char* fmt, ...);
/**
 * Take a snapshot of the temporary allocator state.
 */
#define ds_tmp_snapshot() ds_a_snapshot(&ds_tmp_allocator)
/**
 * Restore the temporary allocator to a previous snapshot.
 */
#define ds_tmp_restore(snap) ds_a_restore(&ds_tmp_allocator, (snap))

/** dirent shims for windows */
#define DT_FILE 0x8
#ifdef _WIN32
#define DT_DIR 0x4
#define DT_REG DT_FILE
#define DT_UNKNOWN 0

struct dirent {
    unsigned char d_type;
    char d_name[MAX_PATH + 1];
};
typedef struct {
    HANDLE hFind;
    WIN32_FIND_DATA data;
    struct dirent *dirent;
} DIR;
DIR *opendir(const char *dirpath);
struct dirent *readdir(DIR *dirp);
int closedir(DIR *dirp);

#endif // _WIN32

#endif // DS_H_

#ifdef DS_IMPLEMENTATION

int ds_log_level = DS_LOG_INFO;
void ds_set_log_level(int level) {
    ds_log_level = level;
}
void ds_simple_log_handler(int level, const char *fmt, ...) {
    if (ds_log_level <= level) {
        FILE *log_file = level >= DS_LOG_ERROR ? stderr : stdout;
        va_list args;
        va_start(args, fmt);
        switch (level) {
        case DS_LOG_DEBUG:
            fprintf(log_file, "[DEBUG] ");
            break;
        case DS_LOG_INFO:
            fprintf(log_file, "[INFO] ");
            break;
        case DS_LOG_WARN:
            fprintf(log_file, "[WARN] ");
            break;
        case DS_LOG_ERROR:
            fprintf(log_file, "[ERROR] ");
            break;
        default:
            fprintf(log_file, "[LOG] ");
            break;
        }
        vfprintf(log_file, fmt, args);
        fflush(log_file);
        va_end(args);
    }
}

void ds_color_log_handler(int level, const char *fmt, ...) {
    FILE *log_file = level >= DS_LOG_ERROR ? stderr : stdout;
    va_list args;
    va_start(args, fmt);
    switch (level) {
    case DS_LOG_DEBUG:
        fprintf(log_file, "[\033[36mDEBUG\033[0m] ");
        break;
    case DS_LOG_INFO:
        fprintf(log_file, "[\033[32mINFO\033[0m] ");
        break;
    case DS_LOG_WARN:
        fprintf(log_file, "[\033[33mWARN\033[0m] ");
        break;
    case DS_LOG_ERROR:
        fprintf(log_file, "[\033[31mERROR\033[0m] ");
        break;
    default:
        fprintf(log_file, "[LOG] ");
        break;
    }
    vfprintf(log_file, fmt, args);
    fflush(log_file);
    va_end(args);
}

void (*ds_log_handler)(int level, const char *fmt, ...) = ds_simple_log_handler;
void ds_set_log_handler(void (*handler)(int level, const char *fmt, ...)) {
    ds_log_handler = handler;
}

void _ds_str_append(DsString *str, ...) {
    va_list args;
    va_start(args, str);
    const char *cstr;
    while ((cstr = va_arg(args, const char *))) {
        size_t len = strlen(cstr);
        ds_da_reserve(str, str->length + len + 1);
        memcpy(str->data + str->length, cstr, len);
        str->length += len;
    }
    va_end(args);
    ds_da_reserve(str, str->length + 1);
    str->data[str->length] = '\0';
}

void ds_str_appendf(DsString *str, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (n > 0) {
        ds_da_reserve(str, str->length + n + 1);
        va_start(args, fmt);
        vsnprintf(str->data + str->length, n + 1, fmt, args);
        va_end(args);
        str->length += n;
    }
}

void ds_str_prependf(DsString *str, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (n > 0) {
        ds_da_reserve(str, str->length + n + 1);
        char c0 = str->data[0];
        memmove(str->data + n, str->data, str->length);
        va_start(args, fmt);
        vsnprintf(str->data, n + 1, fmt, args);
        str->data[n] = c0;
        va_end(args);
        str->length += n;
    }
}

void ds_str_insert(DsString *dstr, const char *str, size_t index) {
    if (!dstr || !str || index > dstr->length) return;
    size_t len = strlen(str);
    ds_da_reserve(dstr, dstr->length + len + 1);
    memmove(dstr->data + index + len, dstr->data + index, dstr->length - index);
    memcpy(dstr->data + index, str, len);
    dstr->length += len;
}

bool ds_str_include(const DsString *str, const char *substr) {
    if (!str || !substr || str->length == 0 || strlen(substr) == 0) return false;
    return strstr(str->data, substr) != NULL;
}

DsString *ds_str_ltrim(DsString *str) {
    if (str && str->length > 0) {
        size_t i = 0;
        while (i < str->length && (str->data[i] == ' ' || str->data[i] == '\t' || str->data[i] == '\n' || str->data[i] == '\r')) {
            i++;
        }
        if (i > 0) {
            memmove(str->data, str->data + i, str->length - i + 1);
            str->length -= i;
        }
    }
    return str;
}

DsString *ds_str_rtrim(DsString *str) {
    if (str && str->length > 0) {
        size_t i = str->length;
        while (i > 0 && (str->data[i - 1] == ' ' || str->data[i - 1] == '\t' || str->data[i - 1] == '\n' || str->data[i - 1] == '\r')) {
            i--;
        }
        if (i < str->length) {
            str->data[i] = '\0';
            str->length = i;
        }
    }
    return str;
}

size_t _ds_hash_pointer(const void *key) {
    return (size_t)key;
}

size_t _ds_hash_int(int key) {
    uint32_t k = (uint32_t)key;
    k = (k ^ 61) ^ (k >> 16);
    k = k + (k << 3);
    k = k ^ (k >> 4);
    k = k * 0x27d4eb2d;
    k = k ^ (k >> 15);
    return (size_t)k;
}
size_t _ds_hash_long(long key) {
    uint64_t k = (uint64_t)key;
    k = (~k) + (k << 21);
    k = k ^ (k >> 24);
    k = (k + (k << 3)) + (k << 8);
    k = k ^ (k >> 14);
    k = (k + (k << 2)) + (k << 4);
    k = k ^ (k >> 28);
    k = k + (k << 31);
    return (size_t)k;
}

size_t _ds_hash_float(double key) {
    union {
        double f;
        uint64_t u;
    } tmp;
    tmp.f = key;
    return _ds_hash_long(tmp.u);
}

size_t _ds_hash_string(const char *key) {
    if (!key) return 0;
    size_t hash = 5381;
    int c;
    while ((c = *key++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

int _ds_eq_int(int a, int b) {
    return a == b;
}

int _ds_eq_long(long a, long b) {
    return a == b;
}

int _ds_eq_float(double a, double b) {
    return a == b;
}

int _ds_eq_str(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

int _ds_eq_ptr(const void *a, const void *b) {
    return a == b;
}

bool ds_read_entire_file(const char *path, DsString *str) {
    bool result = false;

    FILE *f = fopen(path, "rb");
    if (f == NULL) goto cleanup;
    if (fseek(f, 0, SEEK_END) < 0) goto cleanup;
    long s = ftell(f);
    if (s < 0) goto cleanup;
    if (fseek(f, 0, SEEK_SET) < 0) goto cleanup;

    size_t size = str->length + s;
    ds_da_reserve(str, size);
    fread(str->data + str->length, s, 1, f);
    if (ferror(f)) goto cleanup;
    str->length = size;
    result = true;
cleanup:
    if (!result)
        ds_log(DS_LOG_ERROR, "Could not read file %s: %s", path, strerror(errno));
    if (f)
        fclose(f);
    return result;
}

bool ds_write_entire_file(const char *path, const DsString *str) {
    bool result = false;
    FILE *f = fopen(path, "wb");
    if (f == NULL) goto cleanup;

    const char *buf = str->data;
    size_t size = str->length;
    while (size > 0) {
        size_t n = fwrite(buf, 1, size, f);
        if (ferror(f)) goto cleanup;
        size -= n;
        buf += n;
    }
    result = true;
cleanup:
    if (!result) ds_log(DS_LOG_ERROR, "Could not write file %s: %s\n", path, strerror(errno));
    if (f) fclose(f);
    return result;
}

DsStringIterator ds_s_split(DsStringIterator *it, char sep) {
    DsStringIterator part = {0};
    if (it->length == 0) return part;
    size_t i = 0;
    while (i < it->length && it->data[i] != sep)
        i++;
    part.data = it->data;
    part.length = i;
    if (i < it->length) {
        it->data += i + 1;
        it->length -= i + 1;
    } else {
        it->data += i;
        it->length -= i;
    }
    return part;
}

DsStringIterator ds_s_ltrim(DsStringIterator *it) {
    size_t i = 0;
    while (i < it->length && (it->data[i] == ' ' || it->data[i] == '\t' || it->data[i] == '\n' || it->data[i] == '\r')) {
        i++;
    }
    it->data += i;
    it->length -= i;
    return *it;
}

DsStringIterator ds_s_rtrim(DsStringIterator *it) {
    while (it->length > 0 && (it->data[it->length - 1] == ' ' || it->data[it->length - 1] == '\t' || it->data[it->length - 1] == '\n' || it->data[it->length - 1] == '\r')) {
        it->length--;
    }
    return *it;
}

DsStringIterator ds_s_trim(DsStringIterator *it) {
    ds_s_rtrim(it);
    ds_s_ltrim(it);
    return *it;
}

bool ds_mkdir_p(const char *path) {
    DsStringIterator iter = ds_cstr_iter(path);
    DsString tmp_path = {0};
    if (iter.length && iter.data[0] == '/') ds_da_append(&tmp_path, '/');

    while (iter.length) {
        DsStringIterator part = ds_s_split(&iter, '/');
        if (part.length == 0) continue;
        if (part.length == 1 && part.data[0] == '.') continue;
        ds_da_append_many(&tmp_path, part.data, part.length);
        ds_str_append(&tmp_path, "/");
#ifdef _WIN32
        int result = mkdir(tmp_path.data);
#else
        int result = mkdir(tmp_path.data, 0755);
#endif
        if (result < 0) {
            if (errno != EEXIST) {
                ds_log(DS_LOG_ERROR, "Could not create directory `%s`: %s", tmp_path.data,
                       strerror(errno));
                ds_da_free(&tmp_path);
                return false;
            }
        }
    }
    ds_da_free(&tmp_path);
    return true;
}

bool ds_ends_with_sn(const char *str, const char *suffix, size_t str_len, size_t len) {
    if (len > str_len) return false;
    return strncmp(str + str_len - len, suffix, len) == 0;
}

void *ds_a_malloc(DsArena *a, size_t size) {
    if (size == 0) return NULL;
    size = (size + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);
    if (!a->end || a->end->size + size > a->end->capacity) {
        size_t region_size = DS_MIN_ALLOC_REGION;
        if (size + sizeof(DsRegion) > region_size) {
            region_size = size + sizeof(DsRegion);
        }
        DsRegion *r = DS_ALLOC(region_size);
        if (!r) return NULL;
        r->size = 0;
        r->capacity = region_size - sizeof(DsRegion);
        r->next = NULL;
        if (a->end) {
            a->end->next = r;
            a->end = r;
        } else {
            a->start = a->end = r;
        }
    }
    void *ptr = (void *)((uintptr_t)a->end->items + a->end->size);
    a->end->size += size;
    return ptr;
}

void *ds_a_rmalloc(DsRArena *a, size_t size) {
    if (size == 0) return NULL;
    size = (size + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);
    DsRRegion *r = DS_ALLOC(size + sizeof(DsRRegion));
    if (!r) return NULL;
    r->size = size;
    r->next = NULL;
    if (a->end) {
        a->end->next = r;
        a->end = r;
    } else {
        a->start = a->end = r;
    }
    return r->items;
}

void *ds_a_realloc(DsArena *a, void *ptr, size_t old_size, size_t new_size) {
    if (new_size == 0 || new_size <= old_size) return NULL;
    void *new_ptr = ds_a_malloc(a, new_size);
    if (!new_ptr) return NULL;
    if (ptr && old_size > 0) {
        memcpy(new_ptr, ptr, old_size);
    }
    return new_ptr;
}

void *ds_a_rrealloc(DsRArena *a, void *ptr, size_t new_size) {
    if (new_size == 0) return NULL;
    if (!ptr) {
        return ds_a_rmalloc(a, new_size);
    }
    new_size = (new_size + sizeof(uintptr_t) - 1) & ~(sizeof(uintptr_t) - 1);
    DsRRegion *r = (DsRRegion *)ptr - 1;
    DsRRegion * new_r = DS_REALLOC(r, sizeof(DsRRegion) + new_size);
    if (!new_r) return NULL;
    new_r->size = new_size;
    if (r != new_r) {
        DsRRegion *curr = a->start;
        DsRRegion *prev = NULL;
        while (curr) {
            if (curr == r) {
                if (prev) {
                    prev->next = new_r;
                } else {
                    a->start = new_r;
                }
                if (a->end == r) {
                    a->end = new_r;
                }
                break;
            }
            prev = curr;
            curr = curr->next;
        }
    }
    return new_r->items;
}

void ds_a_free(DsArena *a) {
    DsRegion *r = a->start;
    while (r) {
        DsRegion *next = r->next;
        DS_FREE(r);
        r = next;
    }
    a->start = a->end = NULL;
}

void ds_a_rfree(DsRArena *a) {
    DsRRegion *r = a->start;
    while (r) {
        DsRRegion *next = r->next;
        DS_FREE(r);
        r = next;
    }
    a->start = a->end = NULL;
}

void ds_a_rfree_one(DsRArena *a, void *ptr) {
    DsRRegion *r = (DsRRegion *)ptr - 1;
    DsRRegion *curr = a->start;
    DsRRegion *prev = NULL;
    while (curr) {
        if (curr == r) {
            if (prev) {
                prev->next = curr->next;
            } else {
                a->start = curr->next;
            }
            if (a->end == curr) {
                a->end = prev;
            }
            DS_FREE(curr);
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

DsArenaSnapshot ds_a_snapshot(DsArena *a) {
    DsArenaSnapshot snapshot = {0};
    snapshot.region = a->end;
    if (a->end) snapshot.size = a->end->size;
    return snapshot;
}

void ds_a_restore(DsArena *a, DsArenaSnapshot snapshot) {
    if(!snapshot.region) {
        ds_a_free(a);
        return;
    }
    a->end = snapshot.region;
    a->end->size = snapshot.size;
    DsRegion *r = a->end ? a->end->next : NULL;
    while (r) {
        DsRegion *next = r->next;
        DS_FREE(r);
        r = next;
    }
}

DsArena ds_tmp_allocator = {0};

void *ds_tmp_alloc(size_t size) {
    return ds_a_malloc(&ds_tmp_allocator, size);
}
void *ds_tmp_realloc(void *ptr, size_t old_size, size_t new_size) {
    return ds_a_realloc(&ds_tmp_allocator, ptr, old_size, new_size);
}
void ds_tmp_free() {
    ds_a_free(&ds_tmp_allocator);
}

char* ds_tmp_strndup(const char* str, size_t len) {
    char* tmp_str = ds_tmp_alloc(len + 1);
    if (tmp_str) {
        memcpy(tmp_str, str, len);
        tmp_str[len] = '\0';
    }
    return tmp_str;
}

char* ds_tmp_sprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    int n = vsnprintf(NULL, 0, fmt, args);
    va_end(args);
    if (n < 0) return NULL;
    char* tmp_str = ds_tmp_alloc(n + 1);
    if (!tmp_str) return NULL;
    va_start(args, fmt);
    vsnprintf(tmp_str, n + 1, fmt, args);
    va_end(args);
    return tmp_str;
}



#ifdef _WIN32
DIR *opendir(const char *path) {
    assert(path);

    char buffer[MAX_PATH];
    snprintf(buffer, MAX_PATH, "%s\\*", path);
    DIR *dir = (DIR *)DS_ALLOC(sizeof(DIR));

    dir->hFind = FindFirstFile(buffer, &dir->data);
    if (dir->hFind == INVALID_HANDLE_VALUE) {
        free(dir);
        return NULL;
    }

    return dir;
}

struct dirent *readdir(DIR *dir) {
    assert(dir);

    if (dir->dirent == NULL) {
        dir->dirent = DS_ALLOC(sizeof(struct dirent));
    } else if (!FindNextFile(dir->hFind, &dir->data)) {
        return NULL;
    }

    strncpy(
        dir->dirent->d_name,
        dir->data.cFileName,
        sizeof(dir->dirent->d_name) - 1);
    dir->dirent->d_name[sizeof(dir->dirent->d_name) - 1] = '\0';
    dir->dirent->d_type = DT_UNKNOWN;
    if (dir->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        dir->dirent->d_type = DT_DIR;
    } else if (dir->data.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE) {
        dir->dirent->d_type = DT_FILE;
    }

    return dir->dirent;
}

int closedir(DIR *dir) {
    assert(dir);

    if (!FindClose(dir->hFind)) return -1;

    free(dir->dirent);
    free(dir);

    return 0;
}
#endif // _WIN32
#endif // DS_IMPLEMENTATION

#ifdef DS_NO_PREFIX
// Strip prefixes
#define UNREACHABLE DS_UNREACHABLE
#define TODO DS_TODO
#define UNUSED DS_UNUSED
#define log_info ds_log_info
#define log_debug ds_log_debug
#define log_warn ds_log_warn
#define log_error ds_log_error
#define set_log_level ds_set_log_level
#define set_log_handler ds_set_log_handler
#define color_log_handler ds_color_log_handler
#define simple_log_handler ds_simple_log_handler
#define LOG_DEBUG DS_LOG_DEBUG
#define LOG_INFO DS_LOG_INFO
#define LOG_WARN DS_LOG_WARN
#define LOG_ERROR DS_LOG_ERROR
#define ARRAY_LEN DS_ARRAY_LEN
#define da_declare ds_da_declare
#define da_reserve ds_da_reserve
#define da_reserve_with_init_capacity ds_da_reserve_with_init_capacity
#define da_reserve_min ds_da_reserve_min
#define da_append ds_da_append
#define da_remove ds_da_remove
#define da_insert ds_da_insert
#define da_prepend ds_da_prepend
#define da_pop ds_da_pop
#define da_append_many ds_da_append_many
#define da_last ds_da_last
#define da_first ds_da_first
#define da_zero ds_da_zero
#define da_free ds_da_free
#define da_foreach ds_da_foreach
#define da_foreach_idx ds_da_foreach_idx
#define da_find ds_da_find
#define da_index_of ds_da_index_of
#define str_append ds_str_append
#define str_appendf ds_str_appendf
#define str_prependf ds_str_prependf
#define str_insert ds_str_insert
#define str_prepend ds_str_prepend
#define str_include ds_str_include
#define str_ltrim ds_str_ltrim
#define str_rtrim ds_str_rtrim
#define str_trim ds_str_trim
#define hm_declare ds_hm_declare
#define hm_get ds_hm_get
#define hm_has ds_hm_has
#define hm_try ds_hm_try
#define hm_set ds_hm_set
#define hm_remove ds_hm_remove
#define hm_foreach ds_hm_foreach
#define hm_free ds_hm_free
#define hm_clear ds_hm_free
#define hs_declare ds_hs_declare
#define hs_has ds_hs_has
#define hs_add ds_hs_add
#define hs_remove ds_hs_remove
#define hs_foreach ds_hs_foreach
#define hs_cat ds_hs_cat
#define hs_cat_da ds_hs_cat_da
#define hs_sub ds_hs_sub
#define hs_sub_da ds_hs_sub_da
#define hs_to_da ds_hs_to_da
#define da_to_hs ds_da_to_hs
#define hs_free ds_hs_free
#define hs_clear ds_hs_free
#define ll_declare ds_ll_declare
#define ll_push ds_ll_push
#define ll_append ds_ll_append
#define ll_pop ds_ll_pop
#define ll_free ds_ll_free
#define String DsString
#define read_entire_file ds_read_entire_file
#define write_entire_file ds_write_entire_file
#define StringIterator DsStringIterator
#define s_split ds_s_split
#define s_ltrim ds_s_ltrim
#define s_rtrim ds_s_rtrim
#define s_trim ds_s_trim
#define str_iter ds_str_iter
#define cstr_iter ds_cstr_iter
#define str_iter_empty ds_str_iter_empty
#define mkdir_p ds_mkdir_p
#define starts_with ds_starts_with
#define starts_with_s ds_starts_with_s
#define ends_with ds_ends_with
#define ends_with_sn ds_ends_with_sn
#define ends_with_n ds_ends_with_n
#define ends_with_s ds_ends_with_s
#define Arena DsArena
#define RArena DsRArena
#define ArenaSnapshot DsArenaSnapshot
#define RArenaSnapshot DsRArenaSnapshot
#define a_malloc ds_a_malloc
#define a_realloc ds_a_realloc
#define a_free ds_a_free
#define a_rmalloc ds_a_rmalloc
#define a_rrealloc ds_a_rrealloc
#define a_rfree ds_a_rfree
#define a_rfree_one ds_a_rfree_one
#define a_snapshot ds_a_snapshot
#define a_restore ds_a_restore
#define tmp_alloc ds_tmp_alloc
#define tmp_realloc ds_tmp_realloc
#define tmp_free ds_tmp_free
#define tmp_strndup ds_tmp_strndup
#define tmp_strdup ds_tmp_strdup
#define tmp_sprintf ds_tmp_sprintf
#define tmp_snapshot ds_tmp_snapshot
#define tmp_restore ds_tmp_restore
#endif
