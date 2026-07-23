/*
    These are some operations:

    ELOS_ASYNC_FILE_OPEN
        Look for a file in the Virtual File System specified by the path.
        A handle to that file is returned. Unless read-only is specified
        the file will be locked and cannot be opened by other processes until closed.
        When a file is locked it cannot be deleted.
        @TODO Can parent directory be moved? It cannot be deleted.


    ELOS_ASYNC_FILE_CLOSE,
    ELOS_ASYNC_FILE_READ,
    ELOS_ASYNC_FILE_WRITE,
    ELOS_ASYNC_FILE_INFO,

*/

#pragma once

#include "elos/common/types.h"

#include "elos/syscalls.h"

#define ASYNC_OK 0
#define ASYNC_GENERIC_ERROR 1


u32 ringMaskFromEntryCount(u32 count);

int ASYNC_create_async_rings(u32 maxEntries, ELOS_AsyncCreateFlag flags, ELOS_AsyncRequestRing** requestRing, ELOS_AsyncCompletionRing** completionRing);

int ASYNC_destroy_async_rings(ELOS_AsyncRequestRing* requestRing, ELOS_AsyncCompletionRing* completionRing, u32* maxEntries);

int ASYNC_submit_async_ring(ELOS_AsyncRequestRing* requestRing);

// int ASYNC_wait_async_ring(ELOS_AsyncCompletionRing* completionRing, u64 timeout_ns);


void ASYNC_handler();
