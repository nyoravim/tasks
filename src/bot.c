#include "bot.h"
#include "credentials.h"

#include <concord/discord.h>
#include <concord/log.h>

#include <string.h>
#include <assert.h>

#include <nyoravim/map.h>
#include <nyoravim/mem.h>

typedef struct bot {
    struct discord* client;
    struct credentials* creds;

    struct bot_callbacks callbacks;
} bot_t;

static void make_bot_context(bot_t* bot, struct bot_context* context) {
    context->bot = bot;
    context->client = bot->client;
    context->user = bot->callbacks.user;
}

static void bot_on_ready(struct discord* client, const struct discord_ready* event) {
    bot_t* bot = discord_get_data(client);
    if (!bot->callbacks.on_ready) {
        return;
    }

    struct bot_context context;
    make_bot_context(bot, &context);
    bot->callbacks.on_ready(&context, event);
}

static void bot_on_interaction(struct discord* client, const struct discord_interaction* event) {
    bot_t* bot = discord_get_data(client);
    if (!bot->callbacks.on_interaction) {
        return;
    }

    struct bot_context context;
    make_bot_context(bot, &context);
    bot->callbacks.on_interaction(&context, event);
}

static struct discord* create_discord_client(const char* token) {
    struct discord_config config;
    memset(&config, 0, sizeof(struct discord_config));

    config.log.color = true;
    config.log.level = LOGMOD_LEVEL_TRACE;

    /* concord owns the token string fsr */
    config.token = strdup(token); /* NOT nv_strdup! */

    return discord_from_config(&config);
}

bot_t* bot_create(const struct bot_spec* spec) {
    struct credentials* creds = credentials_dup(spec->creds);
    log_info("authenticating as app %" PRIu64, creds->app_id);

    struct discord* client = create_discord_client(creds->token);
    if (!client) {
        log_error("failed to connect client!");
        return NULL;
    }

    discord_set_on_ready(client, bot_on_ready);
    discord_set_on_interaction_create(client, bot_on_interaction);

    bot_t* bot = nv_alloc(sizeof(bot_t));
    assert(bot);

    discord_set_data(client, bot);

    memcpy(&bot->callbacks, spec->callbacks, sizeof(struct bot_callbacks));
    bot->creds = creds;
    bot->client = client;

    return bot;
}

void bot_destroy(bot_t* bot) {
    if (!bot) {
        return;
    }

    discord_cleanup(bot->client);

    credentials_free(bot->creds);
    nv_free(bot);
}

struct discord* bot_get_client(const bot_t* bot) { return bot->client; }
uint64_t bot_get_app_id(const bot_t* bot) { return bot->creds->app_id; }

void bot_start(bot_t* bot) { discord_run(bot->client); }
void bot_stop(bot_t* bot) { discord_shutdown(bot->client); }
