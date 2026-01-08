#include "command.h"

#include "bot.h"

#include "types/interaction.h"

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

#include <log.h>

#include <nyoravim/mem.h>
#include <nyoravim/util.h>

typedef struct command {
    char* name;

    uint64_t app_id;
    uint64_t guild_id;

    void* user;
    command_invocation_callback callback;

    uint32_t type;
} command_t;

static void make_command_endpoint(char* buffer, size_t max_length, uint64_t app_id,
                                  uint64_t guild_id) {
    if (guild_id > 0) {
        snprintf(buffer, max_length, "/applications/%" PRIu64 "/guilds/%" PRIu64 "/commands",
                 app_id, guild_id);
    } else {
        snprintf(buffer, max_length, "/applications/%" PRIu64 "/commands", app_id);
    }
}

static json_object* create_command_payload(const struct command_spec* spec) {
    json_object* body = json_object_new_object();
    assert(body);

    return body;
}

static bool register_command(const struct command_spec* spec) {
    uint64_t app_id = bot_get_app_id(spec->bot);

    char path[256];
    make_command_endpoint(path, sizeof(path), app_id, spec->guild_id);

    json_object* body = create_command_payload(spec);
    json_object* response = bot_send_api_request(spec->bot, path, "POST", NULL);
    json_object_put(body);

    if (!response) {
        log_error("failed to register command!");
        return false;
    }

    /* do we need to do anything with the received data? */

    json_object_put(response);
    return true;
}

command_t* command_register(const struct command_spec* spec) {
    if (!register_command(spec)) {
        return NULL;
    }

    command_t* cmd = nv_alloc(sizeof(command_t));
    assert(cmd);

    cmd->name = nv_strdup(spec->name);
    cmd->app_id = bot_get_app_id(spec->bot);
    cmd->guild_id = spec->guild_id;

    cmd->user = spec->user;
    cmd->callback = spec->callback;

    cmd->type = spec->type;

    return cmd;
}

void command_free(command_t* cmd) {
    if (!cmd) {
        return;
    }

    nv_free(cmd->name);
    nv_free(cmd);
}

static void command_option_data_free_callback(void* user, void* value) { nv_free(value); }

static bool strings_equal(void* user, const void* lhs, const void* rhs) {
    return strcmp(lhs, rhs) == 0;
}

static size_t hash_string(void* user, const void* key) { return nv_hash_string(key); }

bool command_invoke(command_t* cmd, const struct interaction* event) {
    if (event->application_id != cmd->app_id) {
        log_warn("app ids dont match; command will be ignored");
        return false;
    }

    switch (event->type) {
    case INTERACTION_TYPE_APPLICATION_COMMAND: {
        struct interaction_command_data* data = event->command_data;

        struct command_invocation_context ic;
        ic.cmd = cmd;
        ic.user = cmd->user;

        struct nv_map_callbacks callbacks;
        callbacks.equals = strings_equal;
        callbacks.hash = hash_string;
        callbacks.free_key = command_option_data_free_callback;
        callbacks.free_value = command_option_data_free_callback;

        nv_map_t* options = nv_map_alloc(64, &callbacks);
    } break;
    default:
        log_warn("this event does not concern this command; disregarding");
        return false;
    }

    return true;
}
