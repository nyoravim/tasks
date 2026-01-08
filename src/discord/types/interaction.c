#include "interaction.h"

#include "snowflake.h"
#include "user.h"

#include <string.h>

#include <log.h>

#include <nyoravim/mem.h>
#include <nyoravim/util.h>

static void free_command_options(struct command_option_data* data, size_t count) {
    for (size_t i = 0; i < count; i++) {
        const struct command_option_data* option = &data[i];

        nv_free(option->name);
        nv_free(option->value);
    }

    nv_free(data);
}

static void free_command_data(struct interaction_command_data* data) {
    if (!data) {
        return;
    }

    free_command_options(data->options, data->num_options);
    nv_free(data->name);
    nv_free(data);
}

static void parse_option_data(struct command_option_data* option, const json_object* data) {
    json_object* field = json_object_object_get(data, "type");
    if (!field || json_object_get_type(field) != json_type_int) {
        log_warn("option data has no type; aborting");
        return;
    }

    option->type = (uint32_t)json_object_get_int(field);

    field = json_object_object_get(data, "name");
    if (!field || json_object_get_type(field) != json_type_string) {
        log_warn("option data has no name; aborting");
        return;
    }

    const char* name = json_object_get_string(field);
    option->name = nv_strdup(name);

    field = json_object_object_get(data, "value");
    if (field) {
        const char* value = json_object_get_string(field);
        option->value = nv_strdup(value);
    }

    field = json_object_object_get(data, "focused");
    if (field && json_object_get_type(field) == json_type_boolean) {
        option->focused = (bool)json_object_get_boolean(field);
    }
}

static struct interaction_command_data* parse_command_data(const json_object* data) {
    if (!data) {
        return NULL;
    }

    struct interaction_command_data* cmd = nv_alloc(sizeof(struct interaction_command_data));
    memset(cmd, 0, sizeof(struct interaction_command_data));

    json_object* field = json_object_object_get(data, "id");
    if (!snowflake_parse(&cmd->id, field)) {
        log_error("command data had no id!");

        free_command_data(cmd);
        return NULL;
    }

    field = json_object_object_get(data, "name");
    if (!field || json_object_get_type(field) != json_type_string) {
        log_error("command data had no name!");

        free_command_data(cmd);
        return NULL;
    }

    const char* name = json_object_get_string(field);
    cmd->name = nv_strdup(name);

    field = json_object_object_get(data, "type");
    if (!field || json_object_get_type(field) != json_type_int) {
        log_error("command data had no command type!");

        free_command_data(cmd);
        return NULL;
    }

    cmd->type = (uint32_t)json_object_get_int(field);

    field = json_object_object_get(data, "options");
    if (field && json_object_get_type(field) == json_type_array) {
        cmd->num_options = json_object_array_length(field);
        cmd->options = nv_calloc(cmd->num_options, sizeof(struct command_option_data));

        for (size_t i = 0; i < cmd->num_options; i++) {
            json_object* element = json_object_array_get_idx(field, i);
            parse_option_data(&cmd->options[i], element);
        }
    }

    field = json_object_object_get(data, "guild_id");
    if (!snowflake_parse(&cmd->guild_id, field)) {
        cmd->guild_id = 0;
    }

    field = json_object_object_get(data, "target_id");
    if (!snowflake_parse(&cmd->target_id, field)) {
        cmd->target_id = 0;
    }

    return cmd;
}

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
    case INTERACTION_TYPE_APPLICATION_COMMAND:
    case INTERACTION_TYPE_APPLICATION_COMMAND_AUTOCOMPLETE:
        interaction->command_data = parse_command_data(field);
        if (!interaction->command_data) {
            interaction_cleanup(interaction);
            return false;
        }

        break;
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
    switch (interaction->type) {
    case INTERACTION_TYPE_APPLICATION_COMMAND:
    case INTERACTION_TYPE_APPLICATION_COMMAND_AUTOCOMPLETE:
        free_command_data(interaction->command_data);
        break;
    default:
        if (interaction->reserved) {
            log_warn("potentially leaking memory from interaction; no data associated with "
                     "interaction type");

            nv_free(interaction->reserved);
        }

        break;
    }

    if (interaction->user) {
        user_cleanup(interaction->user);
        nv_free(interaction->user);
    }

    nv_free(interaction->token);
}
