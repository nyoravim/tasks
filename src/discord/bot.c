#include "bot.h"
#include "credentials.h"
#include "gateway.h"

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

typedef struct bot {
    struct credentials* creds;
    struct bot_callbacks callbacks;

    rest_t* rest;
    gateway_t* gateway;
    uint32_t api;

    bool running;
} bot_t;

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

static char* get_url_from_gateway_response(const json_object* object, uint32_t api) {
    if (!object) {
        return NULL;
    }

    json_object* url_field;
    if (!json_object_object_get_ex(object, "url", &url_field)) {
        return NULL;
    }

    const char* returned_url = json_object_get_string(url_field);
    if (!returned_url) {
        return NULL;
    }

    char buffer[256];
    snprintf(buffer, 256, "%s/?v=%" PRIu32 "&encoding=json", returned_url, api);

    return nv_strdup(buffer);
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

    char* url = get_url_from_gateway_response(response, api);
    json_object_put(response);

    return url;
}

static gateway_t* open_gateway(bot_t* bot) {
    char* url = get_gateway_url(bot->rest, bot->creds->token, bot->api);
    if (!url) {
        log_error("failed to retrieve gateway url from discord!");
        return NULL;
    }

    log_debug("discord gateway url: %s", url);

    gateway_t* gw = gateway_open(url, bot);
    nv_free(url);

    if (!gw) {
        log_error("failed to open gateway websocket!");
        return NULL;
    }

    return gw;
}

bot_t* bot_create(const struct bot_spec* spec) {
    bot_t* bot = nv_alloc(sizeof(bot_t));
    assert(bot);

    bot->rest = NULL;
    bot->gateway = NULL;

    bot->creds = credentials_dup(spec->creds);
    log_info("authenticating as app %" PRIu64, bot->creds);

    /* https://discord.com/developers/docs/reference#api-versioning */
    uint32_t api = spec->api > 0 ? spec->api : 10;

    bot->rest = rest_init();
    if (!bot->rest) {
        log_error("failed to initialize curl!");

        bot_destroy(bot);
        return NULL;
    }

    memcpy(&bot->callbacks, spec->callbacks, sizeof(struct bot_callbacks));

    bot->api = api;
    bot->running = false;

    bot->gateway = open_gateway(bot);
    if (!bot->gateway) {
        log_error("failed to open discord gateway!");

        bot_destroy(bot);
        return NULL;
    }

    return bot;
}

void bot_destroy(bot_t* bot) {
    if (!bot) {
        return;
    }

    gateway_close(bot->gateway);
    rest_shutdown(bot->rest);

    credentials_free(bot->creds);
    nv_free(bot);
}

const char* bot_get_token(const bot_t* bot) { return bot->creds->token; }
uint64_t bot_get_app_id(const bot_t* bot) { return bot->creds->app_id; }
const struct bot_callbacks* bot_get_callbacks(const bot_t* bot) { return &bot->callbacks; }

void bot_start(bot_t* bot) {
    bot->running = true;
    while (bot->running) {
        rest_poll(bot->rest);
        gateway_poll(bot->gateway);

        usleep(10000);
    }
}

void bot_stop(bot_t* bot) { bot->running = false; }
