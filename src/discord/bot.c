#include "bot.h"
#include "credentials.h"

#include "../core/rest.h"
#include "../core/websocket.h"

#include <log.h>

#include <json.h>

#include <string.h>
#include <assert.h>
#include <inttypes.h>
#include <unistd.h>
#include <time.h>

#include <nyoravim/map.h>
#include <nyoravim/mem.h>
#include <nyoravim/util.h>

enum {
    OPCODE_HEARTBEAT = 1,
    OPCODE_HELLO = 10,
    OPCODE_HEARTBEAT_ACK = 11,
};

typedef struct bot {
    struct credentials* creds;
    struct bot_callbacks callbacks;

    rest_t* rest;
    ws_t* gateway;
    uint32_t api;

    bool has_sequence;
    uint64_t sequence;

    uint64_t heartbeat_interval_ms;
    struct timespec last_heartbeat;

    bool running;
} bot_t;

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

static bool send_heartbeat(bot_t* bot) {
    json_object* heartbeat = create_heartbeat(bot->has_sequence, bot->sequence);

    const char* content = json_object_to_json_string(heartbeat);
    size_t length = strlen(content);

    log_debug("sending heartbeat");

    bool success = ws_send(bot->gateway, content, length, CURLWS_TEXT);
    json_object_put(heartbeat);

    clock_gettime(CLOCK_MONOTONIC, &bot->last_heartbeat);

    if (success) {
        log_info("sent heartbeat");
        return true;
    } else {
        log_error("failed to sent heartbeat");
        return false;
    }
}

static void create_api_url(const char* path, uint32_t api_version, char* buffer,
                           size_t max_length) {
    /* spaghetti */
    snprintf(buffer, max_length, "https://discord.com/api/v%" PRIu32 "%s", api_version, path);
}

struct discord_rest_data {
    uint32_t api;
    const char* path;

    const char* method;
    json_object* body;

    const char* token;
};

static int64_t send_discord_rest_request(rest_t* rest, const struct discord_rest_data* data,
                                         json_object** resp) {
    char url[256];
    create_api_url(data->path, data->api, url, sizeof(url));

    char auth_header[256];
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bot %s", data->token);

    const char* header = auth_header;

    struct http_request req;
    req.url = url;
    req.method = data->method;
    req.num_headers = 1;
    req.headers = &header;

    if (data->body) {
        /* owned by json object */
        const char* body = json_object_to_json_string(data->body);

        req.data = body;
        req.size = strlen(body);
    } else {
        req.data = NULL;
        req.size = 0;
    }

    struct http_response response;
    if (!rest_send_await(rest, &req, &response)) {
        return -1;
    }

    if (resp) {
        *resp = json_tokener_parse(response.content);
    }

    nv_free(response.content);
    return response.status;
}

static char* get_url_from_gateway_response(const json_object* object) {
    if (!object) {
        return NULL;
    }

    json_object* url_field;
    if (!json_object_object_get_ex(object, "url", &url_field)) {
        return NULL;
    }

    const char* url = json_object_get_string(url_field);
    if (!url) {
        return NULL;
    }

    return nv_strdup(url);
}

static char* get_gateway_url(rest_t* rest, const char* token, uint32_t api) {
    struct discord_rest_data data;
    data.path = "/gateway/bot";
    data.method = "GET";
    data.token = token;
    data.body = NULL;
    data.api = api;

    json_object* response;
    int64_t status = send_discord_rest_request(rest, &data, &response);

    if (status < 0) {
        log_error("somehow failed to talk to discord retrieving gateway url");
        return NULL;
    }

    if (status != 200) {
        log_error("discord authentication failed (%" PRIi64 "): %s", status,
                  response ? json_object_to_json_string(response) : "<null>");

        return NULL;
    }

    char* url = get_url_from_gateway_response(response);
    json_object_put(response);

    return url;
}

static ws_t* open_gateway(rest_t* rest, const char* token, uint32_t api,
                          const struct websocket_callbacks* callbacks) {
    char* url = get_gateway_url(rest, token, api);
    if (!url) {
        log_error("failed to retrieve gateway url from discord!");
        return NULL;
    }

    log_debug("discord gateway url: %s", url);

    ws_t* gateway = ws_open(url, callbacks);
    nv_free(url);

    if (!gateway) {
        log_error("failed to open gateway websocket!");
        return NULL;
    }

    return gateway;
}

static void handle_hello(const json_object* data, bot_t* bot) {
    assert(data);

    log_info("discord says hello!");

    json_object* field = json_object_object_get(data, "heartbeat_interval");
    assert(field && json_object_get_type(field) == json_type_int);

    bot->heartbeat_interval_ms = json_object_get_uint64(field);
    log_debug("heartbeat interval: %" PRIu64 " ms", bot->heartbeat_interval_ms);

    /* discord expects heartbeat right away */
    send_heartbeat(bot);
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

static void handle_frame(const json_object* frame, bot_t* bot) {
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
        send_heartbeat(bot);
        break;
    case OPCODE_HELLO:
        handle_hello(data, bot);
        break;
    }
}

static void read_sequence(const json_object* frame, bot_t* bot) {
    json_object* sequence_field = json_object_object_get(frame, "s");
    if (!sequence_field || json_object_get_type(sequence_field) == json_type_null) {
        return;
    }

    bot->has_sequence = true;
    bot->sequence = json_object_get_uint64(sequence_field);
}

static void on_frame_received(void* user, const char* data, size_t size,
                              const struct curl_ws_frame* meta) {
    if (!(meta->flags & CURLWS_TEXT)) {
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

bot_t* bot_create(const struct bot_spec* spec) {
    struct credentials* creds = credentials_dup(spec->creds);
    log_info("authenticating as app %" PRIu64, creds->app_id);

    /* https://discord.com/developers/docs/reference#api-versioning */
    uint32_t api = spec->api > 0 ? spec->api : 10;

    rest_t* rest = rest_init();
    if (!rest) {
        log_error("failed to initialize curl!");
        return NULL;
    }

    bot_t* bot = nv_alloc(sizeof(bot_t));
    assert(bot);

    struct websocket_callbacks callbacks;
    callbacks.user = bot;
    callbacks.on_frame_received = on_frame_received;

    ws_t* gateway = open_gateway(rest, creds->token, api, &callbacks);
    if (!gateway) {
        log_error("failed to open discord gateway!");

        rest_shutdown(rest);
        return NULL;
    }

    memcpy(&bot->callbacks, spec->callbacks, sizeof(struct bot_callbacks));
    bot->creds = creds;

    bot->rest = rest;
    bot->gateway = gateway;
    bot->api = api;

    bot->has_sequence = false;
    bot->sequence = 0;
    bot->heartbeat_interval_ms = 0;

    bot->running = false;
    return bot;
}

void bot_destroy(bot_t* bot) {
    if (!bot) {
        return;
    }

    ws_close(bot->gateway);
    rest_shutdown(bot->rest);

    credentials_free(bot->creds);
    nv_free(bot);
}

struct discord* bot_get_client(const bot_t* bot) { return /* bot->client; */ NULL; }
uint64_t bot_get_app_id(const bot_t* bot) { return bot->creds->app_id; }

static void check_heartbeat_timer(bot_t* bot) {
    if (bot->heartbeat_interval_ms == 0) {
        return;
    }

    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);

    const struct timespec* last = &bot->last_heartbeat;
    int64_t diff_seconds = (int64_t)now.tv_sec - (int64_t)last->tv_sec;
    int64_t diff_nano = (int64_t)now.tv_nsec - (int64_t)last->tv_nsec;
    int64_t diff_ms = diff_seconds * 1e3 + diff_nano / 1e6;

    if (diff_ms >= bot->heartbeat_interval_ms) {
        log_trace("timeout elapsed; sending heartbeat");
        send_heartbeat(bot);
    }
}

void bot_start(bot_t* bot) {
    bot->running = true;
    while (bot->running) {
        rest_poll(bot->rest);
        ws_poll(bot->gateway);

        check_heartbeat_timer(bot);
        usleep(10000);
    }
}

void bot_stop(bot_t* bot) { bot->running = false; }
