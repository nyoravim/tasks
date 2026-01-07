#ifndef _GATEWAY_H
#define _GATEWAY_H

typedef struct gateway gateway_t;

/* from bot.h */
typedef struct bot bot_t;

gateway_t* gateway_open(const char* url, bot_t* bot);
void gateway_close(gateway_t* gw);

void gateway_poll(gateway_t* gw);

void gateway_start_session(gateway_t* gw, const char* id, const char* resume_url);

bot_t* gateway_get_bot(const gateway_t* gw);

#endif
