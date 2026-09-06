/*
    @TODO

    Consider memory allocation for line text. Current approach is frail.
        Realloc or allocate more text buffer memory.
        Since the memory is used in lines we can't realloc because it would
        invalidate the memory.
        We would need to allocate new memory, go through all lines and copy text
        to new buffer then free old buffer.
        Or we can have a linked list of memory chunks we can allocate from.
        A heap allocator implementation in user land that is optimized
        for small strings may be a good idea.
        Or some other good idea for small strings.

    Opening big files. How large files do we want to support. 512MB?
        1GB but special optimizations for memory with read-only limitation?

    Show column and line number where cursor is, for multiple cursors pick first cursor?
        Scroll in X direction. important for long lines
        Pick text_content_x based on how many lines are in the file. Large numbers (1002) touches the text area.

    Insert, delete, replace
    Undo, redo
    Multiple cursors
    Text selection
    Copy and paste
    Syntax highlight
    Handle large files (1G)
    Search, regex matching and replace
    Hex editor

    IN ELOS implement repeat kernel service.
    Don't use repeat events from the keyboard device.
    Make our own where we can choose repeat delay and frequency.
    We can also support multiple repeats. For example you hold down left and up arrow. Only up arrow is repeated.
    You release up arrow and nothing happens even though left arrow is still down. This we can fix with repeat kernel service.

Tests
    Open /pkg/slate/slate.elf and replace .rodata strings with something else and save.
    Restart the program and see if different text is printed.

*/

#include "supper/supper.h"

#include "prism/prism.h"
#include "stdui.h"



PrismInstance*        g_instance;
PrismSurface*         g_surface;
PrismSurfaceInfo      g_surfaceInfo;
u64                   g_ticks_per_second;
ELOS_UserEventBuffer* g_userEvents;
SupperSession         g_supperSession;

bool get_event(ELOS_UserEvent* event) {
    // @TODO Not thread or context switch safe.
    u32 tail = g_userEvents->tail % g_userEvents->maxEvents;
    u32 head = g_userEvents->head % g_userEvents->maxEvents;
    if (tail == head) {
        return false;
    }
    *event = g_userEvents->events[tail];
    g_userEvents->tail++;
    return true;
}


void _start() {

    // @NOCHECKIN Temporary
    // dumpdir("/", 0);

    SYS_ticks_per_second(&g_ticks_per_second);



    g_instance = prism_init();
    if (!g_instance) {
        printf("slate: Could not init PRISM client\n");
        exit(1);
    }

    g_surface = prism_createSurface(g_instance, 800, 600);
    if (!g_surface) {
        printf("slate: Could not create surface\n");
        exit(1);
    }

    prism_surfaceInfo(g_surface, &g_surfaceInfo);

    prism_moveSurface(g_surface, 200, 200);

    ELOS_Error error;
    error = SYS_request_user_event_buffer(100, &g_userEvents);
    if (error != ELOS_OK) {
        printf("slate: Could not create user event buffer\n");
        exit(1);
    }

    stdui_set_surface(&g_surfaceInfo);

    game_loop();
}

