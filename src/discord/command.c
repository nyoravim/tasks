#include "command.h"

#include "bot.h"

#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include <assert.h>

#include <log.h>

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
}
