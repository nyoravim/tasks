#include "status.h"

#include <inttypes.h>
#include <string.h>

#include <hiredis/hiredis.h>

#include <nyoravim/mem.h>

bool status_get(redisContext* db, uint64_t user, struct status* status) {
    memset(status, 0, sizeof(struct status));

    redisReply* reply = redisCommand(db, "HGETALL user:%" PRIu64, user);
    if (reply->type != REDIS_REPLY_ARRAY || reply->elements % 2 != 0) {
        freeReplyObject(reply);
        return false;
    }

    return true;
}

void status_cleanup(const struct status* status) {
    nv_free(status->display_name);
    nv_free(status->status_description);
    nv_free(status->current_thought);
}
