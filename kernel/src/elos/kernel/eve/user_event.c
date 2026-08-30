
#include "elos/user_event.h"

#include "elos/common/string.h"

#include "elos/keyboard.h"
#include "elos/cpu.h"
#include "elos/physical_memory.h"

#include "elos/kernel_console.h"

#define printf(...) KCON_printf(__VA_ARGS__)

typedef struct {
    ELOS_UserEventBuffer* userEventBuffer;    
    u32 maxEvents;
    volatile u32 head;
} KernelEventBuffer;

void EVE_init(BootAPI* boot_api) {
    KBD_init(boot_api);
}

volatile u32 g_userEventLock;

#define MAX_EVENT_BUFFERS 64

KernelEventBuffer g_eventBuffers[MAX_EVENT_BUFFERS];

#define EVENT_BUFFER_PRESENT(BUF) ((BUF)->userEventBuffer)

bool EVE_request_user_event_buffer(u32 maxEvents, ELOS_UserEventBuffer** buffer, u32* wholeBufferSize) {
    bool returnValue = false;
    LOCK_INT(&g_userEventLock);

    int freeIndex = -1;
    for (int i = 0; i < ARRAY_LENGTH(g_eventBuffers); i++) {
        if (!EVENT_BUFFER_PRESENT(&g_eventBuffers[i])) {
            freeIndex = i;
            break;
        }
    }
    if (freeIndex == -1) {
        goto exit;
    }

    if (maxEvents > 10000) {
        // @TODO Return ELOS_Error not bool.
        printf("User tried requesting buffer with %d events.\n", maxEvents);
        goto exit;
    }

    KernelEventBuffer* eventBuffer = &g_eventBuffers[freeIndex];
    
    u32 bufferSize = (sizeof(ELOS_UserEventBuffer) + maxEvents * sizeof(ELOS_UserEvent) + (PAGE_SIZE-1)) & (PAGE_SIZE-1);

    eventBuffer->maxEvents = (bufferSize - sizeof(ELOS_UserEventBuffer)) / sizeof(ELOS_UserEvent);
    eventBuffer->head = 0;

    ELOS_UserEventBuffer* newBuffer = PMEM_alloc_phys(bufferSize, PMEM_FLAG_IDENTITY_MAPPED);
    if (!newBuffer) {
        goto exit;
    }

    memset(newBuffer, 0, bufferSize);
    newBuffer->head = 0;
    newBuffer->tail = 0;
    *(u32*)&newBuffer->maxEvents = eventBuffer->maxEvents;

    eventBuffer->userEventBuffer = newBuffer;

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
        KernelEventBuffer* buffer = &g_eventBuffers[i];
        if (!EVENT_BUFFER_PRESENT(buffer)) {
            continue;
        }

        // @TODO This is not thread safe or buffer overflow safe.
        //   If we write too much weird stuff happens. Fine for slow key presses
        //   but mouse movement would spam events. I can't use spinlock because
        //   user process should not block kernel thread.

        // @TODO We can't trust maxEvents. user may have put a wierd value there.

        u64 index = buffer->head % buffer->maxEvents;
        ELOS_UserEvent* event = &buffer->userEventBuffer->events[index];
        *event = *newEvent;
        buffer->head++;
        buffer->userEventBuffer->head = buffer->head;

        // @TODO The idea behind this is to increment tail so it "drags" behind the head
        //   But I am pretty sure this doesn't work in practise with multiple threads because user
        //   May have an old tail and a new head. Maybe we need to update tail first then head to make
        //   sure tail is never behind head? User must then read head first and then tail?
        //   Any other good way to handle event overflow? Requiring user to process them and not overflow
        //   doesn't seem nice.
        if (buffer->head % buffer->maxEvents == buffer->userEventBuffer->tail % buffer->maxEvents) {
            buffer->userEventBuffer->tail++;
        }
    }

exit:
    UNLOCK_INT(&g_userEventLock);
}

