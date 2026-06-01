#pragma once

#include "elos/common/types.h"

typedef struct {
    volatile u64 head;
    volatile u64 tail;
    u64 capacity;
    u8* buffer;
} RingBuffer;

typedef struct {
    u8* buffer;
    u64 buffer_size;
} SharedMemory;

typedef struct ServiceEndpoint ServiceEndpoint;

typedef struct {
    bool used;
    char name[64];
    u32 queueSize;
    ServiceEndpoint* serviceEndpoint;
    ServiceEndpoint* clientLinkedList;
    ServiceEndpoint* clientLinkedList_last;
} Service;

struct ServiceEndpoint {
    bool isService;
    Service* service;

    RingBuffer toService;
    RingBuffer toClient;
    u8*        phys_recvBuffer;
    u32        recvBuffer_size;

    ServiceEndpoint* nextEndpoint;
};

/*
    @TODO If the service crashes and restarts then we want applications to reconnect.
      Audio server crashes and all applications has to restart to get the new ServiceEndpoint...
      NOOO! that sucks. When restating and creating service again we reuse old endpoint?
      What if a malicious program is able to reuse and steal the endpoint?
      For audio maybe that's fine but you may have more important processes.
*/

bool SRV_service_create(const char* name, ServiceEndpoint** endpoint, u64 queueSize);

bool SRV_service_connect(const char* name, ServiceEndpoint** endpoint, u64 queueSize);

bool SRV_service_send(ServiceEndpoint* endpoint, const u8* data, u64 size);

bool SRV_service_recv(ServiceEndpoint* endpoint, ServiceEndpoint** senderEndpoint, u8** data, u64* size, u64 timeout_ns);


// @TODO shared memory under SRV (service)  doesn't make much sense.
//   Consider renaming SRV to IPC (inter-process communication).

bool SRV_shared_memory_create(u64 size, SharedMemory** handle);

bool SRV_shared_memory_grant(SharedMemory* handle, ServiceEndpoint* endpoint);

bool SRV_shared_memory_info(SharedMemory* handle, void** buffer, u64* size);




// ELOS_Error SYS_shm_create(u64 size, ELOS_SHMHandle* handle) {
//     ELOS_Error rax;
//     SYSCALL2(_SYS_SHM_CREATE, size, handle);
//     return rax;
// }

// ELOS_Error SYS_shm_grant(ELOS_SHMHandle handle, ELOS_ServiceEndpoint endpoint) {
//     ELOS_Error rax;
//     SYSCALL2(_SYS_SHM_GRANT, handle, endpoint);
//     return rax;
// }

// ELOS_Error SYS_shm_info(ELOS_SHMHandle handle, void** buffer, u64* size)  {
//     ELOS_Error rax;
//     SYSCALL3(_SYS_SHM_INFO, handle, buffer, size);
//     return rax;
// }

