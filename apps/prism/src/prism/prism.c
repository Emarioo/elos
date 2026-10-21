
#include "prism/prism.h"
#include "prism/prism_protocol.h"

#include "elos/elos.h"
#include "elos/common/intrinsics.h"

#include "elos/common/string.h"

#include "stdlib.h"
#include "stdio.h"
#include "stdnet.h"

#include "stdui.h"

#include "vendor/stb_image.h"


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
    int   order;
    u64   mtime;

    ELOS_ProcessID ownerEndpoint; // @TODO This assumes process ids can't be reused.
} Surface;


void prism_loop();

Surface* create_surface(int width, int height);
void destroy_surface(int surfaceID);
Surface* get_surface(int surfaceID);
void present_surface(int surfaceID, int extra_x, int extra_y, int extra_w, int extra_h);


void init_rendered_surfaces();
void kill_inactive_surfaces();
void sort_surfaces();

ELOS_FrameBuffer monitorFrameBuffer;

void* monitor_backFrameBuffer;

ELOS_Net_Handle netHandle;

Texture* wallpaper;

PrismSurfaceInfo fake_surface;

#define MAX_SURFACES 64

#define IS_SURFACE_VALID(SURFACE) (SURFACE->sharedMemoryHandle != ELOS_NULL_HANDLE)

Surface surfaces[MAX_SURFACES];

u32 rendered_surfaces_len;
u32 rendered_surfaces_max;
Surface** rendered_surfaces;
int nextOrder = 0;

static u64 ticks_per_second;

void init_fake_surface() {
    
    monitor_backFrameBuffer = malloc(monitorFrameBuffer.size);
    if (!monitor_backFrameBuffer) {
        printf("prism: Could not get default monitor frame buffer\n");
        exit(1);
    }

    fake_surface.width = monitorFrameBuffer.width;
    fake_surface.height = monitorFrameBuffer.height;
    fake_surface.stride = monitorFrameBuffer.pixels_per_scan_line;
    fake_surface.buffer = monitor_backFrameBuffer;
    stdui_set_surface(&fake_surface);
}


void load_wallpaper() {
    const char* path = "/pkg/prism/back.png";
    wallpaper = load_texture(path);

    if (!wallpaper) {
        printf("prism: Could not read %s\n", path);
    }

    printf("Wallapper size: %d/%d KiB\n", (int)wallpaper->width, (int)wallpaper->height);

    // load pixels with stb_image



}

void present_back_frame(int x, int y, int w, int h);

void _start() {
    SYS_ticks_per_second(&ticks_per_second);


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

    init_fake_surface();
    init_rendered_surfaces();


    load_wallpaper();


    if (wallpaper) {
        // @TODO Do we want to render wallpaper here?
        draw_texture(0, 0, monitorFrameBuffer.width, monitorFrameBuffer.height, 0, 0, wallpaper->width, wallpaper->height, wallpaper);
    
        // Test sub texture drawing
        // draw_texture(0, 0, monitorFrameBuffer.width / 2 - 50, monitorFrameBuffer.height, 0, 0, wallpaper->width/2, wallpaper->height, wallpaper);
        // draw_texture(monitorFrameBuffer.width / 2 + 50, 0, monitorFrameBuffer.width / 2 - 50, monitorFrameBuffer.height, wallpaper->width / 2, 0, wallpaper->width/2, wallpaper->height, wallpaper);

        present_back_frame(0, 0, monitorFrameBuffer.width, monitorFrameBuffer.height);
    }

    prism_loop();

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

        // printf("prism: %d %d %d %d\n", message->type, message->moveSurface.surfaceID);
        
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

                if (rendered_surfaces_len >= rendered_surfaces_max) {
                    printf("prism: Rendered surface limit reached (%d)\n", rendered_surfaces_len);
                    break;
                }
                rendered_surfaces[rendered_surfaces_len] = surface;
                rendered_surfaces_len++;

                // printf("surf %p, %d\n", surface, rendered_surfaces_len);

                surface->order = nextOrder++;

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

                // printf("Surface %d %dx%d\n", surface->surfaceId, surface->width, surface->height);
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
                surface->mtime = rdtsc();

                // Kill surfaces first, because render list may be unordered afterwards.
                kill_inactive_surfaces();
                // NOW we sort.
                sort_surfaces();

                present_surface(surface->surfaceId, 0, 0, 0, 0);
            } break;
            case PRISM_MOVE_SURFACE: {
                Surface* surface = get_surface(message->moveSurface.surfaceID);
                if (!surface || surface->ownerEndpoint != message->processID) {
                    printf("prism: Bad surface %d\n", message->moveSurface.surfaceID);
                    break;
                }
                int min_x = min(surface->x, message->moveSurface.x);
                int min_y = min(surface->y, message->moveSurface.y);
                int extra_w = max(surface->x, message->moveSurface.x) + surface->width - min_x;
                int extra_h = max(surface->y, message->moveSurface.y) + surface->height - min_y;

                surface->x = message->moveSurface.x;
                surface->y = message->moveSurface.y;
                
                surface->mtime = rdtsc();

                // Kill surfaces first, because render list may be unordered afterwards.
                kill_inactive_surfaces();
                // NOW we sort.
                sort_surfaces();

                present_surface(surface->surfaceId, min_x, min_y, extra_w, extra_h);

                // printf("Move %d %dx%d\n", surface->surfaceId, surface->x, surface->y);
            } break;
        }
        // printf("prism: msg done\n");
    }

}


void init_rendered_surfaces() {
    rendered_surfaces_max = 128;
    rendered_surfaces = malloc(sizeof(Surface*) * rendered_surfaces_max);
    if (!rendered_surfaces) {
        printf("prism: Could not allocate array for rendered surfaces.\n");
        exit(1);
    }
    rendered_surfaces_len = 0;
}

int partition(Surface** arr, int low, int high)
{

    // choose the pivot
    Surface* pivot = arr[high];

    // undex of smaller element and indicates
    // the right position of pivot found so far
    int i = low - 1;

    // Traverse arr[low..high] and move all smaller
    // elements on left side. Elements from low to
    // i are smaller after every iteration
    for (int j = low; j <= high - 1; j++)
    {
        if (arr[j]->order > pivot->order)
        {
            i++;
            Surface* temp = arr[i];
            arr[i] = arr[j];
            arr[j] = temp;
        }
    }

    // move pivot after smaller elements and
    // return its position
    // swap(arr[i + 1], arr[high]);
    
    Surface* temp = arr[i + 1];
    arr[i + 1] = arr[high];
    arr[high] = temp;
    return i + 1;
}

// the QuickSort function implementation
void quickSort(Surface** arr, int low, int high)
{

    if (low < high)
    {

        // pi is the partition return index of pivot
        int pi = partition(arr, low, high);

        // recursion calls for smaller elements
        // and greater or equals elements
        quickSort(arr, low, pi - 1);
        quickSort(arr, pi + 1, high);
    }
}

void sort_surfaces() {

    quickSort(rendered_surfaces, 0, rendered_surfaces_len - 1);

}


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
    
    int surfaceID = -1;
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

void kill_inactive_surfaces() {
    u64 now = rdtsc();
    for (int si=0;si<rendered_surfaces_len;si++) {
        Surface* surface = rendered_surfaces[si];

        u64 elapsed_ms = (1000 * (now - surface->mtime)) / ticks_per_second;
        // printf("Age %d (%d ms)\n", si, (int)elapsed_ms);
        if (elapsed_ms > 5000) {
            // I would love to kill surfaces quickly but some apps like doom sets some stuff up
            // before presenting frames.

            // @TODO Make a list of killed surfaces then update canvas at the end.
            printf("Killed inactive surface %d (%d ms)\n", si, (int)elapsed_ms);
            rendered_surfaces[si] = rendered_surfaces[rendered_surfaces_len-1];
            rendered_surfaces_len--;
            si--;

            present_surface(surface->surfaceId, 0, 0, 0, 0);
        }
    }
}


void render_partial_surface(Surface* surface, int screen_x, int screen_y, int screen_w, int screen_h) {
    
    int left   = surface->x > screen_x ? surface->x : screen_x;
    int top    = surface->y > screen_y ? surface->y : screen_y;

    int right1  = surface->x + surface->width;
    int right2  = screen_x + screen_w;
    int right   = right1 < right2 ? right1 : right2;

    int bottom1 = surface->y + surface->height;
    int bottom2 = screen_y + screen_h;
    int bottom  = bottom1 < bottom2 ? bottom1 : bottom2;

    // Nothing to render if the rectangles don't overlap.
    if (right <= left || bottom <= top) {
        return;
    }

    // Calculate source offsets and visible dimensions.
    int sub_x = left - surface->x;
    int sub_y = top - surface->y;
    int sub_w = right - left;
    int sub_h = bottom - top;

    u32* src = surface->buffer;
    int src_stride = surface->stride;

    uint32_t* const dst = monitor_backFrameBuffer;
    uint32_t const dst_stride =
        monitorFrameBuffer.pixels_per_scan_line;

    for (int iy = 0; iy < sub_h; iy++) {
        uint32_t* dst_line =
            &dst[left + (top + iy) * dst_stride];

        u32* src_line =
            &src[sub_x + (sub_y + iy) * src_stride];

        memcpy(dst_line, src_line, (size_t)sub_w * sizeof(uint32_t));
    }
}

void present_surface(int surfaceID, int extra_x, int extra_y, int extra_w, int extra_h) {
    Surface* surface = get_surface(surfaceID);
    if (!surface) {
        return;
    }


    // @TODO Implement levels for each surface. The order which to draw them.
    
    // @TODO Implement double buffering to prevent tearing. 

    // @TODO Detect when a surface is completely covered and mark is non-visible.
    
    int x = surface->x;
    int y = surface->y;
    int w = surface->width;
    int h = surface->height;
    
    if (extra_w && extra_h) {
        x = extra_x;
        y = extra_y;
        w = extra_w;
        h = extra_h;
    }

    // printf("Present %d %d %d %d\n", x, y, w, h);
    
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }

    int dst_width = monitorFrameBuffer.width;
    int dst_height = monitorFrameBuffer.height;
    if (x + w > dst_width)
        w = dst_width - x;
    if (y + h > dst_height)
        h = dst_height - y;

    int sub_x = (x * wallpaper->width) / dst_width;
    int sub_y = (y * wallpaper->height) / dst_height;
    int sub_w = (w * wallpaper->width) / dst_width;
    int sub_h = (h * wallpaper->height) / dst_height;
    // printf("prism: %d %d %d %d\n", sub_x, sub_y, sub_w, sub_h);
    draw_texture(x, y, w, h, sub_x, sub_y, sub_w, sub_h, wallpaper);

    // printf("prism: %d %d %d %d\n", x, y, w, h);

    for (int si=0;si<rendered_surfaces_len;si++) {
        Surface* other_surface = rendered_surfaces[si];
        // printf("prism: %d %d, %d %d %d %d\n", si, other_surface->x, other_surface->y, other_surface->width, other_surface->height);
        render_partial_surface(other_surface, x, y, w, h);
    }

    present_back_frame(x, y, w, h);
}

void present_back_frame(int x, int y, int w, int h) {
    uint32_t* const dst  = monitorFrameBuffer.pixels;
    uint32_t  const dst_stride  = monitorFrameBuffer.pixels_per_scan_line;

    u32* src = monitor_backFrameBuffer;
    int src_stride = monitorFrameBuffer.pixels_per_scan_line;

    for (int iy = y; iy < y + h; iy++) {
        void* begin = &dst[x + iy * dst_stride];
        void* from = &src[x + iy * src_stride];
        int size = 4 * w;
        memcpy(begin, from, size);
    }
}
