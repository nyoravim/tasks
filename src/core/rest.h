#ifndef _REST_H
#define _REST_H

#include <curl/curl.h>

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

typedef struct rest rest_t;

struct rest_callbacks {
    void (*receive_data_callback)(void* user, const void* data, size_t size);
    void (*done_callback)(void* user, CURLcode result, int64_t status);

    void* user;
};

struct http_request {
    const char* url;
    const char* method;

    const void* data;
    size_t size;

    const char* const* headers;
    size_t num_headers;

    const struct rest_callbacks* callbacks;
};

bool rest_curl_ref();
void rest_curl_unref();

rest_t* rest_init();
void rest_shutdown(rest_t* rest);

bool rest_send(rest_t* rest, const struct http_request* spec);
bool rest_poll(rest_t* rest);

#endif
