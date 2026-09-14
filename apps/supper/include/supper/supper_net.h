#pragma once


typedef enum {
    MESSAGE_NONE,
    MESSAGE_LOCATION,
} MessageKind;

typedef struct {
    int id;
    float pos[3];
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
