#ifndef _DISPATCH_H
#define _DISPATCH_H

#include <json.h>

/* from gateway.h */
typedef struct gateway gateway_t;

void dispatch_event(gateway_t* gw, const char* type, const json_object* data);

#endif
