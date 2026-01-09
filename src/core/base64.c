#include "base64.h"

#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#include <string.h>

#include <nyoravim/mem.h>

/* im using http encoding just to be safe */

static bool to_base64(uint8_t number, char* c) {
    if (number >= 64) {
        return false;
    }

    /* A-Z starts at 0 */
    if (number < 26) {
        *c = (char)number + 'A';
        return true;
    }

    /* a-z starts at 26 */
    if (number < 52) {
        *c = (char)(number - 26) + 'a';
        return true;
    }

    /* 0-9 starts at 52 */
    if (number < 62) {
        *c = (char)(number - 52) + '0';
        return true;
    }

    /* + or - = 62 */
    if (number == 62) {
        *c = '-';
        return true;
    }

    /* / or _ = 63 */
    *c = '_';
    return true;
}

static bool from_base64(char c, uint8_t* value) {
    /* A-Z starts at 0 */
    if (c >= 'A' && c <= 'Z') {
        *value = (uint8_t)(c - 'A');
        return true;
    }

    /* a-z starts at 26 */
    if (c >= 'a' && c <= 'z') {
        *value = (uint8_t)(c - 'a') + 26;
        return true;
    }

    /* 0-9 starts at 52 */
    if (c >= '0' && c <= '9') {
        *value = (uint8_t)(c - '0') + 52;
        return true;
    }

    switch (c) {
    case '+':
    case '-':
        *value = 62;
        return true;
    case '/':
    case '_':
        *value = 63;
        return true;
    default:
        return false;
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
            uint8_t mask = 0xFF >> (8 - i * 2);
            uint8_t data = src[i - 1] & mask;
            value |= data << (6 - i * 2);
        }

        if (i < size) {
            value |= src[i] >> ((i + 1) * 2);
        }

        assert(to_base64(value, &dst[i]));
    }
}

char* base64_encode(const void* src, size_t size) {
    size_t chunk_count = size / 3;
    if (size % 3 != 0) {
        chunk_count++;
    }

    size_t output_length = chunk_count * 4;
    char* dst = nv_alloc(output_length + 1);

    assert(dst);
    dst[output_length] = '\0';

    for (size_t i = 0; i < chunk_count; i++) {
        size_t src_offset = i * 3;
        size_t dst_offset = i * 4;

        encode_chunk(src + src_offset, size - src_offset, dst + dst_offset);
    }

    return dst;
}

static bool decode_chunk(const char* src, size_t size, uint8_t* dst) {
    uint8_t data[size + 1];
    for (size_t i = 0; i < size + 1; i++) {
        if (!from_base64(src[i], &data[i])) {
            return false;
        }
    }

    if (dst) {
        for (size_t i = 0; i < size; i++) {
            uint8_t current = data[i];
            uint8_t next = data[i + 1];

            uint8_t value = 0;
            value |= current << ((i + 1) * 2);
            value |= next >> (4 - i * 2);

            dst[i] = value;
        }
    }

    return true;
}

size_t base64_decode(const char* src, void* dst) {
    if (!src) {
        return 0;
    }

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

    for (size_t i = 0; i < chunk_count; i++) {
        size_t src_offset = i * 4;
        size_t dst_offset = i * 3;

        if (!decode_chunk(src + src_offset, size - dst_offset, dst ? dst + dst_offset : NULL)) {
            /* decoding failed */
            return 0;
        }
    }

    return size;
}
