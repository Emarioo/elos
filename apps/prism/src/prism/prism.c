
#include "prism/prism.h"
#include "prism/prism_protocol.h"

#include "elos/elos.h"
#include "elos/common/intrinsics.h"

#include "elos/common/string.h"

#include "stdlib.h"
#include "stdio.h"
#include "stdnet.h"


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

    ELOS_ProcessID ownerEndpoint; // @TODO This assumes process ids can't be reused.
} Surface;


void prism_loop();

Surface* create_surface(int width, int height);
void destroy_surface(int surfaceID);
Surface* get_surface(int surfaceID);
void present_surface(int surfaceID);

ELOS_FrameBuffer monitorFrameBuffer;

ELOS_Net_Handle netHandle;


void _start() {
    ELOS_Error error;

    ELOS_Net_Address address = {0};
    address.protocol = ELOS_NET_PROTO_UDP_IPV4;
    address.udp_tcp4.address = net_ipv4_from_str("127.0.0.1");
    address.udp_tcp4.port = PRISM_PROTOCOL_PORT;

    error = net_open(&address, &netHandle);
    if (error != ELOS_OK) {
        // Happens if another instance is open
        printf("prism: Could not start service at port %u\n", address.udp_tcp4.port);
        exit(1);
    }

    error = SYS_default_monitor(&monitorFrameBuffer);
    if (error != ELOS_OK) {
        printf("prism: Could not get default monitor frame buffer\n");
        exit(1);
    }

    prism_loop();

    // printf("prism quit\n");

    exit(1);
}



void prism_loop() {
    ELOS_Error error;
    while (1) {
        char _message_buffer[512];
        u32 messageSize = sizeof(_message_buffer);

        ELOS_Net_Address senderAddress = {0};

        // printf("prism: Try read\n");

        error = net_read(netHandle, &senderAddress, &_message_buffer, &messageSize, 0);
        if (error == ELOS_ERR_TIMEOUT) {
            // Nothing to do.
            pause();
            continue;
        } else if (error != ELOS_OK) {
            printf("prism: net_read error %s\n", elos_error(error));
            break;
        }

        const PrismMessage* message = (const PrismMessage*)_message_buffer;

        // @TODO Add magic number to message. To prevent accidental messages from doing stuff.
        //    In theory it should be fine, prism should be sturdy and not allow super large surface width but still.

        // printf("prism: %d %d %d %d\n", message->type, message->moveSurface.surfaceID, message->moveSurface.x, message->moveSurface.y);
        
        switch (message->type) {
            case PRISM_INVALID: break;
            case PRISM_PING_RESPONSE:
            case PRISM_CREATE_SURFACE_RESPONSE: {
                printf("prism: Received response type %d which shouldn't happen.\n", message->type);
            } break;
            case PRISM_PING: {
                PrismMessage response = {
                    .type = PRISM_PING_RESPONSE,
                };
                error = net_write(netHandle, &senderAddress, &response, sizeof(response));
                // Can't do much with an error.
            } break;
            case PRISM_CREATE_SURFACE: {
                Surface* surface = create_surface(message->createSurface.width, message->createSurface.height);

                if (!surface) {
                    PrismMessage response = {
                        .type = PRISM_CREATE_SURFACE_RESPONSE,
                        .createSurfaceResponse = {
                            .sharedMemoryHandle = NULL,
                        },
                    };
                    error = net_write(netHandle, &senderAddress, &response, sizeof(response));
                    // Can't do much with an error.
                    break;
                }

                if (message->processID == 0) {
                    printf("prism: Client didn't provide process ID. Can't share memory buffer.\n");
                    break;
                }

                error = SYS_shared_memory_grant(surface->sharedMemoryHandle, message->processID);
                if (error != ELOS_OK) {
                    printf("Could not grant\n");
                    destroy_surface(surface->surfaceId);
                    break;
                }

                surface->ownerEndpoint = message->processID;

                PrismMessage response = {
                    .type = PRISM_CREATE_SURFACE_RESPONSE,
                    .createSurfaceResponse = {
                        .sharedMemoryHandle = surface->sharedMemoryHandle,
                        .surfaceID = surface->surfaceId,
                        .stride = surface->stride,
                    },
                };
                error = net_write(netHandle, &senderAddress, &response, sizeof(response));
                if (error != ELOS_OK) {
                    printf("Could not net_write response\n");
                    // Can't do much with an error.
                    destroy_surface(surface->surfaceId);
                    break;
                }
            } break;
            case PRISM_DESTROY_SURFACE: {
                Surface* surface = get_surface(message->destroySurface.surfaceID);
                // @TODO Any client can provide any processID and shutdown any surface if they guess.
                //   We need better authorization.
                if (!surface || surface->ownerEndpoint != message->processID) {
                    printf("prism: Could not destroy\n");
                    break;
                }
                destroy_surface(message->destroySurface.surfaceID);
            } break;
            case PRISM_PRESENT_SURFACE: {
                Surface* surface = get_surface(message->presentSurface.surfaceID);
                if (!surface || surface->ownerEndpoint != message->processID) {
                    printf("prism: Could not present\n");
                    break;
                }
                present_surface(surface->surfaceId);
            } break;
            case PRISM_MOVE_SURFACE: {
                Surface* surface = get_surface(message->moveSurface.surfaceID);
                if (!surface || surface->ownerEndpoint != message->processID) {
                    printf("prism: Bad surface %d\n", message->moveSurface.surfaceID);
                    break;
                }
                surface->x = message->moveSurface.x;
                surface->y = message->moveSurface.y;
            } break;
        }
    }

}

#define MAX_SURFACES 64

#define IS_SURFACE_VALID(SURFACE) (SURFACE->sharedMemoryHandle != ELOS_NULL_HANDLE)

Surface surfaces[MAX_SURFACES];



Surface* create_surface(int width, int height) {
    ELOS_Error error;

    size_t size = width * height * sizeof(u32);
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
        surfaceID = i;
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
        void* begin = &dst[x + iy * dst_stride];
        void* from = &src[(iy-y) * src_stride];
        int size = 4 * w;
        memcpy(begin, from, size);
        
        // for (int ix = x; ix < x + w; ix++) {
        //     // printf("%d %d\n", ix, iy);
        //     dst[ix + iy * dst_stride] = src[(ix-x) + (iy-y) * src_stride];
        // }
    }

}


