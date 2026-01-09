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
        if (i > size) {
            dst[i] = '=';
            continue;
        }

        uint8_t value = 0;
        if (i > 0) {
            value |= src[i - 1] << (6 - i * 2);
        }

        if (i < size) {
            value |= src[i] >> ((i + 1) * 2);
        }

        dst[i] = to_base64(value);
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

static void decode_chunk(const char* src, size_t size, uint8_t* dst) {
    uint8_t data[size + 1];
    for (size_t i = 0; i < size + 1; i++) {
        data[i] = from_base64(src[i]);
    }

    for (size_t i = 0; i < size; i++) {
        uint8_t current = data[i];
        uint8_t next = data[i + 1];

        dst[i] = (current << ((i + 1) * 2)) | (next >> (6 - i * 2));
    }
}

size_t base64_decode(const char* src, void* dst) {
    size_t length = 0;
    for (const char* c = src; *c != '\0' && *c != '='; c++) {
        length++;
    }

    size_t chunk_count = length / 4;
    size_t size = chunk_count * 3;

    size_t remainder = length % 4;
    if (remainder > 0) {
        size += remainder - 1;
        chunk_count++;
    }

    if (dst) {
        for (size_t i = 0; i < chunk_count; i++) {
            size_t src_offset = i * 4;
            size_t dst_offset = i * 3;

            decode_chunk(src + src_offset, size - dst_offset, dst + dst_offset);
        }
    }

    return size;
}
