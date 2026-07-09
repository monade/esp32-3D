#include "ht.h"


size_t yr__hash_string(const void *entry, size_t key_size, size_t seed) {
    (void)key_size;
    const char *str = *(const char *const *)entry;
    if (!str || !*str) return seed ? seed : 0;

    size_t hash = seed ? seed : 5381;

    // DJB2 hash algorithm - simple, fast, and well-distributed
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c; // hash * 33 + c
    }

    return hash;
}

size_t yr__hash_bytes(const void *entry, size_t key_size, size_t seed) {
    if (!entry || key_size == 0) return seed;

    const unsigned char *data = (const unsigned char *)entry;
    size_t hash = seed ? seed : 5381;

    // FNV-1a hash algorithm
    for (size_t i = 0; i < key_size; i++) {
        hash ^= data[i];
        hash *= 0x01000193; // FNV prime
    }

    return hash;
}

int yr__eq_bytes(const void *entry, const void *user_key, size_t key_size) {
    return memcmp(entry, user_key, key_size) == 0;
}

int yr__eq_str(const void *entry, const void *user_key, size_t key_size) {
    (void)key_size;
    const char *str = *(const char *const *)entry;
    const char *key = user_key;
    if (str == key) return 1;
    if (!str || !key) return 0;
    return strcmp(str, key) == 0;
}

bool yr__table_find(const struct yr__ht *ht, size_t entry_size, size_t key_size, const void *user_key, size_t key_hash, yr_entry_eq_fn eq_fn, size_t *out_slot, size_t *out_idx) {
    if (!ht->table.capacity) return false;
    size_t capacity = ht->table.capacity;
    size_t slot = key_hash % capacity;
    while (ht->table.data[slot] != 0) {
        size_t idx = ht->table.data[slot] - 1;
        const void *entry = (const char *)ht->data + idx * entry_size;
        if (eq_fn(entry, user_key, key_size)) {
            *out_slot = slot;
            *out_idx = idx;
            return true;
        }
        slot = (slot + 1) % capacity;
    }
    *out_slot = slot;
    return false;
}

void yr__ht_copy_key(void *yrt, const void *src, size_t key_size, bool is_cstr_key) {
    if (!is_cstr_key) {
        memcpy(yrt, src, key_size);
        return;
    }

    const char *str = NULL;
    memcpy(&str, src, sizeof(str));
    if (!str) {
        memset(yrt, 0, key_size);
        return;
    }

    size_t len = strlen(str) + 1;
    char *copy = YR_ALLOC(len);
    assert(copy != NULL);
    memcpy(copy, str, len);
    memcpy(yrt, &copy, sizeof(copy));
}

void yr__ht_free_key(void *key, bool is_cstr_key) {
    if (!is_cstr_key) return;

    void *ptr = NULL;
    memcpy(&ptr, key, sizeof(ptr));
    YR_FREE(ptr);
}

void yr__ht_free_cstr_keys(void *ht, size_t entry_size) {
    struct yr__ht *h = ht;
    for (size_t i = 0; i < h->length; i++) {
        void *entry = (char *)h->data + i * entry_size;
        yr__ht_free_key(entry, true);
    }
}

void yr__table_resize(struct yr__ht *ht, size_t entry_size, size_t key_size, yr_entry_hash_fn hash_fn, size_t new_capacity) {
    YR_FREE(ht->table.data);
    ht->table.data = YR_ALLOC(new_capacity * sizeof(size_t));
    assert(ht->table.data != NULL);
    memset(ht->table.data, 0, new_capacity * sizeof(size_t));
    for (size_t i = 0; i < ht->length; i++) {
        const void *entry = (const char *)ht->data + i * entry_size;
        size_t h = hash_fn(entry, key_size, ht->seed) % new_capacity;
        while (ht->table.data[h] != 0) {
            h = (h + 1) % new_capacity;
        }
        ht->table.data[h] = i + 1;
    }
    ht->table.capacity = new_capacity;
}

void yr__table_remove_slot(struct yr__ht *ht, size_t entry_size, size_t key_size, yr_entry_hash_fn hash_fn, size_t slot, size_t idx) {
    size_t capacity = ht->table.capacity;
    if (idx < ht->length) {
        const void *moved = (const char *)ht->data + idx * entry_size;
        size_t h = hash_fn(moved, key_size, ht->seed) % capacity;
        while (ht->table.data[h] != 0) {
            if (ht->table.data[h] - 1 == ht->length) {
                ht->table.data[h] = idx + 1;
                break;
            }
            h = (h + 1) % capacity;
        }
    }
    ht->table.data[slot] = 0;
    slot = (slot + 1) % capacity;
    while (ht->table.data[slot] != 0) {
        size_t i = ht->table.data[slot] - 1;
        ht->table.data[slot] = 0;
        const void *entry = (const char *)ht->data + i * entry_size;
        size_t h = hash_fn(entry, key_size, ht->seed) % capacity;
        while (ht->table.data[h] != 0) {
            h = (h + 1) % capacity;
        }
        ht->table.data[h] = i + 1;
        slot = (slot + 1) % capacity;
    }
}
