/*

    Prism protocol for message passing between server and client.

*/

#pragma once

#include "elos/elos.h"


#define PRISM_SERVICE_NAME "prism"

typedef enum {
    PRISM_INVALID = 0,

    PRISM_PING,
    PRISM_PING_RESPONSE,

    PRISM_CREATE_SURFACE,
    PRISM_CREATE_SURFACE_RESPONSE,
    
    PRISM_DESTROY_SURFACE,

    PRISM_PRESENT_SURFACE,

    PRISM_MOVE_SURFACE, // @TODO Response?
} PrismMessageType;

typedef struct {
    PrismMessageType type;
    ELOS_ProcessID processID;
    union {
        struct {
            int width;
            int height;
        } createSurface;
        struct {
            int surfaceID;
            int stride;
            ELOS_SharedMemoryHandle sharedMemoryHandle;
        } createSurfaceResponse;
        struct {
            int surfaceID;
        } destroySurface;
        struct {
            int surfaceID;
        } presentSurface;
        struct {
            int surfaceID;
            int x, y;
        } moveSurface;
    };
} PrismMessage;