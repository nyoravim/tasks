/* https://curl.se/libcurl/c/multi-app.html */

#include "rest.h"

#include <log.h>

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>
#include <ctype.h>

#include <nyoravim/mem.h>
#include <nyoravim/map.h>
#include <nyoravim/util.h>

struct request {
    void* body;
    size_t body_size, body_offset;

    CURL* handle;
    struct curl_slist* headers;

    struct rest_callbacks callbacks;
};

static void free_request(CURLM* multi, struct request* req) {
    curl_multi_remove_handle(multi, req->handle);
    curl_easy_cleanup(req->handle);
    curl_slist_free_all(req->headers);

    nv_free(req->body);
    nv_free(req);
}

/* user: CURLM*
 * value: struct request* */
static void rest_free_value(void* user, void* value) { free_request(user, value); }

typedef struct rest {
    CURLM* multi;

    /* the CURL handle also serves as the key to the request state */
    nv_map_t* requests;
} rest_t;

static uint32_t curl_refs = 0;

bool rest_curl_ref() {
    log_trace("curl ref");

    if (curl_refs == 0) {
        log_debug("initializing libcurl");

        CURLcode result = curl_global_init(CURL_GLOBAL_ALL);
        if (result != CURLE_OK) {
            log_error("failed to init curl: %s", curl_easy_strerror(result));
            return false;
        }
    }

    curl_refs++;
    return true;
}

void rest_curl_unref() {
    if (curl_refs == 0) {
        return;
    }

    log_trace("curl unref");
    curl_refs--;

    if (curl_refs == 0) {
        log_debug("no more curl handles; cleaning up libcurl");
        curl_global_cleanup();
    }
}

rest_t* rest_init() {
    log_debug("new rest instance");

    if (!rest_curl_ref()) {
        return NULL;
    }

    CURL* multi = curl_multi_init();
    if (!multi) {
        log_error("failed to initialize multi curl handle fsr; bailing");

        rest_curl_unref();
        return NULL;
    }

    struct nv_map_callbacks callbacks;
    memset(&callbacks, 0, sizeof(struct nv_map_callbacks));

    callbacks.user = multi;
    callbacks.free_value = rest_free_value;

    rest_t* rest = nv_alloc(sizeof(rest_t));
    assert(rest);

    rest->multi = multi;
    rest->requests = nv_map_alloc(64, &callbacks);
    assert(rest->requests);

    return rest;
}

void rest_shutdown(rest_t* rest) {
    if (!rest) {
        return;
    }

    nv_map_free(rest->requests);
    curl_multi_cleanup(rest->multi);

    nv_free(rest);
    rest_curl_unref();
}

static bool dispatch_done(rest_t* rest, CURL* handle, CURLcode code) {
    struct request* req;
    if (!nv_map_get(rest->requests, handle, (void**)&req)) {
        return false;
    }

    if (req->callbacks.done_callback) {
        long status;
        curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &status);

        req->callbacks.done_callback(req->callbacks.user, code, (int64_t)status);
    } else if (code != CURLE_OK) {
        log_error("curl error: %s", curl_easy_strerror(code));
    }

    return true;
}

bool rest_poll(rest_t* rest) {
    int still_running;
    CURLMcode result = curl_multi_perform(rest->multi, &still_running);

    if (result != CURLM_OK) {
        log_error("curl_multi_perform: %s", curl_multi_strerror(result));
        return false;
    }

    if (still_running > 0) {
        result = curl_multi_poll(rest->multi, NULL, 0, 100, NULL);
        if (result != CURLM_OK) {
            log_error("curl_multi_poll: %s", curl_multi_strerror(result));
            return false;
        }
    }

    int msgs_left;
    CURLMsg* msg;

    while ((msg = curl_multi_info_read(rest->multi, &msgs_left)) != NULL) {
        if (msg->msg != CURLMSG_DONE) {
            continue; /* dont care */
        }

        if (!dispatch_done(rest, msg->easy_handle, msg->data.result)) {
            log_warn("failed to dispatch \"done\" message on rest request");
        }

        nv_map_remove(rest->requests, msg->easy_handle);
    }

    return true;
}

/* https://curl.se/libcurl/c/CURLOPT_READFUNCTION.html */
static size_t rest_read_callback(char* ptr, size_t size, size_t nitems, void* userdata) {
    struct request* req = userdata;

    size_t available = req->body_size - req->body_offset;
    size_t to_read = available > nitems ? nitems : available;

    if (to_read > 0) {
        memcpy(ptr, req->body + req->body_offset, to_read);
    }

    req->body_offset += to_read;
    return to_read;
}

/* https://curl.se/libcurl/c/CURLOPT_WRITEFUNCTION.html */
static size_t rest_write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    struct request* req = userdata;
    if (req->callbacks.receive_data_callback) {
        req->callbacks.receive_data_callback(req->callbacks.user, ptr, nmemb);
    }

    /* i cant be bothered to ask the user */
    return nmemb;
}

static char* str_to_upper(const char* str) {
    char* block = nv_strdup(str);
    if (!block) {
        return NULL;
    }

    size_t len = strlen(str);
    for (size_t i = 0; i < len; i++) {
        block[i] = toupper(block[i]);
    }

    return block;
}

static struct curl_slist* create_header_list(const char* const* headers, size_t num) {
    struct curl_slist* list = NULL;

    for (size_t i = 0; i < num; i++) {
        const char* header = headers[i];

        /* does not say anywhere if it copies. assuming it does because const char* */
        list = curl_slist_append(list, header);
    }

    return list;
}

bool rest_send(rest_t* rest, const struct http_request* spec,
               const struct rest_callbacks* callbacks) {
    CURL* handle = curl_easy_init();
    if (!handle) {
        log_error("failed to initialize handle for http request!");
        return NULL;
    }

    struct request* req = nv_alloc(sizeof(struct request));
    assert(req);

    memcpy(&req->callbacks, callbacks, sizeof(struct rest_callbacks));

    req->handle = handle;
    req->body_offset = 0;
    req->headers = create_header_list(spec->headers, spec->num_headers);

    char* method_upper = str_to_upper(spec->method);
    bool is_get = strcmp(method_upper, "GET") == 0;

    /* get method should send no data */
    if (is_get || spec->size == 0) {
        req->body_size = 0;
        req->body = NULL;
    } else {
        void* block = nv_alloc(spec->size);
        assert(block);
        memcpy(block, spec->data, spec->size);

        req->body_size = spec->size;
        req->body = block;
    }

    log_trace("%s request to %s", method_upper, spec->url);
    for (size_t i = 0; i < spec->num_headers; i++) {
        log_trace("header \"%s\"", spec->headers[i]);
    }

    curl_easy_setopt(handle, CURLOPT_URL, spec->url);
    curl_easy_setopt(handle, CURLOPT_CUSTOMREQUEST, method_upper);
    curl_easy_setopt(handle, CURLOPT_HTTPHEADER, req->headers);

    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, rest_write_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, req);

    curl_easy_setopt(handle, CURLOPT_READFUNCTION, rest_read_callback);
    curl_easy_setopt(handle, CURLOPT_READDATA, req);

    curl_multi_add_handle(rest->multi, handle);
    assert(nv_map_insert(rest->requests, handle, req));

    nv_free(method_upper);
    return handle;
}

struct await_data {
    bool done;
    struct http_response* response;
};

static void async_receive_func(void* user, const void* data, size_t length) {
    struct await_data* ad = user;
    struct http_response* resp = ad->response;

    size_t new_size = resp->length + length + 1; /* null terminator */
    if (resp->length > 0) {
        resp->content = nv_realloc(resp->content, new_size);
    } else {
        resp->content = nv_alloc(new_size);
    }

    assert(resp->content);
    memcpy(resp->content + resp->length, data, length);

    resp->length += length;
    resp->content[resp->length] = '\0'; /* so its readable as a string */
}

static void async_on_done(void* user, CURLcode code, int64_t status) {
    struct await_data* ad = user;

    if (code != CURLE_OK && code != CURLE_HTTP_RETURNED_ERROR) {
        log_warn("curl error: %s", curl_easy_strerror(code));
    }

    ad->done = true;
    ad->response->status = status;
}

bool rest_send_await(rest_t* rest, const struct http_request* req, struct http_response* response) {
    response->content = NULL;
    response->length = 0;

    struct await_data data;
    data.done = false;
    data.response = response;

    struct rest_callbacks callbacks;
    callbacks.user = &data;
    callbacks.receive_data_callback = async_receive_func;
    callbacks.done_callback = async_on_done;

    if (!rest_send(rest, req, &callbacks)) {
        return false;
    }

    while (!data.done) {
        if (!rest_poll(rest)) {
            return false;
        }
    }

    return true;
}
