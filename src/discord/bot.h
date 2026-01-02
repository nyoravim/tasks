#ifndef _BOT_H
#define _BOT_H

#include <stdint.h>

typedef struct bot bot_t;

struct bot_context {
    void* user;
    bot_t* bot;
    struct discord* client;
};

struct bot_callbacks {
    void* user;

    /*
    void (*on_ready)(const struct bot_context* context, const struct discord_ready* event);
    void (*on_interaction)(const struct bot_context* context,
                           const struct discord_interaction* event);

    void (*on_success)(const struct bot_context* context, const struct discord_response* response);
    void (*on_error)(const struct bot_context* context, const struct discord_response* response);
    */
};

struct bot_spec {
    uint32_t api;

    const struct credentials* creds;
    const struct bot_callbacks* callbacks;
};

bot_t* bot_create(const struct bot_spec* spec);
void bot_destroy(bot_t* bot);

struct discord* bot_get_client(const bot_t* bot);
uint64_t bot_get_app_id(const bot_t* bot);

void bot_start(bot_t* bot);
void bot_stop(bot_t* bot);

#endif
