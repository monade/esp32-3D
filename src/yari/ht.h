#ifndef YR_HT_H
#define YR_HT_H

#include <stddef.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <assert.h>
#include "da.h"

#ifndef YR_ALLOC
#define YR_ALLOC malloc
#endif

#ifndef YR_FREE
#define YR_FREE free
#endif

#ifndef YR_HM_LOAD_FACTOR
/**
 * Load factor for hash maps and sets.
 */
#define YR_HM_LOAD_FACTOR 0.8f
#endif

#ifndef YR_HM_INIT_CAPACITY
/**
 * Initial capacity for hash maps and sets.
 */
#define YR_HM_INIT_CAPACITY 16
#endif

#ifndef YR_HT_GROW_FACTOR
#define YR_HT_GROW_FACTOR 1.5
#endif
#ifndef YR_HT_SHRINK_FACTOR
#define YR_HT_SHRINK_FACTOR 2
#endif

struct yr__ht_idxs {
    size_t *data;
    size_t capacity;
};

/* Generic view over hashmap/hashset structs: yr_Hm and yr_Hs share this
 * common initial sequence, so a pointer to either can be cast to yr_ht *. */
struct yr__ht {
    void *data;
    size_t length;
    size_t capacity;
    struct yr__ht_idxs table;
    size_t seed;
};

/**
 * Generic key value struct
 */
#define yr_HmKv(key_t, val_t) \
    struct {                  \
        key_t key;            \
        val_t value;          \
    }

typedef size_t (*yr_entry_hash_fn)(const void *entry, size_t key_size, size_t seed);
typedef int (*yr_entry_eq_fn)(const void *entry, const void *user_key, size_t key_size);

size_t yr__hash_string(const void *entry, size_t key_size, size_t seed);
size_t yr__hash_bytes(const void *entry, size_t key_size, size_t seed);
int yr__eq_str(const void *entry, const void *user_key, size_t key_size);
int yr__eq_bytes(const void *entry, const void *user_key, size_t key_size);

bool yr__table_find(const struct yr__ht *ht, size_t entry_size, size_t key_size, const void *user_key, size_t key_hash, yr_entry_eq_fn eq_fn, size_t *out_slot, size_t *out_idx);

void yr__table_resize(struct yr__ht *ht, size_t entry_size, size_t key_size, yr_entry_hash_fn hash_fn, size_t new_capacity);

void yr__table_remove_slot(struct yr__ht *ht, size_t entry_size, size_t key_size, yr_entry_hash_fn hash_fn, size_t slot, size_t idx);

#define yr__ht_view(ht) ((struct yr__ht *)(void *)(ht))

#define yr__ht_key_is_cstr(key) \
    _Generic((key), char *: true, const char *: true, default: false)

void yr__ht_copy_key(void *yrt, const void *src, size_t key_size, bool is_cstr_key);
void yr__ht_free_key(void *key, bool is_cstr_key);

void yr__ht_free_cstr_keys(void *ht, size_t entry_size);

#define yr__ht_free_keys(ht)                            \
    do {                                                \
        if (yr__ht_key_is_cstr((ht)->data[0].key)) {    \
            yr__ht_free_cstr_keys(yr__ht_view(ht),      \
                                  sizeof(*(ht)->data)); \
        }                                               \
    } while (0)

#define yr__entry_hfn(key)             \
    _Generic((key),                    \
        char *: yr__hash_string,       \
        const char *: yr__hash_string, \
        default: yr__hash_bytes)

#define yr__entry_eqfn(key)       \
    _Generic((key),               \
        char *: yr__eq_str,       \
        const char *: yr__eq_str, \
        default: yr__eq_bytes)

#define yr__user_key_ptr(key) \
    _Generic((key), char *: (key), const char *: (key), default: &(key))

/* shared helpers (hashmap and hashset have identical layout) */
#define yr__ht_should_resize(ht) \
    ((ht)->length >= (ht)->table.capacity * YR_HM_LOAD_FACTOR)

#define yr__ht_resize(ht, _k)                                          \
    yr__table_resize(yr__ht_view(ht), sizeof(*(ht)->data), sizeof(_k), \
                     yr__entry_hfn(_k),                                \
                     (ht)->table.capacity == 0 ? YR_HM_INIT_CAPACITY   \
                                               : (ht)->table.capacity * YR_HT_GROW_FACTOR)

/* shrink the table (in reverse of yr__ht_resize) once it gets underfilled,
 * never going below the initial capacity */
#define yr__ht_should_shrink(ht)                    \
    ((ht)->table.capacity > YR_HM_INIT_CAPACITY &&  \
     (ht)->length <= (ht)->table.capacity / YR_HT_SHRINK_FACTOR)

#define yr__ht_shrink(ht, _k)                                                    \
    do {                                                                        \
        size_t _new_cap = (size_t)((ht)->table.capacity / YR_HT_GROW_FACTOR);   \
        if (_new_cap < YR_HM_INIT_CAPACITY) _new_cap = YR_HM_INIT_CAPACITY;     \
        yr__table_resize(yr__ht_view(ht), sizeof(*(ht)->data), sizeof(_k),      \
                         yr__entry_hfn(_k), _new_cap);                          \
    } while (0)

#define yr__ht_find(ht, _k, slot_p, idx_p)                           \
    yr__table_find(yr__ht_view(ht), sizeof(*(ht)->data), sizeof(_k), \
                   yr__user_key_ptr(_k),                             \
                   yr__entry_hfn(_k)(&(_k), sizeof(_k), (ht)->seed), \
                   yr__entry_eqfn(_k), (slot_p), (idx_p))

#define yr__ht_remove_slot(ht, _k, slot, idx)                               \
    yr__table_remove_slot(yr__ht_view(ht), sizeof(*(ht)->data), sizeof(_k), \
                          yr__entry_hfn(_k), (slot), (idx))

/**
 * Declare a hash map.
 * Example:
 *   `yr_hm_declare(my_map, int, const char *);`
 *
 * This will create a hash map of int keys and const char values like this:
 ```c
 // Hash map structure with open addressing
 typedef struct {
    struct {
        int key;
        const char *value;
    } *data;              // Dynamic array of key-value pairs
    size_t length;        // Number of elements
    size_t capacity;      // Capacity of data array
    struct yr__ht_idxs table; // Hash table (stores indices+1 into data array)
    size_t seed;          // Hash seed
 } my_map;
 ```
 * Or without the typedef:
 *   `yr_Hm(int, const char *) my_map = {0};`
 */

#define yr_Hm(key_t, val_t)           \
    struct {                          \
        yr_HmKv(key_t, val_t) * data; \
        size_t length;                \
        size_t capacity;              \
        struct yr__ht_idxs table;     \
        size_t seed;                  \
    }

#define yr_hm_declare(name, key_t, val_t) typedef yr_Hm(key_t, val_t) name

/**
 * Set a key-value pair in the hash map.
 * Example:
```c
yr_hm_declare(my_map, int, const char *);
...
    my_map hm = {0};
    yr_hm_set(&hm, 42, "Hello");
```
 */
#define yr_hm_set(hm, key_v, val_v)                          \
    do {                                                     \
        __typeof__((hm)->data[0].key) _k = (key_v);          \
        __typeof__((hm)->data[0].value) _v = (val_v);        \
        if (yr__ht_should_resize(hm)) yr__ht_resize(hm, _k); \
        size_t _slot = 0, _idx = 0;                          \
        if (yr__ht_find(hm, _k, &_slot, &_idx)) {            \
            (hm)->data[_idx].value = _v;                     \
        } else {                                             \
            __typeof__(*(hm)->data) _entry = {.value = _v};  \
            yr__ht_copy_key(&_entry.key, &_k, sizeof(_k),    \
                            yr__ht_key_is_cstr(_k));         \
            yr_da_append((hm), _entry);                      \
            (hm)->table.data[_slot] = (hm)->length;          \
        }                                                    \
    } while (0)

/**
 * Try to get a value from the hash map.
 * Returns NULL if the key is not found else it returns a pointer to the value.
 * Example:
 ```c
 yr_hm_declare(my_map, int, const char *);
 ...
    const char **value = yr_hm_try(&hm, 42);
    printf("%s\n", *value);
 ```
 */
#define yr_hm_try(hm, key_v)                                                 \
    ({                                                                       \
        __typeof__((hm)->data[0].key) _k = (key_v);                          \
        size_t _slot, _idx;                                                  \
        yr__ht_find(hm, _k, &_slot, &_idx) ? &(hm)->data[_idx].value : NULL; \
    })

#define yr_hm_has(hm, key_v) (yr_hm_try((hm), (key_v)) != NULL)

/**
 * Get a value from the hash map. It will return the value associated with the key
 * The result will be {0} if the key is not found
 * You should use yr_hm_try if you are not really sure the key is present
 * Example:
 ```c
 const char *value = yr_hm_get(&hm, 42);
 ```
 */
#define yr_hm_get(hm, key_v)                                            \
    ({                                                                  \
        __typeof__((hm)->data[0].value) *_p = yr_hm_try((hm), (key_v)); \
        _p ? *_p : (__typeof__((hm)->data[0].value)){0};                \
    })

/**
 * Remove a value from the hash map and return a pointer to it, or NULL if not found.
 * Example:
 ```c
 const char **value = yr_hm_remove(&hm, 42);
 ```
 */
#define yr_hm_remove(hm, key_v)                                \
    ({                                                         \
        __typeof__((hm)->data[0].key) _k = (key_v);            \
        __typeof__(&(hm)->data[0].value) _val = NULL;          \
        size_t _slot, _idx;                                    \
        if (yr__ht_find(hm, _k, &_slot, &_idx)) {              \
            __typeof__((hm)->data[0]) _tmp = (hm)->data[_idx]; \
            yr__ht_free_key(&_tmp.key,                         \
                            yr__ht_key_is_cstr(_tmp.key));     \
            memset(&_tmp.key, 0, sizeof(_tmp.key));            \
            yr_da_remove_unordered((hm), _idx);                \
            (hm)->data[(hm)->length] = _tmp;                   \
            _val = &(hm)->data[(hm)->length].value;            \
            yr__ht_remove_slot(hm, _k, _slot, _idx);           \
            if (yr__ht_should_shrink(hm)) yr__ht_shrink(hm, _k); \
        }                                                      \
        _val;                                                  \
    })

/**
 * Loop over all key-value pairs in the hash map.
 * Example:
```c
yr_hm_foreach(&hm, kv) {
    printf("Key: %d, Value: %s\n", kv->key, kv->value);
}
```
 */
#define yr_hm_foreach yr_da_foreach

/**
 * Free the hash map.
 * It will not free the keys or values themselves.
 * You should free the keys and values separately if needed.
 */
#define yr_hm_free(hm)             \
    do {                           \
        YR_FREE((hm)->table.data); \
        yr__ht_free_keys((hm));    \
        (hm)->table.data = NULL;   \
        (hm)->table.capacity = 0;  \
        yr_da_free((hm));          \
    } while (0)
#define yr_hm_clear yr_hm_free

/**
 * Declare an hash set.
 * Example:
 *   `yr_hm_declare(my_map, int, const char *);`
 *
 * This will create an hash set of int keys and const char values like this:
 ```c
 typedef struct {
    val_t *data;              // Dynamic array of values
    size_t length;            // Number of elements
    size_t capacity;          // Capacity of data array
    struct yr__ht_idxs table; // Hash table (stores indices+1 into data array)
    size_t seed;              // Hash seed
 } my_set;
 ```
 * Or without the typedef:
 *   `yr_Hs(int) my_set = {0};`
*/
#define yr_Hs(val_t)              \
    struct {                      \
        val_t *data;              \
        size_t length;            \
        size_t capacity;          \
        struct yr__ht_idxs table; \
        size_t seed;              \
    }

#define yr_hs_declare(name, val_t) typedef yr_Hs(val_t) name

/**
 * Check if a value exists in the set.
 * Returns true if the key is found, false otherwise.
 * Example:
 ```c
 yr_hs_declare(my_map, int);
 ...
    bool has = yr_hs_has(&hm, 42);
    printf("%s\n", has ? "found" : "not found");
 ```
 */
#define yr_hs_has(set, val_v)                  \
    ({                                         \
        __typeof__(*(set)->data) _k = (val_v); \
        size_t _slot, _idx;                    \
        yr__ht_find(set, _k, &_slot, &_idx);   \
    })

/**
 * Add a value to the hash set.
 * Example:
```c
yr_hs_declare(my_set, int);
...
    my_set set = {0};
    yr_hs_add(&set, 42);
```
 */
#define yr_hs_add(set, val_v)                                  \
    do {                                                       \
        __typeof__(*(set)->data) _k = (val_v);                 \
        if (yr__ht_should_resize(set)) yr__ht_resize(set, _k); \
        size_t _slot, _idx;                                    \
        if (!yr__ht_find(set, _k, &_slot, &_idx)) {            \
            yr_da_append((set), _k);                           \
            (set)->table.data[_slot] = (set)->length;          \
        }                                                      \
    } while (0)

/**
 * Remove a value from the hash set and return a pointer to it, or NULL if not found.
 * Example:
 ```c
 bool has = yr_hs_remove(&hm, 42);
 ```
 */
#define yr_hs_remove(set, val_v)                           \
    ({                                                     \
        __typeof__(*(set)->data) _k = (val_v);             \
        size_t _slot, _idx;                                \
        bool _found = yr__ht_find(set, _k, &_slot, &_idx); \
        if (_found) {                                      \
            yr_da_remove_unordered((set), _idx);           \
            yr__ht_remove_slot(set, _k, _slot, _idx);      \
            if (yr__ht_should_shrink(set)) yr__ht_shrink(set, _k); \
        }                                                  \
        _found;                                            \
    })

/**
 * Loop over all the values in the set.
 * Example:
```c
yr_hs_foreach(&set, value) {
    printf("Value: %d\n", value);
}
```
 */
#define yr_hs_foreach yr_da_foreach

/**
 * Concatenate the second hash set into the first one.
 */
#define yr_hs_cat(set, hs2)        \
    do {                           \
        yr_hs_foreach((hs2), _v) { \
            yr_hs_add((set), *_v); \
        }                          \
    } while (0)

/**
 * Concatenate a dynamic array into a hash set.
 */
#define yr_hs_cat_da(set, da)      \
    do {                           \
        yr_da_foreach((da), _v) {  \
            yr_hs_add((set), *_v); \
        }                          \
    } while (0)

/**
 * Remove the elements of the second hash set from the first one.
 */
#define yr_hs_sub(set, hs2)           \
    do {                              \
        yr_hs_foreach((hs2), _v) {    \
            yr_hs_remove((set), *_v); \
        }                             \
    } while (0)

/**
 * Remove the elements of a dynamic array from a hash set.
 */
#define yr_hs_sub_da(set, da)         \
    do {                              \
        yr_da_foreach((da), _v) {     \
            yr_hs_remove((set), *_v); \
        }                             \
    } while (0)

/**
 * Convert a hash set to a dynamic array.
 */
#define yr_hs_to_da(set, da)         \
    do {                             \
        (da)->count = 0;             \
        yr_hs_foreach((set), _v) {   \
            yr_da_append((da), *_v); \
        }                            \
    } while (0)

/**
 * Free the hash set.
 * It will not free the keys or values themselves.
 * You should free the keys and values separately if needed.
 */
#define yr_hs_free(set)             \
    do {                            \
        YR_FREE((set)->table.data); \
        (set)->table.data = NULL;   \
        (set)->table.capacity = 0;  \
        yr_da_free((set));          \
    } while (0)
#define yr_hs_clear yr_hs_free

/**
 * Convert a dynamic array to a hash set.
 */
#define yr_da_to_hs(da, set)       \
    do {                           \
        yr_hs_free(set);           \
        yr_da_foreach((da), _v) {  \
            yr_hs_add((set), *_v); \
        }                          \
    } while (0)

#ifdef YARI_NO_PREFIX
#define Hm yr_Hm
#define HmKv yr_HmKv
#define hm_declare yr_hm_declare
#define hm_get yr_hm_get
#define hm_has yr_hm_has
#define hm_try yr_hm_try
#define hm_set yr_hm_set
#define hm_remove yr_hm_remove
#define hm_foreach yr_hm_foreach
#define hm_free yr_hm_free
#define hm_clear yr_hm_free
#define Hs yr_Hs
#define hs_declare yr_hs_declare
#define hs_has yr_hs_has
#define hs_add yr_hs_add
#define hs_remove yr_hs_remove
#define hs_foreach yr_hs_foreach
#define hs_cat yr_hs_cat
#define hs_cat_da yr_hs_cat_da
#define hs_sub yr_hs_sub
#define hs_sub_da yr_hs_sub_da
#define hs_to_da yr_hs_to_da
#define da_to_hs yr_da_to_hs
#define hs_free yr_hs_free
#define hs_clear yr_hs_free
#endif
#endif
