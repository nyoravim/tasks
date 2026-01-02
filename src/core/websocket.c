/* https://curl.se/libcurl/c/websocket.html */

#include "websocket.h"

/* libcurl ref stuff */
#include "rest.h"

#include <log.h>

#include <assert.h>
#include <string.h>
#include <unistd.h>

#include <nyoravim/mem.h>

typedef struct ws {
    CURL* handle;
    struct websocket_callbacks callbacks;
} ws_t;

ws_t* ws_open(const char* url, const struct websocket_callbacks* callbacks) {
    log_info("opening websocket to url: %s", url);

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

void ws_close(ws_t* ws) {
    if (!ws) {
        return;
    }

    size_t sent;
    curl_ws_send(ws->handle, "", 0, &sent, 0, CURLWS_CLOSE);

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
    static const size_t buffer_size = 256;
    char buffer[buffer_size];

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
            ws->callbacks.on_frame_received(ws->callbacks.user, buffer, received, meta);
        }
    } while (received >= buffer_size);

    return true;
}
