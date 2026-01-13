#include "status.h"

#include <inttypes.h>
#include <string.h>

#include <hiredis/hiredis.h>

#include <log.h>

#include <nyoravim/mem.h>
#include <nyoravim/util.h>

#define DISPLAY_NAME_KEY "display"
#define STATUS_DESCRIPTION_KEY "status"
#define CURRENT_THOUGHT_KEY "thought"

static void update_status_field(struct status* status, const char* key, const char* value) {
    if (strcmp(key, DISPLAY_NAME_KEY) == 0) {
        status->display_name = nv_strdup(value);
        return;
    }

    if (strcmp(key, STATUS_DESCRIPTION_KEY) == 0) {
        status->status_description = nv_strdup(value);
        return;
    }

    if (strcmp(key, CURRENT_THOUGHT_KEY) == 0) {
        status->current_thought = nv_strdup(value);
        return;
    }

    log_warn("unassociated key in status hash: %s", key);
}

bool status_get(redisContext* db, uint64_t user, struct status* status) {
    memset(status, 0, sizeof(struct status));

    redisReply* reply = redisCommand(db, "HGETALL user:%" PRIu64, user);
    if (reply->type != REDIS_REPLY_ARRAY || reply->elements % 2 != 0) {
        log_error("invalid redis response");

        freeReplyObject(reply);
        return false;
    }

    for (size_t i = 0; i < reply->elements; i += 2) {
        redisReply* key_reply = reply->element[i];
        if (key_reply->type != REDIS_REPLY_STRING) {
            log_error("key not in string format!");
            continue;
        }

        redisReply* value_reply = reply->element[i + 1];
        if (value_reply->type != REDIS_REPLY_STRING) {
            log_warn("value for key %s not in string format!", key_reply->str);
            continue;
        }

        update_status_field(status, key_reply->str, value_reply->str);
    }

    return true;
}

void status_cleanup(const struct status* status) {
    nv_free(status->display_name);
    nv_free(status->status_description);
    nv_free(status->current_thought);
}
