#ifndef _DISCORD_APPLICATION_H
#define _DISCORD_APPLICATION_H

#include <stdint.h>
#include <stdbool.h>

#include <json.h>

enum {
    APPLICATION_AUTO_MODERATION_RULE_CREATE_BADGE = 1 << 6,
    APPLICATION_GATEWAY_PRESENCE = 1 << 12,
    APPLICATION_GATEWAY_PRESENCE_LIMITED = 1 << 13,
    APPLICATION_GATEWAY_GUILT_MEMBERS = 1 << 14,
    APPLICATION_VERIFICATION_PENDING_GUILD_LIMIT = 1 << 16,
    APPLICATION_EMBEDDED = 1 << 17,
    APPLICATION_GATEWAY_MESSAGE_CONTENT = 1 << 18,
    APPLICATION_GATEWAY_MESSAGE_CONTENT_LIMITED = 1 << 19,
    APPLICATION_COMMAND_BADGE = 1 << 23,
};

struct application {
    uint64_t id;
    char* name;
    uint32_t flags;
};

bool application_parse(struct application* app, const json_object* data);
void application_cleanup(const struct application* app);

#endif
