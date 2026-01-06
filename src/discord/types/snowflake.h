#ifndef _SNOWFLAKE_H
#define _SNOWFLAKE_H

#include <stdint.h>
#include <stdbool.h>

#include <json.h>

bool snowflake_parse(uint64_t* id, json_object* data);
json_object* snowflake_serialize(uint64_t id);

#endif
