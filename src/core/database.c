#include "database.h"

#include <nyoravim/mem.h>

#include <assert.h>

#include <log.h>

#include <hiredis/hiredis.h>

typedef struct database {
    redisContext* ctx;
} database_t;

database_t* db_connect(const char* address, uint32_t port) {
    redisContext* ctx = redisConnect(address, port);
    if (ctx->err != REDIS_OK) {
        log_error("failed to connect to database: %s", ctx->errstr);

        redisFree(ctx);
        return NULL;
    }

    database_t* db = nv_alloc(sizeof(database_t));
    assert(db);

    db->ctx = ctx;
    return db;
}

void db_close(database_t* db) {
    if (!db) {
        return;
    }

    redisFree(db->ctx);
    nv_free(db);
}

redisContext* db_get_context(const database_t* db) { return db->ctx; }
