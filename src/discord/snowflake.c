#include "snowflake.h"

#include <errno.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

#include <log.h>

/* snowflakes are serialized as strings; see
 * https://discord.com/developers/docs/reference#snowflakes */
bool snowflake_parse(uint64_t* id, json_object* data) {
    if (!data || json_object_get_type(data) != json_type_string) {
        return false;
    }

    const char* snowflake = json_object_get_string(data);

    errno = 0;
    char* endptr;
    *id = strtoull(snowflake, &endptr, 10);

    if (endptr == snowflake) {
        log_error("invalid snowflake!");
        return false;
    }

    if (errno == ERANGE) {
        log_error("snowflake too large!");
        return false;
    }

    return true;
}

json_object* snowflake_serialize(uint64_t id) {
    char buffer[64];
    snprintf(buffer, sizeof(buffer), "%" PRIu64, id);

    json_object* snowflake = json_object_new_string(buffer);
    assert(snowflake);

    return snowflake;
}
