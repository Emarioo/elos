
#include "prism/prism.h"
#include "prism/prism_protocol.h"

#define ELOS_SYSCALL_IMPL
#include "elos/syscalls.h"
#include "elos/common/intrinsics.h"

#include "elos/common/string.h"



typedef struct {
    int   surfaceId;
    int   x;
    int   y;
    int   width;
    int   height;
    int   stride;
    u64   size;
    void* buffer;
    ELOS_SharedMemoryHandle sharedMemoryHandle;

    ELOS_ServiceEndpoint ownerEndpoint; // @TODO This assumes endpoints can't be reused.
} Surface;



void printf(const char* fmt, ...);


void exit(int exitCode);

void prism_loop();

Surface* create_surface(int width, int height);
void destroy_surface(int surfaceID);
Surface* get_surface(int surfaceID);
void present_surface(int surfaceID);

ELOS_ServiceEndpoint serviceEndpoint;
ELOS_FrameBuffer monitorFrameBuffer;


void _start() {
    ELOS_Error error;

    error = SYS_service_create(PRISM_SERVICE_NAME, &serviceEndpoint, 4096);
    if (error != ELOS_OK) {
        printf("Could not start PRISM service\n");
        exit(1);
    }

    error = SYS_default_monitor(&monitorFrameBuffer);
    if (error != ELOS_OK) {
        printf("Could not get default monitor frame buffer\n");
        exit(1);
    }

    prism_loop();
}



void prism_loop() {
    ELOS_Error error;
    while (1) {
        const PrismMessage* message;
        u64 messageSize;
        ELOS_ServiceEndpoint senderEndpoint;

        error = SYS_service_recv(serviceEndpoint, &senderEndpoint, (const void**)&message, &messageSize, 0);
        if (error != ELOS_OK || !message) {
            // Nothing to do.
            pause();
            continue;
        }

        switch (message->type) {
            case PRISM_CREATE_SURFACE: {
                Surface* surface = create_surface(message->createSurface.width, message->createSurface.height);

                if (!surface) {
                    PrismMessage response = {
                        .type = PRISM_CREATE_SURFACE_RESPONSE,
                        .createSurfaceResponse = {
                            .sharedMemoryHandle = NULL,
                        },
                    };
                    error = SYS_service_send(senderEndpoint, &response, sizeof(response));
                    // Can't do much with an error.
                    break;
                }

                error = SYS_shared_memory_grant(surface->sharedMemoryHandle, senderEndpoint);
                if (error != ELOS_OK) {
                    destroy_surface(surface->surfaceId);
                    break;
                }

                surface->ownerEndpoint = senderEndpoint;

                PrismMessage response = {
                    .type = PRISM_CREATE_SURFACE_RESPONSE,
                    .createSurfaceResponse = {
                        .sharedMemoryHandle = surface->sharedMemoryHandle,
                        .surfaceID = surface->surfaceId,
                        .stride = surface->stride,
                    },
                };
                error = SYS_service_send(senderEndpoint, &response, sizeof(response));
                if (error != ELOS_OK) {
                    // Can't do much with an error.
                    destroy_surface(surface->surfaceId);
                    break;
                }
            } break;
            case PRISM_DESTROY_SURFACE: {
                Surface* surface = get_surface(message->destroySurface.surfaceID);
                if (!surface || surface->ownerEndpoint != senderEndpoint) {
                    break;
                }
                destroy_surface(message->destroySurface.surfaceID);
            } break;
            case PRISM_PRESENT_SURFACE: {
                Surface* surface = get_surface(message->destroySurface.surfaceID);
                if (!surface || surface->ownerEndpoint != senderEndpoint) {
                    break;
                }
                present_surface(surface->surfaceId);
            } break;
        }
    }

}

#define MAX_SURFACES 64

#define IS_SURFACE_VALID(SURFACE) (SURFACE->sharedMemoryHandle != ELOS_NULL_HANDLE)

Surface surfaces[MAX_SURFACES];



Surface* create_surface(int width, int height) {
    ELOS_Error error;

    u64 size = width * height * sizeof(u32);
    ELOS_SharedMemoryHandle handle;
    error = SYS_shared_memory_create(size, &handle);
    if (error != ELOS_OK) {
        return NULL;
    }
    void* buffer;
    error = SYS_shared_memory_info(handle, &buffer, NULL);
    if (error != ELOS_OK) {
        // @TODO Destroy shared memory
        return NULL;
    }
    
    int surfaceID;
    Surface* surface = NULL;
    for (int i=0;i<ARRAY_LENGTH(surfaces);i++) {
        Surface* surf = &surfaces[i];
        if (IS_SURFACE_VALID(surf)) {
            continue;
        }
        surface = surf;
        surface->surfaceId = i;
        break;
    }

    if (!surface) {
        // @TODO Destroy shared memory
        return NULL;
    }

    surface->buffer = buffer;
    surface->width = width;
    surface->stride = width;
    surface->height = height;
    surface->sharedMemoryHandle = handle;
    surface->size = size;
    surface->surfaceId = surfaceID;
    return surface;
}

void destroy_surface(int surfaceID) {
    Surface* surface = get_surface(surfaceID);
    if (!surface) {
        return;
    }
    *surface = (Surface){};
}

Surface* get_surface(int surfaceID) {
    Surface* surface = &surfaces[surfaceID];
    if (!IS_SURFACE_VALID(surface)) {
        return NULL;
    }
    return surface;
}


void present_surface(int surfaceID) {
    Surface* surface = get_surface(surfaceID);
    if (!surface) {
        return;
    }

    // @TODO Implement levels for each surface. The order which to draw them.
    // @TODO Implement double buffering to prevent tearing. 

    // surface->x = 450;
    // surface->y = 150;
    
    surface->x = 0;
    surface->y = 0;

    
    u32* src = surface->buffer;
    int x = surface->x;
    int y = surface->y;
    int w = surface->width;
    int h = surface->height;
    int src_stride = surface->stride;

   if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }

    u32  dst_width = monitorFrameBuffer.width;
    u32  dst_height = monitorFrameBuffer.height;
    if (x + w > dst_width)
        w = dst_width - x;
    if (y + h > dst_height)
        h = dst_height - y;

    uint32_t* const dst  = monitorFrameBuffer.pixels;
    uint32_t  const dst_stride  = monitorFrameBuffer.pixels_per_scan_line;

    // printf("x=%d y=%d w=%d h=%d dst_stride=%d src_stride=%d dst=%x src=%x\n", x, y, w, h, dst_stride, src_stride, dst, src);

    // @TODO We may want to write to an internal buffer which we then write to the frame buffer in one fell swoop.

    for (int iy = y; iy < y + h; iy++) {
        void* begin = &dst[iy * dst_stride];
        void* from = &src[(iy-y) * src_stride];
        int size = 4 * w;
        memcpy_fast(begin, from, size);
        
        // for (int ix = x; ix < x + w; ix++) {
        //     // printf("%d %d\n", ix, iy);
        //     dst[ix + iy * dst_stride] = src[(ix-x) + (iy-y) * src_stride];
        // }
    }

}




void exit(int exitCode) {
    // @TODO Implement exit syscall
    while (1) pause();
}