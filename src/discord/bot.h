#ifndef _BOT_H
#define _BOT_H

#include <stdint.h>

#include <json.h>

typedef struct bot bot_t;

struct bot_context {
    void* user;
    bot_t* bot;
};

struct bot_error {
    int64_t code;

    /* not owned by error callback! make reference if necessary */
    json_object* response;
};

struct bot_ready_event {
    const struct user* user;
    const struct application* app;

    const char* session_id;
};

/* from types/interaction.h */
struct interaction;

struct bot_callbacks {
    void* user;

    void (*on_ready)(const struct bot_context* context, const struct bot_ready_event* event);
    void (*on_interaction)(const struct bot_context* context, const struct interaction* event);

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

uint32_t bot_get_api_version(const bot_t* bot);

void bot_start(bot_t* bot);
void bot_stop(bot_t* bot);

/* send a synchronous request to the discord REST api */
json_object* bot_send_api_request(bot_t* bot, const char* path, const char* method,
                                  json_object* body);

#endif
