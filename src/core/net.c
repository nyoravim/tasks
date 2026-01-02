/* pinned examples:
 * https://curl.se/libcurl/c/websocket.html
 * https://curl.se/libcurl/c/multi-app.html */

#include "net.h"

#include <log.h>

#include <curl/curl.h>

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>

#include <nyoravim/mem.h>
#include <nyoravim/map.h>

enum request_type {
    REQUEST_TYPE_HTTP,
    REQUEST_TYPE_WEBSOCKET,
};

struct http_data {
    void* data;
    size_t size, offset;

    bool finished;
};

struct websocket_data {
    bool open;
};

struct request_state {
    enum request_type type;
    union {
        struct http_data http;
        struct websocket_data ws;
    };

    bool attached;
    CURL* handle;

    struct net_request_callbacks callbacks;
};

static void close_websocket(CURL* handle) {
    size_t sent;
    curl_ws_send(handle, "", 0, &sent, 0, CURLWS_CLOSE);
}

static void free_request_state(CURLM* multi, struct request_state* state) {
    switch (state->type) {
    case REQUEST_TYPE_HTTP:
        nv_free(state->http.data);
        break;
    case REQUEST_TYPE_WEBSOCKET:
        if (state->ws.open) {
            close_websocket(state->handle);
        }

        break;
    }

    if (state->attached) {
        curl_multi_remove_handle(multi, state->handle);
    }

    curl_easy_cleanup(state->handle);
    nv_free(state);
}

/* user: CURLM*
 * value: struct request_state* */
static void net_free_value(void* user, void* value) { free_request_state(user, value); }

typedef struct net {
    CURLM* multi;

    /* key: CURL*
     * value: struct handle_info */
    nv_map_t* handles;
} net_t;

static uint32_t curl_refs = 0;

static bool add_curl_ref() {
    log_debug("curl ref");

    if (curl_refs == 0) {
        log_info("initializing libcurl");

        CURLcode result = curl_global_init(CURL_GLOBAL_ALL);
        if (result != CURLE_OK) {
            log_error("failed to init curl: %s", curl_easy_strerror(result));
            return false;
        }
    }

    curl_refs++;
    return true;
}

static void remove_curl_ref() {
    if (curl_refs == 0) {
        return;
    }

    log_debug("curl unref");
    curl_refs--;

    if (curl_refs == 0) {
        log_info("no more curl handles; cleaning up libcurl");
        curl_global_cleanup();
    }
}

net_t* net_init() {
    log_debug("new net instance");

    if (!add_curl_ref()) {
        return NULL;
    }

    CURL* multi = curl_multi_init();
    if (!multi) {
        log_error("failed to initialize multi curl handle fsr; bailing");

        remove_curl_ref();
        return NULL;
    }

    struct nv_map_callbacks callbacks;
    memset(&callbacks, 0, sizeof(struct nv_map_callbacks));

    callbacks.user = multi;
    callbacks.free_value = net_free_value;

    net_t* net = nv_alloc(sizeof(net_t));
    assert(net);

    net->multi = multi;
    net->handles = nv_map_alloc(64, &callbacks);
    assert(net->handles);

    return net;
}

void net_shutdown(net_t* net) {
    if (!net) {
        return;
    }

    nv_map_free(net->handles);
    curl_multi_cleanup(net->multi);

    nv_free(net);
}

static bool poll_multi(net_t* net) {
    int still_running;
    CURLMcode result = curl_multi_perform(net->multi, &still_running);

    if (result != CURLM_OK) {
        log_error("curl_multi_perform: %s", curl_multi_strerror(result));
        return false;
    }

    if (still_running > 0) {
        result = curl_multi_poll(net->multi, NULL, 0, 100, NULL);
        if (result != CURLM_OK) {
            log_error("curl_multi_poll: %s", curl_multi_strerror(result));
            return false;
        }
    }

    return true;
}

static void update_http_request(net_t* net, struct request_state* state) {
    if (state->http.finished) {
        /* will free */
        nv_map_remove(net->handles, state->handle);
    }
}

static bool update_websocket(struct request_state* state) {
    static const size_t buffer_size = 256;
    char buffer[buffer_size];

    size_t received = buffer_size;
    const struct curl_ws_frame* meta;

    do {
        CURLcode result = curl_ws_recv(state->handle, buffer, buffer_size, &received, &meta);
        if (result == CURLE_AGAIN) {
            log_info("no data on websocket; waiting a second...");
            sleep(1);

            continue;
        }

        if (result != CURLE_OK) {
            log_error("curl_ws_recv: %s", curl_easy_strerror(result));
            return false;
        }

        state->callbacks.receive_data_callback(state->handle, state->callbacks.user, buffer,
                                               received);
    } while (received >= buffer_size);

    return true;
}

static bool update_individual_requests(net_t* net) {
    size_t active_request_count = nv_map_size(net->handles);

    struct nv_map_pair pairs[active_request_count];
    nv_map_enumerate(net->handles, pairs);

    for (size_t i = 0; i < active_request_count; i++) {
        struct request_state* state = pairs[i].value;
        switch (state->type) {
        case REQUEST_TYPE_HTTP:
            update_http_request(net, state);
            break;
        case REQUEST_TYPE_WEBSOCKET:
            update_websocket(state);
            break;
        }
    }

    return true;
}

bool net_poll(net_t* net) {
    if (!poll_multi(net)) {
        log_error("failed to batch poll requests!");
        return false;
    }

    return true;
}
