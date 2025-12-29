#include "bot.h"
#include "credentials.h"

#include <concord/discord.h>
#include <concord/log.h>

#include <signal.h>
#include <string.h>

static void sigint_handler(int sig) { discord_shutdown_all(); }

static void create_command(const struct bot_context* context) {
    struct discord_application_command_option option;
    memset(&option, 0, sizeof(struct discord_application_command_option));
    option.type = DISCORD_APPLICATION_OPTION_STRING;
    option.name = "name";
    option.description = "Your name";
    option.required = true;

    struct discord_application_command_options options;
    memset(&options, 0, sizeof(struct discord_application_command_options));
    options.size = 1;
    options.array = &option;

    struct discord_create_global_application_command params;
    memset(&params, 0, sizeof(discord_create_global_application_command));
    params.name = "fill-form";
    params.description = "Basic fillable form";
    params.default_permission = true;
    params.options = &options;
    params.type = 1;

    struct discord_application_command app_cmd;
    memset(&app_cmd, 0, sizeof(struct discord_application_command));

    struct discord_ret_application_command ret;
    memset(&ret, 0, sizeof(struct discord_ret_application_command));
    ret.sync = &app_cmd;

    uint64_t app_id = bot_get_app_id(context->bot);
    discord_create_global_application_command(context->client, app_id, &params, &ret);
}

static void on_ready(const struct bot_context* context, const struct discord_ready* event) {
    log_info("authenticated: %s#%s\n", event->user->username, event->user->discriminator);

    create_command(context);
}

static void reply_fail(struct discord* client, struct discord_response* response) {
    log_error("%s", discord_strerror(response->code, client));
}

static void on_interaction(const struct bot_context* context,
                           const struct discord_interaction* event) {
    if (event->type != DISCORD_INTERACTION_APPLICATION_COMMAND) {
        return; /* dont care */
    }

    if (strcmp(event->data->name, "fill-form") != 0) {
        return; /* dont care */
    }

    const char* name = NULL;
    for (int i = 0; i < event->data->options->size; i++) {
        const struct discord_application_command_interaction_data_option* option =
            &event->data->options->array[i];

        if (strcmp(option->name, "name") == 0) {
            name = option->value;
            break;
        }
    }

    char buffer[256];
    if (name) {
        snprintf(buffer, 256, "Hello %s!", name);
    } else {
        strncpy(buffer, "Hello... Sorry, didn't quite catch your name. Could you repeat that?",
                256);
    }

    struct discord_interaction_callback_data data;
    memset(&data, 0, sizeof(struct discord_interaction_callback_data));
    data.content = buffer;

    struct discord_interaction_response params;
    memset(&params, 0, sizeof(struct discord_interaction_response));
    params.type = DISCORD_INTERACTION_CHANNEL_MESSAGE_WITH_SOURCE;
    params.data = &data;

    struct discord_ret_interaction_response ret;
    memset(&ret, 0, sizeof(struct discord_ret_interaction_response));
    ret.fail = reply_fail;

    discord_create_interaction_response(context->client, event->id, event->token, &params, &ret);
}

static bot_t* create_bot() {
    struct credentials* creds = credentials_read_from_path("bot.json");
    if (!creds) {
        return NULL;
    }

    struct bot_callbacks callbacks;
    callbacks.on_ready = on_ready;
    callbacks.on_interaction = on_interaction;

    struct bot_spec spec;
    spec.creds = creds;
    spec.callbacks = &callbacks;

    bot_t* bot = bot_create(&spec);
    credentials_free(creds);

    return bot;
}

int main(int argc, const char** argv) {
    __sighandler_t prev_handler = signal(SIGINT, sigint_handler);

    bot_t* bot = create_bot();
    if (bot) {
        bot_start(bot);
        bot_destroy(bot);
    }

    signal(SIGINT, prev_handler);
    return bot ? 0 : 1;
}
