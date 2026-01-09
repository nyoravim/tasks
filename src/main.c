#include "discord/bot.h"
#include "discord/credentials.h"
#include "discord/command.h"

#include "discord/types/user.h"
#include "discord/types/interaction.h"

#include "core/database.h"

#include <log.h>

#include <signal.h>
#include <string.h>
#include <assert.h>

#include <hiredis/hiredis.h>

struct client {
    bot_t* bot;
    command_t* fill_form;
};

struct bot_data {
    struct client client;
    database_t* db;

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
        snprintf(buffer, 256, "Hello %s!", name);
    } else {
        strncpy(buffer, "Hello... Sorry, didn't quite catch your name. Could you repeat that?",
                256);
    }

    struct message_response response;
    response.content = buffer;

    interaction_respond_with_message(context->interaction, data->client.bot, &response);
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

    data->client.fill_form = command_register(&spec);
}

static void on_interaction(const struct bot_context* context, const struct interaction* event) {
    log_debug("interaction received");
    if (event->type == INTERACTION_TYPE_APPLICATION_COMMAND) {
        log_debug("command: %s", event->command_data->name);

        struct bot_data* data = context->user;
        command_invoke(data->client.fill_form, event);
    }
}

static bot_t* create_bot(struct bot_data* user) {
    struct credentials* creds = credentials_read_from_path("bot.json");
    if (!creds) {
        return NULL;
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

    bot_t* bot = bot_create(&spec);
    credentials_free(creds);

    return bot;
}

static bool initialize_client(struct client* client, void* user) {
    memset(client, 0, sizeof(struct client));

    client->bot = create_bot(user);
    if (!client->bot) {
        return false;
    }

    return true;
}

int main(int argc, const char** argv) {
    bool initialized = false;

    struct bot_data data;
    data.db = db_connect("127.0.0.1", 6379);

    if (data.db) {
        active_bot = NULL;
        __sighandler_t prev_handler = signal(SIGINT, sigint_handler);

        if (initialize_client(&data.client, &data)) {
            initialized = true;

            active_bot = data.client.bot;
            bot_start(active_bot);
        }

        signal(SIGINT, prev_handler);
        active_bot = NULL;

        bot_destroy(data.client.bot);
    }

    db_close(data.db);
    return initialized ? 0 : 1;
}
