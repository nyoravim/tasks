#ifndef _BOT_H
#define _BOT_H

#include <stdint.h>

typedef struct bot bot_t;

struct bot_context {
    void* user;
    bot_t* bot;
};

struct bot_error {
    int64_t code;
    const char* response;
};

struct bot_callbacks {
    void* user;

    void (*on_ready)(const struct bot_context* context);

    /* todo: resolve type */
    void (*on_interaction)(const struct bot_context* context, const void* event);

    void (*on_error)(const struct bot_context* context, const struct bot_error* error);
};

struct bot_spec {
    uint32_t api;

    const struct credentials* creds;
    const struct bot_callbacks* callbacks;
};

bot_t* bot_create(const struct bot_spec* spec);
void bot_destroy(bot_t* bot);

const char* bot_get_token(const bot_t* bot);
uint64_t bot_get_app_id(const bot_t* bot);
const struct bot_callbacks* bot_get_callbacks(const bot_t* bot);

void bot_start(bot_t* bot);
void bot_stop(bot_t* bot);

#endif
