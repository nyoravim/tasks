#include "credentials.h"

#include <cjson/cJSON.h>

#include <concord/log.h>

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

static const char* get_object_string(const cJSON* object, const char* key) {
    cJSON* item = cJSON_GetObjectItem(object, key);
    if (!item) {
        return NULL;
    }

    return cJSON_GetStringValue(item);
}

static bool parse_to_u64(const char* str, uint64_t* value) {
    char* endptr;
    *value = strtoull(str, &endptr, 10);

    return errno != ERANGE && endptr != str;
}

static bool get_object_int(const cJSON* object, const char* key, uint64_t* value) {
    cJSON* item = cJSON_GetObjectItem(object, key);
    if (!item) {
        return false;
    }

    const char* integer_str = cJSON_GetStringValue(item);
    return parse_to_u64(integer_str, value);
}

struct credentials* credentials_read_from_path(const char* path) {
    FILE* file = fopen(path, "r");
    if (!file) {
        log_error("Failed to open credentials file: %s", path);
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    size_t len = ftell(file);
    fseek(file, 0, SEEK_SET);

    char* buffer = malloc(len);
    fread(buffer, 1, len, file);
    fclose(file);

    struct credentials* creds = credentials_read_from_data(buffer, len);
    free(buffer);

    return creds;
}

struct credentials* credentials_read_from_data(const char* data, size_t len) {
    cJSON* json = cJSON_ParseWithLength(data, len);

    if (!data) {
        const char* error = cJSON_GetErrorPtr();
        if (!error) {
            error = "<null>";
        }

        log_error("Failed to parse credentials: %s", error);
        return NULL;
    }

    uint64_t app_id;
    const char* token = get_object_string(json, "token");
    bool has_app_id = get_object_int(json, "app_id", &app_id);

    if (!token || !has_app_id) {
        log_error("Invalid credential format! Must provide token (str) and app_id (str)!");

        return NULL;
    }

    struct credentials* creds = malloc(sizeof(struct credentials));
    creds->token = strdup(token);
    creds->app_id = app_id;

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
