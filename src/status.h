#ifndef _STATUS_H
#define _STATUS_H

#include <stdint.h>
#include <stdbool.h>

/* from hiredis/hiredis.h */
typedef struct redisContext redisContext;

struct status {
    char* display_name;
    char* status_description;
    char* current_thought;
};

bool status_get(redisContext* db, uint64_t user, struct status* status);
void status_cleanup(const struct status* status);

#endif
