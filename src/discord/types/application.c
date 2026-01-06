#include "application.h"

#include "snowflake.h"

#include <string.h>

#include <log.h>

#include <nyoravim/mem.h>
#include <nyoravim/util.h>

bool application_parse(struct application* app, const json_object* data) {
    memset(app, 0, sizeof(struct application));
    if (!data) {
        application_cleanup(app);
        return false;
    }

    json_object* field = json_object_object_get(data, "id");
    if (!snowflake_parse(&app->id, field)) {
        log_error("failed to parse application snowflake!");

        application_cleanup(app);
        return false;
    }

    field = json_object_object_get(data, "name");
    if (field && json_object_get_type(field) == json_type_string) {
        const char* name = json_object_get_string(field);
        app->name = nv_strdup(name);
    }

    field = json_object_object_get(data, "flags");
    if (field && json_object_get_type(field) == json_type_int) {
        app->flags = (uint32_t)json_object_get_int(field);
    }

    return true;
}

void application_cleanup(const struct application* app) {
    nv_free(app->name);
}
