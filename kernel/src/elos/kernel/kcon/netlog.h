#pragma once

#include <stdint.h>



#define NETLOG_MAGIC "NETL"

#define NETLOG_DEFAULT_PORT 5999


#define NETLOG_COMMAND_CLEAR 1
#define NETLOG_COMMAND_DATA  2

typedef struct {
    char     magic[4];
    uint8_t  command;
    uint16_t size;
    int      sequence;
    char     payload[];
} NetLog_Header;


