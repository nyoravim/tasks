#ifndef _BASE64_H
#define _BASE64_H

#include <stddef.h>

/* return allocated with nv_alloc */
char* base64_encode(const void* src, size_t size);

/* dst can be null. if not null, expects enough memory to fill, as returned */
size_t base64_decode(const char* src, void* dst);

#endif
