#ifndef _MAP_H
#define _MAP_H

#include <stdbool.h>
#include <stddef.h>

struct map_callbacks {
    void* user;

    bool (*equals)(void* user, const void* lhs, const void* rhs);
    size_t (*hash)(void* user, const void* key);

    void (*free_key)(void* user, void* key);
    void (*free_value)(void* user, void* value);
};

struct key_value_pair {
    const void* key;
    void* value;
};

typedef struct map map_t;

map_t* map_alloc(size_t capacity, const struct map_callbacks* callbacks);
void map_free(map_t* map);

void map_reserve(map_t* map, size_t capacity);

size_t map_size(const map_t* map);

/* retrieves all data in the map. */
/* data must be able to contain an adequate number of elements as per map_size(const map_t*) */
void map_enumerate(const map_t* map, struct key_value_pair* data);

bool map_contains(const map_t* map, const void* key);
bool map_get(const map_t* map, const void* key, void** value);

/* inserts a value into map if the key doesnt already exist. */
/* takes ownership of key and value if returning true. */
bool map_insert(map_t* map, void* key, void* value);

/* sets an existing key to a value. */
/* takes ownership of value if returning true. */
bool map_set(map_t* map, const void* key, void* value);

/* sets key equal to value, regardless of if key already exists. */
/* always takes ownership of value. */
/* takes ownership of key if returning true. */
bool map_put(map_t* map, void* key, void* value);

/* returns true if key existed and was removed. */
bool map_remove(map_t* map, const void* key);

#endif
