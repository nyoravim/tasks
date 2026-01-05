#include "dispatch.h"

#include "bot.h"

#include <log.h>

#include <string.h>
#include <ctype.h>

#include <nyoravim/mem.h>

static void on_ready(bot_t* bot, const json_object* data) {
    /* todo: handle session resume url etc */

    const struct bot_callbacks* callbacks = bot_get_callbacks(bot);
    if (callbacks->on_ready) {
        struct bot_context bc;
        bc.bot = bot;
        bc.user = callbacks->user;

        callbacks->on_ready(&bc);
    }
}

/* assumes type is uppercase */
static void do_dispatch(bot_t* bot, const char* type, const json_object* data) {
    if (strcmp(type, "READY") == 0) {
        on_ready(bot, data);
        return;
    }

    /* todo: more */
}

void dispatch_event(bot_t* bot, const char* type, const json_object* data) {
    size_t len = strlen(type);
    char* type_upper = nv_alloc(len + 1);
    type_upper[len] = '\0';

    for (size_t i = 0; i < len; i++) {
        type_upper[i] = toupper(type[i]);
    }

    do_dispatch(bot, type_upper, data);
    nv_free(type_upper);
}
