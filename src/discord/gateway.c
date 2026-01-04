#include "gateway.h"

#include "bot.h"

#include "../core/websocket.h"

#include <json.h>

#include <log.h>

#include <assert.h>
#include <string.h>
#include <time.h>

#include <nyoravim/mem.h>

/* https://discord.com/developers/docs/events/gateway#identifying */
enum {
    INTENT_GUILDS = (1 << 0),
    INTENT_GUILD_MEMBERS = (1 << 1),
    INTENT_GUILD_MODERATION = (1 << 2),
    INTENT_GUILD_EXPRESSIONS = (1 << 3),
    INTENT_GUILD_INTEGRATIONS = (1 << 4),
    INTENT_GUILD_WEBHOOKS = (1 << 5),
    INTENT_GUILD_INVITES = (1 << 6),
    INTENT_GUILD_VOICE_STATES = (1 << 7),
    INTENT_GUILD_PRESENCES = (1 << 8),
    INTENT_GUILD_MESSAGES = (1 << 9),
    INTENT_GUILD_MESSAGE_REACTIONS = (1 << 10),
    INTENT_GUILD_MESSAGE_TYPING = (1 << 11),
    INTENT_DIRECT_MESSAGES = (1 << 12),
    INTENT_DIRECT_MESSAGE_REACTIONS = (1 << 13),
    INTENT_DIRECT_MESSAGE_TYPING = (1 << 14),
    INTENT_MESSAGE_CONTENT = (1 << 15),
    INTENT_GUILD_SCHEDULED_EVENTS = (1 << 16),
    INTENT_AUTO_MODERATION_CONFIGURATION = (1 << 20),
    INTENT_AUTO_MODERATION_EXECUTION = (1 << 21),
    INTENT_GUILD_MESSAGE_POLLS = (1 << 24),
    INTENT_DIRECT_MESSAGE_POLLS = (1 << 25),
};

enum {
    OPCODE_HEARTBEAT = 1,
    OPCODE_HELLO = 10,
    OPCODE_HEARTBEAT_ACK = 11,
};

typedef struct gateway {
    ws_t* ws;
    bot_t* bot;

    bool has_sequence;
    uint64_t sequence;

    uint64_t heartbeat_interval_ms;
    struct timespec last_heartbeat;
} gateway_t;

static void get_heartbeat_time(struct timespec* now) { clock_gettime(CLOCK_MONOTONIC, now); }

static json_object* create_heartbeat(bool has_sequence, uint64_t sequence) {
    json_object* data = json_object_new_object();
    assert(data);

    json_object* opcode = json_object_new_int(OPCODE_HEARTBEAT);
    assert(opcode);
    json_object_object_add(data, "op", opcode);

    json_object* d = has_sequence ? json_object_new_uint64(sequence) : json_object_new_null();
    json_object_object_add(data, "d", d);

    return data;
}

static bool send_heartbeat(gateway_t* gw) {
    json_object* heartbeat = create_heartbeat(gw->has_sequence, gw->sequence);

    const char* content = json_object_to_json_string(heartbeat);
    size_t length = strlen(content);

    log_debug("sending heartbeat");

    bool success = ws_send(gw->ws, content, length, CURLWS_TEXT);
    json_object_put(heartbeat);

    if (success) {
        log_info("sent heartbeat");
        return true;
    } else {
        log_error("failed to sent heartbeat");
        return false;
    }
}

static void handle_hello(const json_object* data, gateway_t* gw) {
    assert(data);

    log_info("discord says hello!");

    json_object* field = json_object_object_get(data, "heartbeat_interval");
    assert(field && json_object_get_type(field) == json_type_int);

    gw->heartbeat_interval_ms = json_object_get_uint64(field);
    log_debug("heartbeat interval: %" PRIu64 " ms", gw->heartbeat_interval_ms);

    /* discord expects heartbeat right away */
    send_heartbeat(gw);
    get_heartbeat_time(&gw->last_heartbeat);
}

static bool get_opcode(const json_object* data, int32_t* opcode) {
    json_object* field = json_object_object_get(data, "op");
    if (!field) {
        return false;
    }

    if (json_object_get_type(field) != json_type_int) {
        return false;
    }

    *opcode = json_object_get_int(field);
    return true;
}

static void handle_frame(const json_object* frame, gateway_t* gw) {
    int32_t opcode;
    if (!get_opcode(frame, &opcode)) {
        log_warn("no opcode on gateway frame! not handling");
        return;
    }

    log_trace("opcode: %" PRIi32, opcode);

    json_object* data = json_object_object_get(frame, "d");
    if (data && json_object_get_type(data) == json_type_null) {
        data = NULL;
    }

    if (!data) {
        log_debug("no data in frame");
    }

    switch (opcode) {
    case OPCODE_HEARTBEAT:
        /* a server heartbeat expects an app heartbeat back right away */
        send_heartbeat(gw);
        break;
    case OPCODE_HELLO:
        handle_hello(data, gw);
        break;
    }
}

static void read_sequence(const json_object* frame, gateway_t* gw) {
    json_object* sequence_field = json_object_object_get(frame, "s");
    if (!sequence_field || json_object_get_type(sequence_field) == json_type_null) {
        return;
    }

    gw->has_sequence = true;
    gw->sequence = json_object_get_uint64(sequence_field);
}

static uint64_t get_intents(const bot_t* bot) {
    /* do we need any basic ones? dont think so */
    uint64_t intents = 0;

    const struct bot_callbacks* callbacks = bot_get_callbacks(bot);
    /* todo: set intents by checking callbacks */

    return intents;
}

static void on_frame_received(void* user, const char* data, size_t size,
                              const struct curl_ws_frame* meta) {
    if ((meta->flags & CURLWS_TEXT) == 0) {
        return; /* dont care */
    }

    log_debug("frame received from gateway (len %zu)", size);

    /* going to assume its all in one frame */
    json_object* parsed = json_tokener_parse(data);
    if (!parsed) {
        log_warn("failed to parse frame from gateway: %s", data);
        return;
    }

    handle_frame(parsed, user);
    read_sequence(parsed, user);

    json_object_put(parsed);
}

gateway_t* gateway_open(const char* url, bot_t* bot) {
    gateway_t* gw = nv_alloc(sizeof(gateway_t));
    assert(gw);

    gw->bot = bot;
    gw->has_sequence = false;
    gw->heartbeat_interval_ms = 0;

    struct websocket_callbacks callbacks;
    callbacks.user = gw;
    callbacks.on_frame_received = on_frame_received;

    gw->ws = ws_open(url, &callbacks);
    if (!gw->ws) {
        log_error("failed to open websocket to url %s", url);

        nv_free(gw);
        return NULL;
    }

    return gw;
}

void gateway_close(gateway_t* gw) {
    if (!gw) {
        return;
    }

    /* todo: send close message */

    ws_close(gw->ws);
    nv_free(gw);
}

static void check_heartbeat_timer(gateway_t* gw) {
    if (gw->heartbeat_interval_ms == 0) {
        return;
    }

    struct timespec now;
    get_heartbeat_time(&now);

    const struct timespec* last = &gw->last_heartbeat;
    int64_t diff_seconds = (int64_t)now.tv_sec - (int64_t)last->tv_sec;
    int64_t diff_nano = (int64_t)now.tv_nsec - (int64_t)last->tv_nsec;
    int64_t diff_ms = diff_seconds * 1e3 + diff_nano / 1e6;

    if (diff_ms >= gw->heartbeat_interval_ms) {
        log_trace("timeout elapsed; sending heartbeat");
        send_heartbeat(gw);

        memcpy(&gw->last_heartbeat, &now, sizeof(struct timespec));
    }
}

void gateway_poll(gateway_t* gw) {
    ws_poll(gw->ws);
    check_heartbeat_timer(gw);
}
