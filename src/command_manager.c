#include "command_manager.h"

#include <nyoravim/map.h>
#include <nyoravim/mem.h>
#include <nyoravim/util.h>

#include <string.h>
#include <assert.h>

struct bot_command {
    void* data;
    command_callback callback;

    struct discord_application_command command;
};

struct command_family {
    struct bot_command* global;
    nv_map_t* guilds;
};

typedef struct command_manager {
    void* user;
    nv_map_t* families;

    struct discord* client;
    uint64_t app_id;
} command_manager_t;

static size_t cm_hash_key(void* user, const void* key) { return nv_hash_string(key); }

static bool cm_keys_equal(void* user, const void* lhs, const void* rhs) {
    return strcmp(lhs, rhs) == 0;
}

static void cm_free_key(void* user, void* key) { nv_free(key); }

static void free_command(struct bot_command* cmd) {
    if (!cmd) {
        return;
    }

    /* is this necessary? */
    discord_application_command_cleanup(&cmd->command);

    nv_free(cmd);
}

static void free_family(struct command_family* family) {
    if (!family) {
        return;
    }

    free_command(family->global);
    nv_map_free(family->guilds); /* will free commands */
    nv_free(family);
}

static void cm_free_value(void* user, void* value) { free_family(value); }

command_manager_t* cm_new(void* user, struct discord* client, uint64_t app_id) {
    struct nv_map_callbacks callbacks;
    memset(&callbacks, 0, sizeof(struct nv_map_callbacks));

    callbacks.hash = cm_hash_key;
    callbacks.equals = cm_keys_equal;

    callbacks.free_key = cm_free_key;
    callbacks.free_value = cm_free_value;

    command_manager_t* cm = nv_alloc(sizeof(command_manager_t));
    assert(cm);

    cm->user = user;
    cm->families = nv_map_alloc(64, &callbacks);

    cm->client = client;
    cm->app_id = app_id;

    assert(cm->families);
    return cm;
}

void cm_destroy(command_manager_t* cm) {
    if (!cm) {
        return;
    }

    nv_map_free(cm->families);
    nv_free(cm);
}

static struct bot_command* register_global_command(command_manager_t* cm,
                                                   const struct command_spec* spec) {
    struct discord_create_global_application_command params;
    params.default_member_permissions = spec->default_member_permissions;
    params.type = spec->type;
    params.default_permission = spec->default_permission;
    params.dm_permission = spec->dm_permission;

    /* grrr! */
    params.name = (char*)spec->name;
    params.description = (char*)spec->description;
    params.options = (struct discord_application_command_options*)spec->options;

    struct discord_application_command app_cmd;
    memset(&app_cmd, 0, sizeof(struct discord_application_command));

    struct discord_ret_application_command ret;
    memset(&ret, 0, sizeof(struct discord_ret_application_command));
    ret.sync = &app_cmd;

    if (discord_create_global_application_command(cm->client, cm->app_id, &params, &ret) !=
        CCORD_OK) {
        /* failure */
        return NULL;
    }

    struct bot_command* cmd = nv_alloc(sizeof(struct bot_command));
    assert(cmd);

    cmd->callback = spec->callback;
    cmd->data = spec->data;

    memcpy(&cmd->command, &app_cmd, sizeof(struct discord_application_command));
    return cmd;
}

static bool add_global_command(command_manager_t* cm, const struct command_spec* spec) {
    if (nv_map_contains(cm->families, spec->name)) {
        /* at least one command with name already exists */
        return false;
    }

    struct bot_command* cmd = register_global_command(cm, spec);
    if (!cmd) {
        return false;
    }

    struct command_family* family = nv_alloc(sizeof(struct command_family));
    assert(family);

    family->guilds = NULL;
    family->global = cmd;

    nv_map_insert(cm->families, nv_strdup(spec->name), family);
    return true;
}

bool cm_add_command(command_manager_t* cm, const struct command_spec* spec) {
    if (spec->guild_specific) {
        /* todo(nora): implement */
        return false;
    } else {
        return add_global_command(cm, spec);
    }
}

static const struct bot_command* find_command(const command_manager_t* cm,
                                              const struct discord_interaction* event) {
    const struct command_family* family;
    if (!nv_map_get(cm->families, event->data->name, (void**)&family)) {
        return NULL;
    }

    const struct bot_command* cmd;
    if (family->global) {
        cmd = family->global;
    } else {
        void* key = (void*)(size_t)event->guild_id;
        if (!nv_map_get(family->guilds, key, (void**)&cmd)) {
            cmd = NULL;
        }
    }

    return cmd;
}

bool cm_process_interaction(const command_manager_t* cm, void* user_context,
                            const struct discord_interaction* event) {
    const struct bot_command* cmd = find_command(cm, event);
    if (!cmd) {
        return false;
    }

    struct command_context context;
    context.user = cm->user;
    context.data = cmd->data;
    context.event = event;
    context.command = &cmd->command;
    context.user_context = user_context;

    cmd->callback(&context);
    return true;
}
