#ifndef _CREDENTIALS_H
#define _CREDENTIALS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct credentials {
    char* token;
    uint64_t app_id;
    uint64_t guild_scope;
};

struct credentials* credentials_read_from_path(const char* path);

struct credentials* credentials_dup(const struct credentials* src);
void credentials_free(struct credentials* creds);

#endif
