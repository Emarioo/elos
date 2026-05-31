
#include "compositor.h"

#define ELOS_SYSCALL_IMPL
#include "elos/syscalls.h"
#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include <stdarg.h>

void printf(const char* fmt, ...);
void test_messaging();

u64 tsc_per_sec;

void sleep(u64 ns) {
    u64 start = rdtsc();
    while (1) {
        u64 now = rdtsc();
        if (now - start > (tsc_per_sec*ns)/1000000000 )
            break;
        pause();
    }
}


void _start() {
    
    printf("Start compositor\n");
    SYS_ticks_per_second(&tsc_per_sec);

    test_messaging();

    while (1) {
        printf("Running compositor\n");
        sleep(200*1000000);
    }

    work();

    while (1) pause();
}

void work() {

    // Loop and perform compositing work.
    // listen to move,resize events.

}


void test_messaging() {

    ELOS_Error error;
    ELOS_ServiceEndpoint endpoint;
    
    error = SYS_service_create("compositor", &endpoint, 0x1000);
    if (error != ELOS_OK) {
        printf("compositor: Could not create service.\n");

        while (1) pause();
    }
    
    printf("compositor: Created service.\n");

    typedef struct {
        char magic[4];
        ELOS_SharedMemoryHandle handle;
    } CompositorHeader;

    
    typedef struct {
        volatile u32 term_counter;
        volatile u32 comp_counter;
        volatile u32 both_counter;
    } SHM;

    SHM* sharedMemories[10] = {0};
    int sharedMemories_len = 0;

    while (1) {
        ELOS_ServiceEndpoint senderEndpoint;
        const u8* data;
        u64 size;
        error = SYS_service_recv(endpoint, &senderEndpoint, &data, &size, 0);
        if (data) {
            if (size >= 5 && !memcmp(data, "hello", 5)) {
                ELOS_SharedMemoryHandle handle;
                error = SYS_shared_memory_create(0x1000, &handle); 
                if (error != ELOS_OK) {
                    printf("compositor: SYS_shared_memory_create failed, %d\n", error);
                } else {
                    SYS_shared_memory_grant(handle, senderEndpoint); // grant not implemented but always succeeds.

                    void* memory;
                    u64 memory_size;
                    error = SYS_shared_memory_info(handle, &memory, NULL);
                    if (error != ELOS_OK) {
                        printf("compositor: SYS_shared_memory_info failed, %d\n", error);
                    } else {
                        memset(memory, 0, sizeof(SHM));
                        sharedMemories[sharedMemories_len] = memory;
                        sharedMemories_len++;
                        
                        CompositorHeader header = {
                            .magic = "COMP",
                            .handle = handle,
                        };
                        printf("compositor: Sent shared memory handle\n");
                        error = SYS_service_send(senderEndpoint, (u8*)&header, sizeof(header));
                        if (error != ELOS_OK) {
                            printf("compositor: SYS_service_send failed, %d\n", error);
                        }
                    }
                }
            }
        }

        for (int i=0;i<ARRAY_LENGTH(sharedMemories);i++) {
            SHM* shm = sharedMemories[i];
            if (!shm) {
                continue;
            }

            __atomic_fetch_add(&shm->comp_counter, 1, __ATOMIC_SEQ_CST);
            __atomic_fetch_add(&shm->both_counter, 1, __ATOMIC_SEQ_CST);
            
            printf("compositor: [%d] %d %d %d\n", i, shm->term_counter, shm->comp_counter, shm->both_counter);
        }

        sleep(10*1000000);
    }
}

