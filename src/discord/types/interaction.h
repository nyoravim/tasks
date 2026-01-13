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

struct interaction_component_data {
    uint32_t type;

    void* data;
    size_t data_size;
};

struct interaction {
    uint64_t id;
    uint64_t application_id;

    uint32_t type;
    union {
        struct interaction_command_data* command_data;
        struct interaction_component_data* component_data;

        void* reserved;
    };

    /* todo: guild */
    uint64_t guild_id;

    /* todo: channel */
    uint64_t channel_id;

    /* non-null if from a server */
    struct member* member;

    struct user* user;

    /* token for creating response */
    char* token;
};

bool interaction_parse(struct interaction* interaction, const json_object* data);
void interaction_cleanup(const struct interaction* interaction);

enum {
    MESSAGE_EPHEMERAL = 1 << 6,
    MESSAGE_IS_COMPONENTS_V2 = 1 << 15,
};

struct message_response {
    uint32_t flags;

    const char* content;

    size_t num_components;
    const struct component* components;
};

/* from bot.h */
typedef struct bot bot_t;

bool interaction_respond_with_message(const struct interaction* interaction, bot_t* bot,
                                      const struct message_response* data);

#endif
