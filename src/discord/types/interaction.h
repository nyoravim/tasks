#ifndef _DISCORD_INTERACTIOH_H
#define _DISCORD_INTERACTIOH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include <json.h>

enum {
    INTERACTION_TYPE_PING = 1,
    INTERACTION_TYPE_APPLICATION_COMMAND = 2,
    INTERACTION_TYPE_MESSAGE_COMPONENT = 3,
    INTERACTION_TYPE_APPLICATION_COMMAND_AUTOCOMPLETE = 4,
    INTERACTION_TYPE_MODEL_SUBMIT = 5,
};

struct command_option_data {
    char* name;

    uint32_t type;
    char* value;

    size_t num_options;
    struct command_option_data* options;

    bool focused;
};

struct interaction_command_data {
    uint64_t id;
    char* name;

    uint32_t type;

    /* not gonna bother with resolved */

    size_t num_options;
    struct command_option_data* options;

    uint64_t guild_id;
    uint64_t target_id;
};

struct interaction {
    uint64_t id;
    uint64_t application_id;

    uint32_t type;
    void* data;

    /* todo: guild */
    uint64_t guild_id;

    /* todo: channel */
    uint64_t channel_id;

    /* todo: member */

    /* non-null if from a dm */
    struct user* user;

    /* token for creating response */
    char* token;
};

bool interaction_parse(struct interaction* interaction, const json_object* data);
void interaction_cleanup(const struct interaction* interaction);

#endif
