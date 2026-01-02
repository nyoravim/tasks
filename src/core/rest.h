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
};

struct http_response {
    /* allocated with nv_alloc */
    char* content;
    size_t length;

    int64_t status;
};

bool rest_curl_ref();
void rest_curl_unref();

rest_t* rest_init();
void rest_shutdown(rest_t* rest);

bool rest_poll(rest_t* rest);

bool rest_send(rest_t* rest, const struct http_request* spec,
               const struct rest_callbacks* callbacks);

bool rest_send_await(rest_t* rest, const struct http_request* req, struct http_response* response);

#endif
