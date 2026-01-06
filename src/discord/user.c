#include "user.h"

#include "snowflake.h"

#include <string.h>

#include <log.h>

#include <nyoravim/mem.h>
#include <nyoravim/util.h>

bool user_parse(struct user* user, const json_object* data) {
    memset(user, 0, sizeof(struct user));
    if (!data) {
        user_cleanup(user);
        return false;
    }

    json_object* field = json_object_object_get(data, "id");
    if (!snowflake_parse(&user->id, field)) {
        log_error("failed to parse snowflake in user!");

        user_cleanup(user);
        return false;
    }

    field = json_object_object_get(data, "username");
    if (!field || json_object_get_type(field) != json_type_string) {
        log_error("failed to parse username in user!");

        user_cleanup(user);
        return false;
    }

    const char* str = json_object_get_string(field);
    user->username = nv_strdup(str);

    field = json_object_object_get(data, "discriminator");
    if (!field || json_object_get_type(field) != json_type_string) {
        log_error("failed to parse discriminator in user!");

        user_cleanup(user);
        return false;
    }

    str = json_object_get_string(field);
    user->discriminator = nv_strdup(str);

    field = json_object_object_get(data, "global_name");
    if (field && json_object_get_type(field) == json_type_string) {
        str = json_object_get_string(field);
        user->global_name = nv_strdup(str);
    }

    return true;
}

void user_cleanup(const struct user* user) {
    nv_free(user->username);
    nv_free(user->discriminator);
    nv_free(user->global_name);
}
