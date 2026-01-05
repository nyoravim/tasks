/* https://curl.se/libcurl/c/websocket.html */

#include "websocket.h"

/* libcurl ref stuff */
#include "rest.h"

#include <log.h>

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <inttypes.h>

#include <arpa/inet.h>

#include <nyoravim/mem.h>

typedef struct ws {
    CURL* handle;
    struct websocket_callbacks callbacks;
} ws_t;

ws_t* ws_open(const char* url, const struct websocket_callbacks* callbacks) {
    log_debug("opening websocket to url: %s", url);

    if (!rest_curl_ref()) {
        return NULL;
    }

    CURL* handle = curl_easy_init();
    if (!handle) {
        log_error("failed to initialize curl handle for websocket");

        rest_curl_unref();
        return NULL;
    }

    curl_easy_setopt(handle, CURLOPT_URL, url);
    curl_easy_setopt(handle, CURLOPT_PROTOCOLS_STR, "ws,wss");
    curl_easy_setopt(handle, CURLOPT_CONNECT_ONLY, 2L);

    CURLcode result = curl_easy_perform(handle);
    if (result != CURLE_OK) {
        log_error("failed to connect websocket: %s", curl_easy_strerror(result));

        rest_curl_unref();
        return NULL;
    }

    ws_t* ws = nv_alloc(sizeof(ws_t));
    assert(ws);

    memcpy(&ws->callbacks, callbacks, sizeof(struct websocket_callbacks));

    ws->handle = handle;
    return ws;
}

static size_t create_close_payload(char* buffer, size_t max_length, uint16_t code,
                                   const char* reason) {
    if (max_length < sizeof(uint16_t)) {
        return 0;
    }

    /* code in network byte order */
    uint16_t htons_code = htons(code);
    size_t code_size = sizeof(uint16_t);
    memcpy(buffer, &htons_code, code_size);

    /* and then reason, if present */
    size_t reason_length;
    if (reason) {
        strncpy(buffer + code_size, reason, max_length - code_size);
        reason_length = strlen(reason);
    } else {
        reason_length = 0;
    }

    size_t assumed_size = reason_length + code_size;
    return assumed_size > max_length ? max_length : assumed_size;
}

void ws_close(ws_t* ws, uint16_t code, const char* reason) {
    if (!ws) {
        return;
    }

    log_debug("closing websocket with code %" PRIu16, code);
    if (reason) {
        log_debug("reason: %s", reason);
    }

    char buffer[256];
    size_t buffer_size = create_close_payload(buffer, sizeof(buffer), code, reason);

    size_t sent;
    curl_ws_send(ws->handle, buffer, buffer_size, &sent, 0, CURLWS_CLOSE);

    ws_disconnect(ws);
}

void ws_disconnect(ws_t* ws) {
    if (!ws) {
        return;
    }

    curl_easy_cleanup(ws->handle);
    nv_free(ws);

    rest_curl_unref();
}

bool ws_send(ws_t* ws, const void* data, size_t size, uint32_t flags) {
    while (size > 0) {
        size_t sent;
        CURLcode result = curl_ws_send(ws->handle, data, size, &sent, 0, flags);

        if (result == CURLE_AGAIN) {
            log_warn("failed to send data on websocket; waiting a second...");
            sleep(1);

            continue;
        }

        if (result != CURLE_OK) {
            log_error("curl_ws_send: %s", curl_easy_strerror(result));
            return false;
        }

        data += sent;
        size -= sent;
    }

    return true;
}

bool ws_poll(ws_t* ws) {
    static char buffer[8192];
    size_t buffer_size = sizeof(buffer) - 1; /* to make room for '\0' */

    size_t received = buffer_size;
    const struct curl_ws_frame* meta;

    do {
        CURLcode result = curl_ws_recv(ws->handle, buffer, buffer_size, &received, &meta);
        if (result == CURLE_AGAIN) {
            /* no more data */
            break;
        }

        if (result != CURLE_OK) {
            log_error("curl_ws_recv: %s", curl_easy_strerror(result));
            return false;
        }

        if (ws->callbacks.on_frame_received) {
            /* enable strlen */
            buffer[received] = '\0';

            ws->callbacks.on_frame_received(ws->callbacks.user, buffer, received, meta);
        }
    } while (received >= buffer_size);

    return true;
}
