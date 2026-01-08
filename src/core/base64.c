#include "base64.h"

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include <nyoravim/mem.h>

/* im using http encoding just to be safe */

static char to_base64(uint8_t number) {
    assert(number < 64);

    /* A-Z starts at 0 */
    if (number < 26) {
        return (char)number + 'A';
    }

    /* a-z starts at 26 */
    if (number < 52) {
        return (char)(number - 26) + 'a';
    }

    /* 0-9 starts at 52 */
    if (number < 62) {
        return (char)(number - 52) + '0';
    }

    /* + or - = 62 */
    if (number == 62) {
        return '-';
    }

    /* / or _ = 63 */
    return '_';
}

static uint8_t from_base64(char c) {
    /* A-Z starts at 0 */
    if (c >= 'A' && c <= 'Z') {
        return (uint8_t)(c - 'A');
    }

    /* a-z starts at 26 */
    if (c >= 'a' && c <= 'z') {
        return (uint8_t)(c - 'a') + 26;
    }

    /* 0-9 starts at 52 */
    if (c >= '0' && c <= '9') {
        return (uint8_t)(c - '0') + 52;
    }

    switch (c) {
    case '+':
    case '-':
        return 62;
    case '/':
    case '_':
        return 63;
    default:
        assert(false);
        return 0;
    }
}

static void encode_chunk(const uint8_t* src, size_t size, char* dst) {
    for (size_t i = 0; i < 4; i++) {
        if (i < size + 1) {
        }
    }
}

char* base64_encode(const void* src, size_t size) {
    size_t chunk_count = size / 3;
    if (size % 3 != 0) {
        chunk_count++;
    }

    char* dst = nv_alloc(chunk_count * 4);
    assert(dst);

    for (size_t i = 0; i < chunk_count; i++) {
        size_t src_offset = i * 3;
        size_t dst_offset = i * 4;

        encode_chunk(src + src_offset, size - src_offset, dst + dst_offset);
    }

    return dst;
}
