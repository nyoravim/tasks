#ifndef _COMMAND_MANAGER_H
#define _COMMAND_MANAGER_H

#include <stdbool.h>

#include <concord/discord.h>

typedef struct command_manager command_manager_t;

struct command_context {
    void* user;
    void* data;
    void* user_context;

    const struct discord_application_command* command;
    const struct discord_interaction* event;
};

typedef void (*command_callback)(const struct command_context* context);

struct command_spec {
    /* unique name within scope */
    const char* name;

    /* user-friendly description */
    const char* description;

    /* command-specific data */
    void* data;

    /* to be called when command is invoked */
    command_callback callback;

    /* type */
    enum discord_application_command_types type;

    /* options */
    const struct discord_application_command_options* options;

    /* member permissions (?) */
    uint64_t default_member_permissions;

    /* can this be used in dms? */
    bool dm_permission;

    /* todo(nora): what does this do */
    bool default_permission;

    /* whether to scope command to specific guild (guild_id) */
    bool guild_specific;

    /* guild to add command to */
    uint64_t guild_id;
};

command_manager_t* cm_new(void* user, struct discord* client, uint64_t app_id);
void cm_destroy(command_manager_t* cm);

bool cm_add_command(command_manager_t* cm, const struct command_spec* spec);

bool cm_process_interaction(const command_manager_t* cm, void* user_context,
                            const struct discord_interaction* event);

#endif
