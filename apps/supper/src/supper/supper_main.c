/*
    @TODO I want to test networking and this is a game demo for that.

    Supper was the word that came to my mind when I made the folder. Means nothing.

    Networking API in ASYNC. Create UDP connection to a UDP server outside QEMU to linux server for the game?
    Run QEMU twice, have them connect to the server and see player move around?

    Spawn a bunch of moving, rotating cubes. how much can we handle?
    Test on laptop.





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

