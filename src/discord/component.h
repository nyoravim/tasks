#ifndef _COMPONENT_H
#define _COMPONENT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include <json.h>

enum {
    COMPONENT_TYPE_ACTION_ROW = 1,
    COMPONENT_TYPE_BUTTON = 2,
    COMPONENT_TYPE_STRING_SELECT = 3,
    COMPONENT_TYPE_TEXT_INPUT = 4,
    COMPONENT_TYPE_USER_SELECT = 5,
    COMPONENT_TYPE_ROLE_SELECT = 6,
    COMPONENT_TYPE_MENTIONABLE_SELECT = 7,
    COMPONENT_TYPE_CHANNEL_SELECT = 8,
    COMPONENT_TYPE_SECTION = 9,
    COMPONENT_TYPE_TEXT_DISPLAY = 10,
    COMPONENT_TYPE_THUMBNAIL = 11,
    COMPONENT_TYPE_MEDIA_GALLERY = 12,
    COMPONENT_TYPE_FILE = 13,
    COMPONENT_TYPE_SEPARATOR = 14,
    COMPONENT_TYPE_CONTAINER = 17,
    COMPONENT_TYPE_LABEL = 18,
    COMPONENT_TYPE_FILE_UPLOAD = 19,
};

enum {
    BUTTON_STYLE_PRIMARY = 1,
    BUTTON_STYLE_SECONDARY = 2,
    BUTTON_STYLE_SUCCESS = 3,
    BUTTON_STYLE_DANGER = 4,
    BUTTON_STYLE_LINK = 5,
};

struct action_row {
    size_t num_children;
    const struct component* children;
};

struct button {
    uint32_t style;
    const char* label;

    const void* data;
    size_t data_size;

    bool disabled;
};

struct text_display {
    const char* content;
};

struct label {
    const char* label;

    /* can be NULL */
    const char* description;

    const struct component* child;
};

struct component {
    uint32_t type;

    union {
        struct action_row action_row;
        struct button button;
        struct text_display text_display;
        struct label label;
    };
};

json_object* component_serialize(const struct component* comp);

#endif
