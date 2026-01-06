#ifndef _DISCORD_USER_H
#define _DISCORD_USER_H

#include <stdint.h>
#include <stdbool.h>

#include <json.h>

struct user {
    /* snowflake */
    uint64_t id;

    char* username;
    char* discriminator;

    /* can be null */
    char* global_name;

    /* todo: add new fields as necessary */
};

bool user_parse(struct user* user, const json_object* data);
void user_cleanup(const struct user* user);

#endif
