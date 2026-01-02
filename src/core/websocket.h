#ifndef _WEBSOCKET_H
#define _WEBSOCKET_H

#include <curl/curl.h>

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct ws ws_t;

struct websocket_callbacks {
    void (*on_frame_received)(void* user, const char* data, size_t size,
                              const struct curl_ws_frame* meta);

    void* user;
};

ws_t* ws_open(const char* url, const struct websocket_callbacks* callbacks);
void ws_close(ws_t* ws);

bool ws_send(ws_t* ws, const void* data, size_t size, uint32_t flags);

bool ws_poll(ws_t* ws);

#endif
