/*
    Prism client that provides implemention for prism API.
    The client talks to the Prism server using message passing and shared memory.
    
    Static library linked with applications.

    @TODO If we send a message and PRISM server crashes or shuts down when we do
       then we may think we destoyed or presented a surface but didn't.
       We need confirmation when presenting a surface but this sets up a delay.
*/

#include "prism/prism.h"
#include "prism/prism_protocol.h"

#include "elos/syscalls.h"

#include "elos/common/intrinsics.h"


struct PrismInstance {
    ELOS_ServiceEndpoint endpoint;
};

struct PrismSurface {
    int   surfaceID;
    int   width;
    int   height;
    int   stride;
    
    u64   size;
    void* buffer;
    ELOS_SharedMemory sharedMemoryHandle;

    PrismInstance* instance;
};

static PrismInstance g_instance;
static PrismSurface g_surface;

static u64 ticks_per_second;

PrismInstance* prism_init() {
    ELOS_Error error;

    if (g_instance.endpoint != ELOS_NULL_HANDLE) {
        // Already initialized
        return NULL;
    }

    SYS_ticks_per_second(&ticks_per_second);

    u64 start = rdtsc();
    while (1) {
        error = SYS_service_connect(PRISM_SERVICE_NAME, &g_instance.endpoint, 4096);
        if (error == ELOS_OK) {
            break;
        }
        u64 now = rdtsc();
        u64 ms = (1000 * (now - start)) / ticks_per_second;
        if (ms > 800) {
            // Could not connect.
            return NULL;
        }
    }
    
    return (PrismInstance*)&g_instance;
}


PrismSurface* prism_createSurface(PrismInstance* instance, int width, int height) {
    ELOS_Error error;

    if (g_surface.sharedMemoryHandle != ELOS_NULL_HANDLE) {
        // We only support one surface at the moment.
        return NULL;
    }

    PrismMessage message = {
        .type = PRISM_CREATE_SURFACE,
        .createSurface = {
            .width = width,
            .height = height,
        },
    };

    error = SYS_service_send(instance->endpoint, &message, sizeof(message));
    if (error != ELOS_OK) {
        return NULL;
    }

    const PrismMessage* response;
    u64 responseSize;

    // @TODO Timeout doesn't work at the moment. When it does use that instead of while loop.
    while (1) {
        error = SYS_service_recv(instance->endpoint, NULL, (const void**)&response, &responseSize, 0);
        if (error == ELOS_OK && response != NULL) {
            break;
        }
        pause();
    }

    if (response->type != PRISM_CREATE_SURFACE_RESPONSE) {
        return NULL;
    }
    if (response->createSurfaceResponse.sharedMemoryHandle == ELOS_NULL_HANDLE) {
        return NULL;
    }

    void* buffer;
    u64   size;

    error = SYS_shared_memory_info(response->createSurfaceResponse.sharedMemoryHandle, &buffer, &size);
    if (error != ELOS_OK) {
        PrismMessage message = {
            .type = PRISM_DESTROY_SURFACE,
            .destroySurface = {
                .surfaceID = response->createSurfaceResponse.surfaceID,
            },
        };

        error = SYS_service_send(instance->endpoint, &message, sizeof(message));
        // If send fails then we leak a surface.
        // Might be fine because SYS_shared_memory_info shouldn't fail if
        // handle is valid which it is if service sent it to us.
        return NULL;
    }

    PrismSurface* surface = &g_surface;

    surface->surfaceID = response->createSurfaceResponse.surfaceID;
    surface->width = width;
    surface->height = height;
    surface->stride = response->createSurfaceResponse.stride;
    surface->buffer = buffer;
    surface->size = size;
    surface->sharedMemoryHandle = response->createSurfaceResponse.sharedMemoryHandle;
    surface->instance = instance;

    return &g_surface;
}


void prism_destroySurface(PrismSurface* surface) {
    ELOS_Error error;

    PrismMessage message = {
        .type = PRISM_DESTROY_SURFACE,
        .destroySurface = {
            .surfaceID = surface->surfaceID,
        },
    };

    error = SYS_service_send(surface->instance->endpoint, &message, sizeof(message));
    // If send fails then we leak a surface.
    // However, we probably fail because service got shutdown.
    // In which case all resources got destroyed.
}


void prism_moveSurface(PrismSurface* surface, int x, int y) {
    ELOS_Error error;

    PrismMessage message = {
        .type = PRISM_MOVE_SURFACE,
        .moveSurface = {
            .surfaceID = surface->surfaceID,
            .x = x,
            .y = y,
        },
    };

    error = SYS_service_send(surface->instance->endpoint, &message, sizeof(message));
    if (error != ELOS_OK) {
        // What do we do?
    }
}

void prism_surfaceInfo(PrismSurface* surface, PrismSurfaceInfo* info) {
    *info = (PrismSurfaceInfo) { };
    info->buffer = surface->buffer;
    info->width = surface->width;
    info->height = surface->height;
    info->stride = surface->stride;
}


bool prism_presentSurface(PrismSurface* surface) {
    ELOS_Error error;

    PrismMessage message = {
        .type = PRISM_PRESENT_SURFACE,
        .presentSurface = {
            .surfaceID = surface->surfaceID,
        },
    };

    error = SYS_service_send(surface->instance->endpoint, &message, sizeof(message));
    return error == ELOS_OK;
}
