#include "member.h"

#include "user.h"

#include <string.h>

#include <nyoravim/mem.h>
#include <nyoravim/util.h>

bool member_parse(struct member* member, const json_object* data) {
    memset(member, 0, sizeof(struct member));
    if (!data) {
        return false;
    }

    json_object* field = json_object_object_get(data, "user");
    struct user user;
    if (user_parse(&user, field)) {
        member->user = nv_alloc(sizeof(struct user));
        memcpy(member->user, &user, sizeof(struct user));
    }

    field = json_object_object_get(data, "nick");
    if (field && json_object_get_type(field) == json_type_string) {
        const char* nick = json_object_get_string(field);
        member->nick = nv_strdup(nick);
    }

    return true;
}

void member_cleanup(const struct member* member) {
    if (member->user) {
        user_cleanup(member->user);
        nv_free(member->user);
    }

    nv_free(member->nick);
}
