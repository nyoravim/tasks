#include "bot.h"
#include "credentials.h"
#include "command_manager.h"

#include <concord/discord.h>
#include <concord/log.h>

#include <signal.h>
#include <string.h>

struct bot_data {
    bot_t* bot;
    command_manager_t* cm;
};

static bot_t* active_bot;
static void sigint_handler(int sig) {
    if (!active_bot) {
        return;
    }

    bot_stop(active_bot);
}

static void reply_fail(struct discord* client, struct discord_response* response) {
    log_error("%s", discord_strerror(response->code, client));
}

static void on_fill_form(const struct command_context* context) {
    const char* name = NULL;
    for (int i = 0; i < context->event->data->options->size; i++) {
        const struct discord_application_command_interaction_data_option* option =
            &context->event->data->options->array[i];

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

    struct discord_interaction_callback_data callback_data;
    memset(&callback_data, 0, sizeof(struct discord_interaction_callback_data));
    callback_data.content = buffer;

    struct discord_interaction_response params;
    memset(&params, 0, sizeof(struct discord_interaction_response));
    params.type = DISCORD_INTERACTION_CHANNEL_MESSAGE_WITH_SOURCE;
    params.data = &callback_data;

    struct discord_ret_interaction_response ret;
    memset(&ret, 0, sizeof(struct discord_ret_interaction_response));
    ret.fail = reply_fail;

    struct bot_data* data = context->user;
    struct discord* client = bot_get_client(data->bot);

    discord_create_interaction_response(client, context->event->id, context->event->token, &params,
                                        &ret);
}

static void create_command(const struct bot_data* data) {
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

    cm_add_command(data->cm, &spec);
}

static void on_ready(const struct bot_context* context, const struct discord_ready* event) {
    log_info("authenticated: %s#%s\n", event->user->username, event->user->discriminator);

    create_command(context->user);
}

static void on_interaction(const struct bot_context* context,
                           const struct discord_interaction* event) {
    struct bot_data* data = context->user;
    if (cm_process_interaction(data->cm, NULL, event)) {
        /* command handled */
        return;
    }

    /* todo(nora): more */
}

static bot_t* create_bot(struct bot_data* data) {
    struct credentials* creds = credentials_read_from_path("bot.json");
    if (!creds) {
        return NULL;
    }

    struct bot_callbacks callbacks;
    callbacks.user = data;
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
    active_bot = NULL;
    __sighandler_t prev_handler = signal(SIGINT, sigint_handler);

    struct bot_data data;
    data.bot = create_bot(&data);
    data.cm = cm_new(&data, bot_get_client(data.bot), bot_get_app_id(data.bot));

    active_bot = data.bot;
    if (data.bot) {
        bot_start(data.bot);
    }

    signal(SIGINT, prev_handler);
    active_bot = NULL;

    bot_destroy(data.bot);
    cm_destroy(data.cm);

    return data.bot ? 0 : 1;
}
