#include "interaction.h"

#include "snowflake.h"
#include "user.h"

#include <string.h>

#include <log.h>

#include <nyoravim/mem.h>
#include <nyoravim/util.h>

bool interaction_parse(struct interaction* interaction, const json_object* data) {
    memset(interaction, 0, sizeof(struct interaction));
    if (!data) {
        return false;
    }

    json_object* field = json_object_object_get(data, "id");
    if (!snowflake_parse(&interaction->id, field)) {
        log_error("failed to parse interaction id!");

        interaction_cleanup(interaction);
        return false;
    }

    field = json_object_object_get(data, "application_id");
    if (!snowflake_parse(&interaction->application_id, field)) {
        log_error("failed to parse application id!");

        interaction_cleanup(interaction);
        return false;
    }

    field = json_object_object_get(data, "type");
    if (!field || json_object_get_type(field) != json_type_int) {
        log_error("no interaction type!");

        interaction_cleanup(interaction);
        return false;
    }

    interaction->type = (uint32_t)json_object_get_int(field);

    field = json_object_object_get(data, "data");
    bool should_have_data = true;

    switch (interaction->type) {
        /* todo: implement interaction data */
    default:
        interaction->data = NULL;
        should_have_data = false;

        break;
    }

    if (should_have_data && !interaction->data) {
        log_error("expected data for message type; no data or invalid data received!");

        interaction_cleanup(interaction);
        return false;
    }

    field = json_object_object_get(data, "guild_id");
    if (!snowflake_parse(&interaction->guild_id, field)) {
        interaction->guild_id = 0;
    }

    field = json_object_object_get(data, "channel_id");
    if (!snowflake_parse(&interaction->channel_id, field)) {
        interaction->channel_id = 0;
    }

    field = json_object_object_get(data, "user");
    struct user user;
    if (user_parse(&user, field)) {
        interaction->user = nv_alloc(sizeof(struct user));
        memcpy(interaction->user, &user, sizeof(struct user));
    }

    field = json_object_object_get(data, "token");
    if (!field || json_object_get_type(field) != json_type_string) {
        log_error("no interaction response token provided!");

        interaction_cleanup(interaction);
        return false;
    }

    const char* token = json_object_get_string(field);
    interaction->token = nv_strdup(token);

    return true;
}

void interaction_cleanup(const struct interaction* interaction) {
    if (interaction->data) {
        switch (interaction->type) {
            /* todo: implement interaction data */
        default:
            log_warn("potentially leaking memory from interaction; no data associated with "
                     "interaction type");

            break;
        }

        nv_free(interaction->data);
    }

    if (interaction->user) {
        user_cleanup(interaction->user);
        nv_free(interaction->user);
    }

    nv_free(interaction->token);
}
