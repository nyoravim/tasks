#ifndef _COMMAND_H
#define _COMMAND_H

#include <stdint.h>

#include <nyoravim/map.h>

enum {
    COMMAND_TYPE_CHAT_INPUT = 1,
    COMMAND_TYPE_USER = 2,
    COMMAND_TYPE_MESSAGE = 3,
    COMMAND_TYPE_PRIMARY_ENTRY_POINT = 4,
};

enum {
    /* sub commands not supported yet */

    COMMAND_OPTION_TYPE_STRING = 3,
    COMMAND_OPTION_TYPE_INTEGER = 4,
    COMMAND_OPTION_TYPE_BOOLEAN = 5,
    COMMAND_OPTION_TYPE_USER = 6,
    COMMAND_OPTION_TYPE_CHANNEL = 7,
    COMMAND_OPTION_TYPE_ROLE = 8,
    COMMAND_OPTION_TYPE_MENTIONABLE = 9,
    COMMAND_OPTION_TYPE_NUMBER = 10,
    COMMAND_OPTION_TYPE_ATTACHMENT = 11,
};

typedef struct command command_t;

/* from bot.h */
typedef struct bot bot_t;

struct command_invocation_context {
    command_t* cmd;
    void* user;

    const struct interaction* interaction;
    const nv_map_t* options;
};

struct command_option_spec {
    const char* name;
    const char* description;

    uint32_t type;
    bool required;

    const nv_map_t* choices;
};

typedef void (*command_invocation_callback)(const struct command_invocation_context* context);

struct command_spec {
    bot_t* bot;
    void* user;

    command_invocation_callback callback;

    const char* name;
    const char* description;
    uint32_t type;

    size_t num_options;
    const struct command_option_spec* options;

    uint64_t guild_id;
};

command_t* command_register(const struct command_spec* spec);
void command_free(command_t* cmd);

/* from types/interaction.h */
struct interaction;

bool command_invoke(command_t* cmd, const struct interaction* event);

#endif
