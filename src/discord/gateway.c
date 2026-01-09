#include "gateway.h"

#include "bot.h"
#include "dispatch.h"

#include "../core/websocket.h"

#include <json.h>

#include <log.h>

#include <assert.h>
#include <string.h>
#include <time.h>

#include <nyoravim/mem.h>
#include <nyoravim/util.h>

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
    OPCODE_DISPATCH = 0,
    OPCODE_HEARTBEAT = 1,
    OPCODE_IDENTIFY = 2,
    OPCODE_HELLO = 10,
    OPCODE_HEARTBEAT_ACK = 11,
};

struct gateway_session {
    bool started;

    char* id;
    char* resume_url;
};

typedef struct gateway {
    ws_t* ws;
    bot_t* bot;

    struct gateway_session session;

    bool has_sequence;
    uint64_t sequence;

    uint64_t heartbeat_interval_ms;
    struct timespec last_heartbeat;

    char* message_buffer;
    size_t buffer_size;
} gateway_t;

/* takes ownership of data */
static bool send_packet(ws_t* ws, int32_t opcode, json_object* data) {
    json_object* packet = json_object_new_object();
    assert(packet);

    json_object* opcode_obj = json_object_new_int(opcode);
    assert(opcode_obj);

    json_object_object_add(packet, "op", opcode_obj);
    json_object_object_add(packet, "d", data);

    const char* content = json_object_to_json_string(packet);
    size_t length = strlen(content);

    bool success = ws_send(ws, content, length, CURLWS_TEXT);
    json_object_put(packet);

    return success;
}

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
    log_debug("sending heartbeat");

    json_object* d =
        gw->has_sequence ? json_object_new_uint64(gw->sequence) : json_object_new_null();

    if (send_packet(gw->ws, OPCODE_HEARTBEAT, d)) {
        log_debug("sent heartbeat");
        return true;
    } else {
        log_error("failed to sent heartbeat");
        return false;
    }
}

static uint64_t get_intents(const bot_t* bot) {
    /* do we need any basic ones? dont think so */
    uint64_t intents = 0;

    const struct bot_callbacks* callbacks = bot_get_callbacks(bot);
    /* todo: set intents by checking callbacks */

    return intents;
}

static json_object* create_runtime_properties() {
    json_object* properties = json_object_new_object();
    assert(properties);

    /* im gonna assume linux; could detect */
    json_object* os = json_object_new_string("linux");
    assert(os);

    json_object* id = json_object_new_string("nyoravim");
    assert(id);

    json_object_object_add(properties, "browser", json_object_get(id));
    json_object_object_add(properties, "device", id);
    json_object_object_add(properties, "os", os);

    return properties;
}

static void identify_bot(gateway_t* gw) {
    json_object* identify_packet = json_object_new_object();
    assert(identify_packet);

    uint64_t intents = get_intents(gw->bot);
    json_object* intents_obj = json_object_new_uint64(intents);
    assert(intents_obj);

    const char* token = bot_get_token(gw->bot);
    json_object* token_obj = json_object_new_string(token);
    assert(token_obj);

    json_object_object_add(identify_packet, "token", token_obj);
    json_object_object_add(identify_packet, "intents", intents_obj);
    json_object_object_add(identify_packet, "properties", create_runtime_properties());

    if (send_packet(gw->ws, OPCODE_IDENTIFY, identify_packet)) {
        log_debug("sent identify packet to discord");
    } else {
        log_error("failed to identify!");
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

    identify_bot(gw);
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

    const char* type = NULL;
    json_object* type_obj = json_object_object_get(frame, "t");

    if (type_obj && json_object_get_type(type_obj) == json_type_string) {
        type = json_object_get_string(type_obj);
    }

    if (type) {
        log_trace("t: %s", type);
    }

    switch (opcode) {
    case OPCODE_DISPATCH:
        if (type) {
            log_trace("dispatching event %s", type);
            dispatch_event(gw, type, data);
        } else {
            log_warn("dispatch frame had no type; ignoring");
        }

        break;
    case OPCODE_HEARTBEAT:
        /* a server heartbeat expects an app heartbeat back right away */
        send_heartbeat(gw);
        break;
    case OPCODE_HELLO:
        handle_hello(data, gw);
        break;
    case OPCODE_HEARTBEAT_ACK:
        log_trace("heartbeat acknowledged");
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

/* manage message buffer while parsing data. note that this assumes that the websocket receives
 * completely valid json and memory will start leaking if it does not */
static json_object* parse_websocket_data(const char* data, size_t size, gateway_t* gw) {
    bool buffered;
    const char* parseable;

    /* message_buffer is gibberish unless buffer_size > 0 */
    if (gw->buffer_size > 0) {
        buffered = true;
        log_trace("previously buffered data; reallocating and extending buffer to parse");

        size_t new_size = gw->buffer_size + size;
        gw->message_buffer = nv_realloc(gw->message_buffer, new_size + 1);
        assert(gw->message_buffer);

        memcpy(gw->message_buffer + gw->buffer_size, data, size);
        gw->message_buffer[new_size] = '\0';

        gw->buffer_size = new_size;
        parseable = gw->message_buffer;
    } else {
        log_trace("no previously buffered data; not allocating anything");

        buffered = false;
        parseable = data;
    }

    json_object* parsed = json_tokener_parse(parseable);
    if (parsed) {
        /* if previously buffered, we can clear data now */
        if (buffered) {
            nv_free(gw->message_buffer);
            gw->buffer_size = 0;
        }
    } else {
        /* otherwise, if we failed to parse and it wasnt previously buffered, allocate a new buffer
         * for storage */
        if (!buffered) {
            /* buffer will be appended to regardless; we do not need a null terminator */
            gw->message_buffer = nv_alloc(size);
            memcpy(gw->message_buffer, data, size);

            gw->buffer_size = size;
        }
    }

    return parsed;
}

static void on_frame_received(void* user, const char* data, size_t size,
                              const struct curl_ws_frame* meta) {
    if ((meta->flags & CURLWS_TEXT) == 0) {
        return; /* dont care */
    }

    log_debug("packet received from gateway (len %zu)", size);

    json_object* parsed = parse_websocket_data(data, size, user);
    if (!parsed) {
        log_debug(
            "failed to parse received frame; assuming valid json and carrying over to next packet");

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
    gw->buffer_size = 0;

    memset(&gw->session, 0, sizeof(struct gateway_session));

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

    nv_free(gw->session.id);
    nv_free(gw->session.resume_url);

    ws_close(gw->ws, 1000, "bot triggered close");
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

void gateway_start_session(gateway_t* gw, const char* id, const char* resume_url) {
    if (gw->session.started) {
        log_warn("session already started; disregarding new id and url");
        return;
    }

    if (!id || !resume_url) {
        log_error("either id or gateway resume url not passed; session will be unable to restart!");
        return;
    }

    gw->session.started = true;
    gw->session.id = nv_strdup(id);
    gw->session.resume_url = nv_strdup(resume_url);

    log_debug("session started: %s", id);
}

bot_t* gateway_get_bot(const gateway_t* gw) { return gw->bot; }
