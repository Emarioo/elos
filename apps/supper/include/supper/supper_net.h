#pragma once


typedef enum {
    MESSAGE_NONE,
    MESSAGE_LOCATION,
} MessageKind;

typedef struct {
    u32 id;
    float pos[3];
    float rot[4];
} PlayerLocation;

typedef struct {
    MessageKind kind;
    union {
        struct {
            int count;
            PlayerLocation locations[/* count */];
        } location;
    };
} MessageHeader;
