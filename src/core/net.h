#ifndef _NET_H
#define _NET_H

#include <curl/curl.h>

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct net net_t;

struct net_request_callbacks {
    void (*receive_data_callback)(void* request, void* user, const void* data, size_t size);
    void (*done_callback)(void* request, void* user, CURLcode result, int64_t status);

    void* user;
};

struct net_http_request {
    const char* url;
    const char* method;

    const void* data;
    size_t size;

    const struct net_request_callbacks* callbacks;
};

net_t* net_init();
void net_shutdown(net_t* net);

bool net_poll(net_t* net);

void* net_http(const struct net_http_request* http);
void* net_websocket(const char* url, const struct net_request_callbacks* callbacks);

#endif
