#ifndef _DATABASE_H
#define _DATABASE_H

#include <stdint.h>
#include <stdbool.h>

/* from hiredis/hiredis.h */
typedef struct redisContext redisContext;

enum {
    REDIS_VALUE_TYPE_STRING,
};

struct redis_value {
    uint32_t type;

    union {
        char* string;
    };
};

bool db_get_hash_field(redisContext* ctx, struct redis_value* value);

#endif
