#ifndef _DISPATCH_H
#define _DISPATCH_H

#include <json.h>

/* from bot.h */
typedef struct bot bot_t;

void dispatch_event(bot_t* bot, const char* type, const json_object* data);

#endif
