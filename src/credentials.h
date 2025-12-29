#ifndef _CREDENTIALS_H
#define _CREDENTIALS_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

struct credentials {
    char* token;
    uint64_t app_id;
};

struct credentials* credentials_read_from_path(const char* path);
struct credentials* credentials_read_from_data(const char* data, size_t len);

struct credentials* credentials_dup(const struct credentials* src);
void credentials_free(struct credentials* creds);

#endif
