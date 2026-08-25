#include "async_io.h"

#include "elos/common/intrinsics.h"

#include "stdlib.h"


int printf(const char* format, ...);

static bool g_initialized;
static ELOS_AsyncRequestRing* g_requestRing;
static ELOS_AsyncCompletionRing* g_completionRing;

static void async_init() {
    if (g_initialized) return;
    g_initialized = true;
    // @TODO mutex lock here

    ELOS_Error error;
    
    error = SYS_create_async_rings(4, 0, &g_requestRing, &g_completionRing);
    if (error != ELOS_OK) {
        printf("stdio: Could not create async rings\n");
        exit(1);
    }
}

static u32 g_nextRequestID;

Async_RequestID async_submit(ELOS_AsyncRequest* req) {
    ELOS_Error error;
    Async_RequestID requestID;

    async_init();

    // printf("Submitting %d\n", req->operation);

    requestID = __atomic_add_fetch(&g_nextRequestID, 1, __ATOMIC_SEQ_CST);
    req->userData = requestID;
    
    // Wait for free slot
    while ((g_requestRing->head - g_requestRing->tail) > g_requestRing->ringMask) {
        // @TODO Sleep and yield thread instead.
        pause();
    }
    g_requestRing->entries[g_requestRing->head & g_requestRing->ringMask] = *req;
    
    __atomic_thread_fence(__ATOMIC_RELEASE);
    __atomic_fetch_add(&g_requestRing->head, 1, __ATOMIC_SEQ_CST);



    error = SYS_submit_async_ring(g_requestRing);
    if (error != ELOS_OK) {
        printf("async_submit: SYS_submit_async_ring failed\n");
        return -1;
    }

    
    return requestID;
}

bool async_wait(Async_RequestID id, ELOS_AsyncCompletion* com, size_t timeout_ns) {
    
    ELOS_Error error;
    
    
    while (1) {
        error = SYS_wait_async_ring(g_completionRing, timeout_ns);
        if (error != ELOS_OK) {
            printf("async_wait: SYS_wait_async_ring '%s'\n", elos_error(error));
            return false;
        }

        
        volatile ELOS_AsyncCompletion* cqe = &g_completionRing->entries[g_completionRing->tail & g_completionRing->ringMask];
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        
        Async_RequestID requestID = cqe->userData;
        if (requestID != id) {
            printf("async_wait: Got request ID %d, expected %d\n", (int)requestID, (int)id);
            return false;
        }

        *com = *cqe;
        __atomic_fetch_add(&g_completionRing->tail, 1, __ATOMIC_SEQ_CST);
        break;
    }
    return true;
}

