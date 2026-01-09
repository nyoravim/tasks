#include "discord/bot.h"
#include "discord/credentials.h"
#include "discord/command.h"
#include "discord/component.h"

#include "discord/types/user.h"
#include "discord/types/interaction.h"

#include "core/database.h"

#include <log.h>

#include <signal.h>
#include <string.h>
#include <assert.h>

#include <hiredis/hiredis.h>

#include <nyoravim/map.h>
#include <nyoravim/util.h>

static bool string_keys_equal(void* user, const void* lhs, const void* rhs) {
    return strcmp(lhs, rhs) == 0;
}

static size_t hash_string(void* user, const void* key) { return nv_hash_string(key); }

static void free_command(void* user, void* value) { command_free(value); }

struct bot_data {
    redisContext* db;

    bot_t* bot;

    /* keys owned by values */
    nv_map_t* commands;

    uint64_t guild_scope;
};

static bot_t* active_bot;
static void sigint_handler(int sig) {
    if (!active_bot) {
        return;
    }

    bot_stop(active_bot);
}

static void on_fill_form(const struct command_invocation_context* context) {
    struct bot_data* data = context->user;

    const char* name = NULL;
    assert(nv_map_get(context->options, "name", (void**)&name));

    char buffer[256];
    if (name) {
        snprintf(buffer, sizeof(buffer), "hello %s!", name);
    } else {
        strncpy(buffer, "hello... sorry, didn't quite catch your name. could you repeat that?",
                sizeof(buffer));
    }

    struct component button;
    memset(&button, 0, sizeof(struct component));
    button.type = COMPONENT_TYPE_BUTTON;
    button.button.style = BUTTON_STYLE_PRIMARY;
    button.button.data = name;
    button.button.data_size = strlen(name) + 1;
    button.button.label = "boop";

    struct component comp[2];
    memset(comp, 0, sizeof(comp));

    comp[0].type = COMPONENT_TYPE_TEXT_DISPLAY;
    comp[0].text_display.content = buffer;

    comp[1].type = COMPONENT_TYPE_ACTION_ROW;
    comp[1].action_row.num_children = 1;
    comp[1].action_row.children = &button;

    struct message_response response;
    memset(&response, 0, sizeof(struct message_response));

    response.flags |= MESSAGE_IS_COMPONENTS_V2;
    response.num_components = 2;
    response.components = comp;

    interaction_respond_with_message(context->interaction, data->bot, &response);
}

static void register_command(struct bot_data* data, const struct command_spec* spec) {
    if (nv_map_contains(data->commands, spec->name)) {
        log_error("command %s already registered!", spec->name);
        return;
    }

    command_t* cmd = command_register(spec);
    if (!cmd) {
        log_error("failed to register command %s", spec->name);
        return;
    }

    const char* name = command_get_name(cmd);
    assert(nv_map_insert(data->commands, (void*)name, cmd));
}

static void on_ready(const struct bot_context* context, const struct bot_ready_event* event) {
    struct bot_data* data = context->user;
    log_info("authenticated as user: %s#%s", event->user->username, event->user->discriminator);

    struct command_option_spec option;
    memset(&option, 0, sizeof(struct command_option_spec));
    option.name = "name";
    option.description = "your name";
    option.type = COMMAND_OPTION_TYPE_STRING;
    option.required = true;

    struct command_spec spec;
    memset(&spec, 0, sizeof(struct command_spec));

    spec.name = "fill-form";
    spec.description = "fill basic chat form";
    spec.bot = context->bot;
    spec.user = data;
    spec.callback = on_fill_form;
    spec.type = COMMAND_TYPE_CHAT_INPUT;
    spec.num_options = 1;
    spec.options = &option;
    spec.guild_id = data->guild_scope;

    register_command(data, &spec);
}

static void on_interaction(const struct bot_context* context, const struct interaction* event) {
    log_debug("interaction received");
    if (event->type == INTERACTION_TYPE_APPLICATION_COMMAND) {
        command_t* cmd;

        struct bot_data* data = context->user;
        if (!nv_map_get(data->commands, event->command_data->name, (void**)&cmd)) {
            log_error("command not found: %s", event->command_data->name);
            return;
        }

        if (!command_invoke(cmd, event)) {
            log_error("failed to invoke command: %s", event->command_data->name);
        }

        return;
    }

    if (event->type == INTERACTION_TYPE_MESSAGE_COMPONENT) {
    }
}

static bool create_bot(struct bot_data* user) {
    struct credentials* creds = credentials_read_from_path("bot.json");
    if (!creds) {
        return false;
    }

    user->guild_scope = creds->guild_scope;

    struct bot_callbacks callbacks;
    memset(&callbacks, 0, sizeof(struct bot_callbacks));

    callbacks.user = user;
    callbacks.on_ready = on_ready;
    callbacks.on_interaction = on_interaction;

    struct bot_spec spec;
    memset(&spec, 0, sizeof(struct bot_spec));

    spec.creds = creds;
    spec.callbacks = &callbacks;

    user->bot = bot_create(&spec);
    credentials_free(creds);

    return user->bot != NULL;
}

static bool initialize_client(struct bot_data* bot) {
    memset(bot, 0, sizeof(struct bot_data));

    bot->db = redisConnect("127.0.0.1", 6379);
    if (bot->db->err != 0) {
        log_error("failed to connect to redis database: %s", bot->db->errstr);
        return false;
    }

    if (!create_bot(bot)) {
        log_error("failed to create discord client!");
        return false;
    }

    struct nv_map_callbacks callbacks;
    memset(&callbacks, 0, sizeof(struct nv_map_callbacks));

    callbacks.free_value = free_command;
    callbacks.hash = hash_string;
    callbacks.equals = string_keys_equal;

    bot->commands = nv_map_alloc(64, &callbacks);
    assert(bot->commands);

    return true;
}

int main(int argc, const char** argv) {
    bool initialized = false;

    struct bot_data data;
    if (initialize_client(&data)) {
        initialized = true;

        active_bot = data.bot;
        __sighandler_t prev_handler = signal(SIGINT, sigint_handler);

        bot_start(active_bot);

        signal(SIGINT, prev_handler);
        active_bot = NULL;
    }

    nv_map_free(data.commands);
    bot_destroy(data.bot);
    redisFree(data.db);

    return initialized ? 0 : 1;
}
