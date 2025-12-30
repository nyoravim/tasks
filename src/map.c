#include "map.h"

#include <malloc.h>
#include <string.h>

struct map_bucket_node {
    struct map_bucket_node* next;

    void* key;
    void* value;
};

struct map_bucket {
    struct map_bucket_node* first;
    struct map_bucket_node* last;

    size_t size;
};

typedef struct map {
    struct map_callbacks callbacks;

    size_t capacity;
    struct map_bucket* buckets;
} map_t;

static size_t hash_key(const map_t* map, const void* key) {
    size_t hash;
    if (map->callbacks.hash) {
        hash = map->callbacks.hash(map->callbacks.user, key);
    } else {
        hash = (size_t)key;
    }

    return hash % map->capacity;
}

static bool keys_equal(const map_t* map, const void* lhs, const void* rhs) {
    if (map->callbacks.equals) {
        return map->callbacks.equals(map->callbacks.user, lhs, rhs);
    } else {
        return lhs == rhs;
    }
}

static void free_key(const map_t* map, void* key) {
    if (map->callbacks.free_key) {
        map->callbacks.free_key(map->callbacks.user, key);
    }
}

static void free_value(const map_t* map, void* value) {
    if (map->callbacks.free_value) {
        map->callbacks.free_value(map->callbacks.user, value);
    }
}

map_t* map_alloc(size_t capacity, const struct map_callbacks* callbacks) {
    if (capacity == 0) {
        /* are you kidding me lmao */
        return NULL;
    }

    map_t* map = malloc(sizeof(map_t));
    memcpy(&map->callbacks, callbacks, sizeof(struct map_callbacks));

    map->capacity = capacity;
    map->buckets = calloc(capacity, sizeof(struct map_bucket));

    return map;
}

void map_free(map_t* map) {
    if (!map) {
        return;
    }

    for (size_t i = 0; i < map->capacity; i++) {
        struct map_bucket* bucket = &map->buckets[i];

        struct map_bucket_node* current = bucket->first;
        while (current) {
            free_key(map, current->key);
            free_value(map, current->value);

            struct map_bucket_node* next = current->next;
            free(current);
            current = next;
        }
    }

    free(map->buckets);
    free(map);
}

static void bucket_append(struct map_bucket* bucket, struct map_bucket_node* node) {
    if (bucket->last) {
        bucket->last->next = node;
    } else {
        bucket->first = node;
    }

    bucket->last = node;
    bucket->size++;
}

static void rehash_bucket(map_t* map, const struct map_bucket* src_bucket) {
    struct map_bucket_node* current = src_bucket->first;
    while (current) {
        struct map_bucket_node* next = current->next;

        size_t new_hash = hash_key(map, current->key);
        struct map_bucket* dst_bucket = &map->buckets[new_hash];

        current->next = NULL;
        bucket_append(dst_bucket, current);

        current = next;
    }
}

void map_reserve(map_t* map, size_t capacity) {
    if (capacity <= map->capacity) {
        return; /* no work needs to be done */
    }

    size_t src_capacity = map->capacity;
    struct map_bucket* src_buckets = map->buckets;

    map->capacity = capacity;
    map->buckets = calloc(capacity, sizeof(struct map_bucket));

    for (size_t i = 0; i < src_capacity; i++) {
        rehash_bucket(map, &src_buckets[i]);
    }

    free(src_buckets);
}

size_t map_size(const map_t* map) {
    size_t size = 0;

    for (size_t i = 0; i < map->capacity; i++) {
        size += map->buckets[i].size;
    }

    return size;
}

void map_enumerate(const map_t* map, struct key_value_pair* data) {
    size_t index = 0;

    for (size_t i = 0; i < map->capacity; i++) {
        struct map_bucket_node* current = map->buckets[i].first;

        while (current) {
            data[index].key = current->key;
            data[index].value = current->value;
            index++;

            current = current->next;
        }
    }
}

static struct map_bucket_node* bucket_find_node(const map_t* map, const struct map_bucket* bucket,
                                                const void* key, struct map_bucket_node** prev) {
    struct map_bucket_node* current = bucket->first;
    if (prev) {
        *prev = NULL;
    }

    while (current) {
        if (keys_equal(map, current->key, key)) {
            return current;
        }

        if (prev) {
            *prev = current;
        }

        current = current->next;
    }

    return NULL;
}

bool map_contains(const map_t* map, const void* key) {
    size_t hash = hash_key(map, key);
    struct map_bucket_node* node = bucket_find_node(map, &map->buckets[hash], key, NULL);

    return node != NULL;
}

bool map_get(const map_t* map, const void* key, void** value) {
    size_t hash = hash_key(map, key);
    struct map_bucket_node* node = bucket_find_node(map, &map->buckets[hash], key, NULL);

    if (node) {
        *value = node->value;
        return true;
    } else {
        return false;
    }
}

bool map_insert(map_t* map, void* key, void* value) {
    size_t hash = hash_key(map, key);
    struct map_bucket* bucket = &map->buckets[hash];

    if (bucket_find_node(map, bucket, key, NULL)) {
        /* already exists */
        return false;
    }

    struct map_bucket_node* node = malloc(sizeof(struct map_bucket_node));
    node->next = NULL;
    node->key = key;
    node->value = value;

    bucket_append(bucket, node);
    return true;
}

bool map_set(map_t* map, const void* key, void* value) {
    size_t hash = hash_key(map, key);
    struct map_bucket* bucket = &map->buckets[hash];

    struct map_bucket_node* node = bucket_find_node(map, bucket, key, NULL);
    if (node) {
        free_value(map, node->value);
        node->value = value;

        return true;
    } else {
        return false;
    }
}

bool map_put(map_t* map, void* key, void* value) {
    size_t hash = hash_key(map, key);
    struct map_bucket* bucket = &map->buckets[hash];

    struct map_bucket_node* node = bucket_find_node(map, bucket, key, NULL);
    if (node) {
        free_value(map, value);
        node->value = value;

        return false; /* not taking ownership of key */
    } else {
        struct map_bucket_node* node = malloc(sizeof(struct map_bucket_node));
        node->next = NULL;
        node->key = key;
        node->value = value;

        bucket_append(bucket, node);
        return true; /* taking ownership of key */
    }
}

bool map_remove(map_t* map, const void* key) {
    size_t hash = hash_key(map, key);
    struct map_bucket* bucket = &map->buckets[hash];

    struct map_bucket_node* prev;
    struct map_bucket_node* node = bucket_find_node(map, bucket, key, &prev);

    if (!node) {
        return false;
    }

    if (prev) {
        prev->next = node->next;
    } else {
        bucket->first = node->next;
    }

    if (bucket->last == node) {
        bucket->last = prev;
    }

    free_key(map, node->key);
    free_value(map, node->value);
    free(node);

    return true;
}
