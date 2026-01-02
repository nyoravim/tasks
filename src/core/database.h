#ifndef _DATABASE_H
#define _DATABASE_H

#include <stdint.h>

/* from hiredis/hiredis.h */
typedef struct redisContext redisContext;

typedef struct database database_t;

database_t* db_connect(const char* address, uint32_t port);
void db_close(database_t* db);

redisContext* db_get_context(const database_t* db);

#endif
