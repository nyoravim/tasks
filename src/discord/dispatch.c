#include "dispatch.h"

#include "bot.h"

#include "application.h"
#include "user.h"

#include <log.h>

#include <string.h>
#include <ctype.h>

#include <nyoravim/mem.h>
#include <nyoravim/util.h>

struct ready_frame {
    bool has_app;
    struct application app;

    bool has_user;
    struct user user;

    char* resume_gateway_url;
    char* session_id;
};

static void parse_ready_frame(struct ready_frame* event, const json_object* data) {
    json_object* field = json_object_object_get(data, "user");
    event->has_user = user_parse(&event->user, field);

    field = json_object_object_get(data, "application");
    event->has_app = application_parse(&event->app, field);

    field = json_object_object_get(data, "session_id");
    if (field && json_object_get_type(field) == json_type_string) {
        const char* session_id = json_object_get_string(field);
        log_trace("session id: %s", session_id);

        event->session_id = nv_strdup(session_id);
    } else {
        event->session_id = NULL;
    }

    field = json_object_object_get(data, "resume_gateway_url");
    if (field && json_object_get_type(field) == json_type_string) {
        const char* url = json_object_get_string(field);
        log_trace("resume url: %s", url);

        event->resume_gateway_url = nv_strdup(url);
    } else {
        event->resume_gateway_url = NULL;
    }
}

static void on_ready(bot_t* bot, const json_object* data) {
    struct ready_frame ready;
    parse_ready_frame(&ready, data);

    const struct bot_callbacks* callbacks = bot_get_callbacks(bot);
    if (callbacks->on_ready) {
        struct bot_context bc;
        bc.bot = bot;
        bc.user = callbacks->user;

        struct bot_ready_event event;
        event.user = ready.has_user ? &ready.user : NULL;
        event.app = ready.has_app ? &ready.app : NULL;
        event.session_id = ready.session_id;

        callbacks->on_ready(&bc, &event);
    }

    application_cleanup(&ready.app);
    user_cleanup(&ready.user);

    nv_free(ready.session_id);
    nv_free(ready.resume_gateway_url);
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
