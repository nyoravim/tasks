#include "credentials.h"

#include <json.h>

#include <log.h>

#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static const char* get_object_string(const json_object* object, const char* key) {
    json_object* item = json_object_object_get(object, key);
    if (!item) {
        return NULL;
    }

    return json_object_get_string(item);
}

static bool parse_to_u64(const char* str, uint64_t* value) {
    char* endptr;
    *value = strtoull(str, &endptr, 10);

    return errno != ERANGE && endptr != str;
}

static bool get_object_int(const json_object* object, const char* key, uint64_t* value) {
    json_object* item = json_object_object_get(object, key);
    if (!item) {
        return false;
    }

    if (!json_object_is_type(item, json_type_int)) {
        return false;
    }

    *value = json_object_get_uint64(item);
    return true;
}

struct credentials* credentials_read_from_path(const char* path) {
    json_object* json = json_object_from_file(path);
    if (!json) {
        log_error("Failed to parse credentials file: %s", json_util_get_last_err());
        return NULL;
    }

    uint64_t app_id;
    const char* token = get_object_string(json, "token");
    bool has_app_id = get_object_int(json, "app_id", &app_id);

    if (!token || !has_app_id) {
        log_error("invalid credential format; must provide token (int) and app_id (str)!");

        json_object_put(json);
        return NULL;
    }

    struct credentials* creds = malloc(sizeof(struct credentials));
    creds->token = strdup(token);
    creds->app_id = app_id;

    json_object_put(json);
    return creds;
}

struct credentials* credentials_dup(const struct credentials* src) {
    if (!src) {
        return NULL;
    }

    struct credentials* dst = malloc(sizeof(struct credentials));
    dst->token = strdup(src->token);
    dst->app_id = src->app_id;

    return dst;
}

void credentials_free(struct credentials* creds) {
    if (!creds) {
        return;
    }

    free(creds->token);
    free(creds);
}
