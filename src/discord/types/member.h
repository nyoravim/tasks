#ifndef _DISCORD_MEMBER_H
#define _DISCORD_MEMBER_H

#include <stdbool.h>

#include <json.h>

struct member {
    struct user* user;
    char* nick;
};

bool member_parse(struct member* member, const json_object* data);
void member_cleanup(const struct member* member);

#endif
