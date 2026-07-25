
#include "elos/user_event.h"

#include "elos/common/string.h"

#include "elos/keyboard.h"
#include "elos/cpu.h"
#include "elos/physical_memory.h"

#include "elos/kernel_console.h"


void EVE_init(BootAPI* boot_api) {
    KBD_init(boot_api);
}

volatile u32 g_userEventLock;

#define MAX_EVENT_BUFFERS 64

ELOS_UserEventBuffer* g_eventBuffers[MAX_EVENT_BUFFERS];


bool EVE_request_user_event_buffer(u32 maxEvents, ELOS_UserEventBuffer** buffer, u64* wholeBufferSize) {
    bool returnValue = false;
    LOCK_INT(&g_userEventLock);

    int freeIndex = -1;
    for (int i = 0; i < ARRAY_LENGTH(g_eventBuffers); i++) {
        if (!g_eventBuffers[i]) {
            freeIndex = i;
            break;
        }
    }
    if (freeIndex == -1) {
        goto exit;
    }
    
    u64 bufferSize = (sizeof(ELOS_UserEventBuffer) + maxEvents * sizeof(ELOS_UserEvent) + (PAGE_SIZE-1)) & (PAGE_SIZE-1);

    ELOS_UserEventBuffer* newBuffer = PMEM_alloc_phys(bufferSize, PMEM_FLAG_IDENTITY_MAPPED);
    if (!newBuffer) {
        goto exit;
    }

    memset(newBuffer, 0, bufferSize);
    newBuffer->head = 0;
    newBuffer->tail = 0;
    // Should we round up maxEvents or provide the exact amount user requested?
    // We round up for now. It's easier to change API later to provide exact amount if needed.
    *(u64*)&newBuffer->maxEvents = (bufferSize - sizeof(ELOS_UserEventBuffer)) / sizeof(ELOS_UserEvent);
    g_eventBuffers[freeIndex] = newBuffer;

    *wholeBufferSize = bufferSize;
    *buffer = newBuffer;
    returnValue = true;

exit:
    UNLOCK_INT(&g_userEventLock);
    return returnValue;
}


void EVE_push_event(ELOS_UserEvent* newEvent) {
    LOCK_INT(&g_userEventLock);

    for (int i = 0; i < ARRAY_LENGTH(g_eventBuffers); i++) {
        ELOS_UserEventBuffer* buffer = g_eventBuffers[i];
        if (!buffer) {
            continue;
        }

        // @TODO This is not thread safe or buffer overflow safe.
        //   If we write too much weird stuff happens. Fine for slow key presses
        //   but mouse movement would spam events. I can't use spinlock because
        //   user process should not block kernel thread.

        // @TODO We can't trust maxEvents. user may have put a wierd value there.

        u64 index = buffer->head % buffer->maxEvents;
        ELOS_UserEvent* event = &buffer->events[index];
        *event = *newEvent;
        buffer->head++;
        if (buffer->head % buffer->maxEvents == buffer->tail % buffer->maxEvents) {
            buffer->tail++;
        }
    }

exit:
    UNLOCK_INT(&g_userEventLock);
}

