/*

    Prism protocol for message passing between server and client.

*/

#pragma once

#include "elos/syscalls.h"


#define PRISM_SERVICE_NAME "prism"

typedef enum {
    PRISM_CREATE_SURFACE = 1,
    PRISM_CREATE_SURFACE_RESPONSE,
    
    PRISM_DESTROY_SURFACE,

    PRISM_PRESENT_SURFACE,

    PRISM_MOVE_SURFACE, // @TODO Response?
} PrismMessageType;

typedef struct {
    PrismMessageType type;
    union {
        struct {
            int width;
            int height;
        } createSurface;
        struct {
            int surfaceID;
            int stride;
            ELOS_SharedMemory sharedMemoryHandle;
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