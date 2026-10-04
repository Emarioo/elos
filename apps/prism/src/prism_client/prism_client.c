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

#include "elos/common/intrinsics.h"
#include "elos/elos.h"
#include "stdnet.h"
#include "stdio.h"


#define PRISM_CLIENT_PROTOCOL_PORT_START (PRISM_PROTOCOL_PORT + 1)
#define PRISM_CLIENT_PROTOCOL_PORT_END   (PRISM_PROTOCOL_PORT + 100)

struct PrismInstance {
    bool             started;
    ELOS_ProcessID   my_processID;
    ELOS_Net_Address my_address;
    ELOS_Net_Address target_address;
    ELOS_Net_Handle  netHandle;
};

struct PrismSurface {
    int   surfaceID;
    int   width;
    int   height;
    int   stride;
    
    u64   size;
    void* buffer;
    ELOS_SharedMemoryHandle sharedMemoryHandle;

    PrismInstance* instance;
};

static PrismInstance g_instance;
static PrismSurface g_surface;

static u64 ticks_per_second;

static ELOS_Error prism_send(const void* buffer, u32 size);
static ELOS_Error prism_send_recv(const void* buffer, u32 size, void* out_buffer, u32* out_bufferSize);


PrismInstance* prism_init() {
    ELOS_Error error;
    PrismInstance* returnValue = NULL;

    if (g_instance.started) {
        // Already initialized
        return NULL;
    }

    SYS_ticks_per_second(&ticks_per_second);


    // The PRISM endpoint
    g_instance.target_address.protocol = ELOS_NET_PROTO_UDP_IPV4;
    g_instance.target_address.udp_tcp4.address = net_ipv4_from_str("127.0.0.1");
    g_instance.target_address.udp_tcp4.port = PRISM_PROTOCOL_PORT;


    // Find a free port for our prism client
    u16 attemptPort = PRISM_CLIENT_PROTOCOL_PORT_START;
    while (1) {
        g_instance.my_address.protocol = ELOS_NET_PROTO_UDP_IPV4;
        g_instance.my_address.udp_tcp4.address = net_ipv4_from_str("127.0.0.1");
        g_instance.my_address.udp_tcp4.port = attemptPort;
        
        error = net_open(&g_instance.my_address, &g_instance.netHandle);
        if (error == ELOS_OK) {
            break;
        }
        attemptPort++;
        if (attemptPort > PRISM_CLIENT_PROTOCOL_PORT_END) {
            printf("prism_client: Could not find free port between [%u, %u]", PRISM_CLIENT_PROTOCOL_PORT_START, PRISM_CLIENT_PROTOCOL_PORT_END);
            goto exit;
        }
    }


    // Ping PRISM endpoint to see if it exists.
    u64 start = rdtsc();
    while (1) {
        PrismMessage response = {
            .type = PRISM_PING,
        };
        error = net_write(g_instance.netHandle, &g_instance.target_address, &response, sizeof(response));
        if (error == ELOS_OK) {
            char _message_buffer[512];
            u32 messageSize = sizeof(_message_buffer);

            ELOS_Net_Address senderAddress = {0};
            error = net_read(g_instance.netHandle, &senderAddress, &_message_buffer, &messageSize, 0);
            if (error == ELOS_OK) {
                const PrismMessage* message = (const PrismMessage*)_message_buffer;
                if (message->type == PRISM_PING_RESPONSE) {
                    // PRISM endpoint exists.
                    break;
                }
            }
            
        }
        
        // On boot PRISM may not be started yet so we give it some time.
        u64 now = rdtsc();
        u64 ms = (1000 * (now - start)) / ticks_per_second;
        if (ms > 800) {
            // Could not connect.
            goto exit;
        }
        pause();
    }
    
    // All is clear.
    g_instance.my_processID = SYS_process_id();
    g_instance.started = true;
    returnValue = &g_instance;
    
exit:
    if (!returnValue) {
        if (g_instance.netHandle) {
            net_close(g_instance.netHandle);
            g_instance.netHandle = 0;
        }
    }
    return returnValue;
}


PrismSurface* prism_createSurface(PrismInstance* instance, int width, int height) {
    ELOS_Error error;

    if (g_surface.sharedMemoryHandle != ELOS_NULL_HANDLE) {
        // We only support one surface at the moment.
        return NULL;
    }

    PrismMessage message = {
        .type = PRISM_CREATE_SURFACE,
        .processID = g_instance.my_processID,
        .createSurface = {
            .width = width,
            .height = height,
        },
    };

    char _message_buffer[512];
    u32 responseSize = sizeof(_message_buffer);

    error = prism_send_recv(&message, sizeof(message), &_message_buffer, &responseSize);
    if (error != ELOS_OK) {
        return NULL;
    }
    const PrismMessage* response = (const PrismMessage*)_message_buffer;

    if (response->type != PRISM_CREATE_SURFACE_RESPONSE) {
        return NULL;
    }
    if (response->createSurfaceResponse.sharedMemoryHandle == ELOS_NULL_HANDLE) {
        return NULL;
    }

    void*  buffer;
    size_t size;

    error = SYS_shared_memory_info(response->createSurfaceResponse.sharedMemoryHandle, &buffer, &size);
    if (error != ELOS_OK) {
        PrismMessage message = {
            .type = PRISM_DESTROY_SURFACE,
            .processID = g_instance.my_processID,
            .destroySurface = {
                .surfaceID = response->createSurfaceResponse.surfaceID,
            },
        };

        error = prism_send(&message, sizeof(message));
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
        .processID = g_instance.my_processID,
        .destroySurface = {
            .surfaceID = surface->surfaceID,
        },
    };

    error = prism_send(&message, sizeof(message));
    // If send fails then we leak a surface.
    // However, we probably fail because service got shutdown.
    // In which case all resources got destroyed.
}


void prism_moveSurface(PrismSurface* surface, int x, int y) {
    ELOS_Error error;

    PrismMessage message = {
        .type = PRISM_MOVE_SURFACE,
        .processID = g_instance.my_processID,
        .moveSurface = {
            .surfaceID = surface->surfaceID,
            .x = x,
            .y = y,
        },
    };

    error = prism_send(&message, sizeof(message));
}

void prism_surfaceInfo(PrismSurface* surface, PrismSurfaceInfo* info) {
    *info = (PrismSurfaceInfo) { };
    info->buffer = surface->buffer;
    info->width = surface->width;
    info->height = surface->height;
    info->stride = surface->stride;
}


void prism_presentSurface(PrismSurface* surface) {
    ELOS_Error error;

    PrismMessage message = {
        .type = PRISM_PRESENT_SURFACE,
        .processID = g_instance.my_processID,
        .presentSurface = {
            .surfaceID = surface->surfaceID,
        },
    };

    error = prism_send(&message, sizeof(message));
}


static ELOS_Error prism_send(const void* buffer, u32 size) {
    ELOS_Error error;
    error = net_write(g_instance.netHandle, &g_instance.target_address, buffer, size);
    return error;
}

static ELOS_Error prism_send_recv(const void* buffer, u32 size, void* out_buffer, u32* out_bufferSize) {
    ELOS_Error error;
    
    error = net_write(g_instance.netHandle, &g_instance.target_address, buffer, size);
    if (error != ELOS_OK) {
        return error;
    }

    u64 start = rdtsc();
    while (1) {
        ELOS_Net_Address senderAddress;
        error = net_read(g_instance.netHandle, &senderAddress, out_buffer, out_bufferSize, 0);
        
        if (error == ELOS_OK) {
            const PrismMessage* message = (const PrismMessage*)out_buffer;
            if (message->type != PRISM_PING_RESPONSE) {
                // we got message
                break;
            }
            // it's a duplicate ping, keep trying
        } else if (error != ELOS_ERR_TIMEOUT) {
            // error
            break;
        } else {
            // timeout, keep trying
        }
        
        u64 now = rdtsc();
        u64 ms = (1000 * (now - start)) / ticks_per_second;
        if (ms > 800) {
            // timeout
            break;
        }

        pause();
    }

    return error;
}
