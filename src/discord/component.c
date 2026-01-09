#include "component.h"

#include "../core/base64.h"

#include <assert.h>

#include <log.h>

#include <nyoravim/mem.h>

static void serialize_action_row(json_object* result, const struct action_row* data) {
    json_object* children = json_object_new_array();
    assert(children);

    for (size_t i = 0; i < data->num_children; i++) {
        const struct component* child = &data->children[i];
        json_object* serialized = component_serialize(child);

        json_object_array_add(children, serialized);
    }

    json_object_object_add(result, "components", children);
}

static void serialize_button(json_object* result, const struct button* data) {
    json_object* field = json_object_new_int((int32_t)data->style);
    assert(field);
    json_object_object_add(result, "style", field);

    field = json_object_new_string(data->label);
    assert(field);
    json_object_object_add(result, "label", field);

    field = json_object_new_boolean((json_bool)data->disabled);
    assert(field);
    json_object_object_add(result, "disabled", field);

    char* b64_data = base64_encode(data->data, data->data_size);
    assert(b64_data);

    field = json_object_new_string(b64_data);
    assert(field);

    json_object_object_add(result, "custom_id", field);
    nv_free(b64_data);
}

static void serialize_text_display(json_object* result, const struct text_display* data) {
    json_object* field = json_object_new_string(data->content);
    assert(field);
    json_object_object_add(result, "content", field);
}

json_object* component_serialize(const struct component* comp) {
    json_object* result = json_object_new_object();
    assert(result);

    switch (comp->type) {
    case COMPONENT_TYPE_ACTION_ROW:
        serialize_action_row(result, &comp->action_row);
        break;
    case COMPONENT_TYPE_BUTTON:
        serialize_button(result, &comp->button);
        break;
    case COMPONENT_TYPE_TEXT_DISPLAY:
        serialize_text_display(result, &comp->text_display);
        break;
    default:
        log_error("unsupported component type: %" PRIu32, comp->type);

        json_object_put(result);
        return NULL;
    }

    json_object* field = json_object_new_int((int32_t)comp->type);
    assert(field);
    json_object_object_add(result, "type", field);

    return result;
}
