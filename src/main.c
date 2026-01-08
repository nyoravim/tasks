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
};

static bot_t* active_bot;
static void sigint_handler(int sig) {
    if (!active_bot) {
        return;
    }

    bot_stop(active_bot);
}

static void on_fill_form(const struct command_invocation_context* context) {
    const char* name = NULL;
    assert(nv_map_get(context->options, "name", (void**)name));

    char buffer[256];
    if (name) {
        snprintf(buffer, 256, "Hello %s!", name);
    } else {
        strncpy(buffer, "Hello... Sorry, didn't quite catch your name. Could you repeat that?",
                256);
    }

    log_info("Hello %s!", name);

    /*
    struct discord_component display;
    memset(&display, 0, sizeof(struct discord_component));
    display.type = DISCORD_COMPONENT_CONTAINER;
    display.label = "greeting";

    struct discord_components components;
    memset(&components, 0, sizeof(struct discord_components));
    components.array = &display;
    components.size = 1;

    struct discord_interaction_callback_data callback_data;
    memset(&callback_data, 0, sizeof(struct discord_interaction_callback_data));
    callback_data.custom_id = "test_modal";
    callback_data.title = "hello";
    callback_data.components = &components;

    struct discord_interaction_response params;
    memset(&params, 0, sizeof(struct discord_interaction_response));
    params.type = DISCORD_INTERACTION_MODAL;
    params.data = &callback_data;
    */
}

static void send_redis_command(struct bot_data* data, const char* command, char* buffer,
                               size_t length) {
    redisContext* ctx = db_get_context(data->db);

    redisReply* reply = redisCommand(ctx, "%s", command);
    snprintf(buffer, length, "redis: %s", reply->str);
    freeReplyObject(reply);
}

static void on_send_command(const struct command_invocation_context* context) {
    /*
    const char* command = NULL;
    for (int i = 0; i < context->event->data->options->size; i++) {
        const struct discord_application_command_interaction_data_option* option =
            &context->event->data->options->array[i];

        if (strcmp(option->name, "command") == 0) {
            command = option->value;
            break;
        }
    }

    size_t buffer_size = 256;
    char buffer[buffer_size + 1];
    buffer[buffer_size] = '\0';

    if (command) {
        send_redis_command(context->user, command, buffer, buffer_size);
    } else {
        strncpy(buffer, "no command specified", buffer_size);
    }

    struct discord_interaction_callback_data callback_data;
    memset(&callback_data, 0, sizeof(struct discord_interaction_callback_data));
    callback_data.content = buffer;

    struct discord_interaction_response params;
    memset(&params, 0, sizeof(struct discord_interaction_response));
    params.type = DISCORD_INTERACTION_CHANNEL_MESSAGE_WITH_SOURCE;
    params.data = &callback_data;
    */
}

static void create_command(const struct bot_data* data) {
    /*
    struct discord_application_command_option option;
    memset(&option, 0, sizeof(struct discord_application_command_option));
    option.type = DISCORD_APPLICATION_OPTION_STRING;
    option.name = "name";
    option.description = "your name";
    option.required = true;

    struct discord_application_command_options options;
    memset(&options, 0, sizeof(struct discord_application_command_options));
    options.size = 1;
    options.array = &option;

    struct command_spec spec;
    memset(&spec, 0, sizeof(struct command_spec));
    spec.name = "fill-form";
    spec.description = "basic fillable form";
    spec.default_permission = true;
    spec.options = &options;
    spec.type = DISCORD_APPLICATION_CHAT_INPUT;
    spec.callback = on_fill_form;

    cm_add_command(data->client.cm, &spec);

    spec.name = "send-command";
    spec.description = "send command to redis database";
    spec.callback = on_send_command;
    option.name = "command";
    option.description = "command to send";

    cm_add_command(data->client.cm, &spec);
    */
}

static void on_ready(const struct bot_context* context, const struct bot_ready_event* event) {
    struct bot_data* data = context->user;
    log_info("authenticated as user: %s#%s", event->user->username, event->user->discriminator);

    struct command_spec spec;
    memset(&spec, 0, sizeof(struct command_spec));

    spec.name = "fill-form";
    spec.bot = context->bot;
    spec.user = data;
    spec.callback = on_fill_form;

    data->client.fill_form = command_register(&spec);
    log_info("guh");
}

static void on_interaction(const struct bot_context* context, const struct interaction* event) {
    log_debug("interaction received");
    if (event->type == INTERACTION_TYPE_APPLICATION_COMMAND) {
        log_debug("command: %s", event->command_data->name);

        struct bot_data* data = context->user;
        command_invoke(data->client.fill_form, event);
    }
}

static bot_t* create_bot(void* user) {
    struct credentials* creds = credentials_read_from_path("bot.json");
    if (!creds) {
        return NULL;
    }

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
