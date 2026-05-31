
#include "elos/service.h"

#include "elos/cpu.h"
#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/physical_memory.h"



#define MESSAGE_HEADER_LENGTH sizeof(u16)
#define MAX_MESSAGE_SIZE ((1 << (8*MESSAGE_HEADER_LENGTH))-1)


#define MAX_SERVICES 128

Service g_services[MAX_SERVICES];

volatile u32 g_service_lock;


void ringbuf_write(RingBuffer* ringBuffer, const void* data, u64 size) {
    uint64_t head = ringBuffer->head % ringBuffer->capacity;
    if (size + head > ringBuffer->capacity) {
        memcpy((char*)ringBuffer->buffer + head, data, ringBuffer->capacity - head);
        memcpy((char*)ringBuffer->buffer, (char*)data + ringBuffer->capacity - head, size - (ringBuffer->capacity - head));
    } else {
        memcpy((char*)ringBuffer->buffer + head, data, size);
    }
    ringBuffer->head = (ringBuffer->head + size) % ringBuffer->capacity;
}

void ringbuf_read(RingBuffer* ringBuffer, void* data, u64 size) {
    uint64_t tail = ringBuffer->tail % ringBuffer->capacity;
    if (size + tail > ringBuffer->capacity) {
        memcpy((char*)data, (char*)ringBuffer->buffer + tail, ringBuffer->capacity - tail);
        memcpy((char*)data + ringBuffer->capacity - tail, (char*)ringBuffer->buffer, size - (ringBuffer->capacity - tail));
    } else {
        memcpy((char*)data, (char*)ringBuffer->buffer + tail, size);
    }
    ringBuffer->tail = (ringBuffer->tail + size) % ringBuffer->capacity;
}


Service* service_lookup(const char* name) {
    for (int i=0;i<MAX_SERVICES;i++) {
        Service* service = &g_services[i];
        if (!service->used)
            continue;
        if (strcmp(service->name, name))
            continue;
        return service;
    }
    return NULL;
}
Service* service_create(const char* name) {
    int name_len = strlen(name);
    if (name_len > sizeof(((Service*)NULL)->name) - 1) {
        // Name to big
        return NULL;
    }

    Service* avail_service = NULL;
    for (int i=0;i<MAX_SERVICES;i++) {
        Service* service = &g_services[i];
        if (!service->used) {
            if (!avail_service) {
                avail_service = service;
            }
            continue;
        }
        if (!strcmp(service->name, name)) {
            // Service already exists
            return false;
        }
    }
    if (avail_service) {
        avail_service->used = true;
        memcpy(avail_service->name, name, name_len);
    }
    return avail_service;
}

#define MAX_ENDPOINTS 128
ServiceEndpoint g_endpoints[MAX_ENDPOINTS];
int g_endpoints_len;

ServiceEndpoint* service_create_endpoint() {
    if (g_endpoints_len >= MAX_ENDPOINTS)
        return NULL;
    ServiceEndpoint* endpoint = &g_endpoints[g_endpoints_len];
    g_endpoints_len++;
    return endpoint;
}


bool SRV_service_create(const char* name, ServiceEndpoint** endpoint, u64 queueSize) {
    bool returnValue = false;
    LOCK_INT(&g_service_lock);

    Service* service = service_create(name);
    if (!service) {
        goto exit;
    }
    ServiceEndpoint* newEndpoint = service_create_endpoint();
    if (!newEndpoint) {
        // @TODO Free service using a function?
        service->used = false;
        goto exit;
    }

    // @TODO Set queueSize limit. Maybe capability in syscall layer checks, sets, controls this.

    service->queueSize = queueSize;
    service->serviceEndpoint = newEndpoint;
    service->clientLinkedList = NULL;

    newEndpoint->service = service;
    newEndpoint->isService = true;

    if (queueSize <= MAX_MESSAGE_SIZE)
        newEndpoint->recvBuffer_size = queueSize;
    else
        newEndpoint->recvBuffer_size = MAX_MESSAGE_SIZE;
    newEndpoint->phys_recvBuffer = PMEM_alloc_phys(newEndpoint->recvBuffer_size, PMEM_FLAG_IDENTITY_MAPPED);

    *endpoint = newEndpoint;
    returnValue = true;

exit:
    UNLOCK_INT(&g_service_lock);
    return returnValue;
}

bool SRV_service_connect(const char* name, ServiceEndpoint** endpoint, u64 queueSize) {
    bool returnValue = false;
    LOCK_INT(&g_service_lock);

    Service* service = service_lookup(name);
    if (!service) {
        goto exit;
    }
    ServiceEndpoint* newEndpoint = service_create_endpoint();
    if (!newEndpoint) {
        // @TODO Free service using a function?
        service->used = false;
        goto exit;
    }
    newEndpoint->service = service;
    newEndpoint->isService = false;

    if (queueSize <= MAX_MESSAGE_SIZE)
        newEndpoint->recvBuffer_size = queueSize;
    else
        newEndpoint->recvBuffer_size = MAX_MESSAGE_SIZE;
    newEndpoint->phys_recvBuffer = PMEM_alloc_phys(newEndpoint->recvBuffer_size, PMEM_FLAG_IDENTITY_MAPPED);

    newEndpoint->toClient.capacity = queueSize;
    newEndpoint->toClient.buffer = PMEM_alloc(queueSize);
    newEndpoint->toClient.head = 0;
    newEndpoint->toClient.tail = 0;

    newEndpoint->toService.capacity = service->queueSize;
    newEndpoint->toService.buffer = PMEM_alloc(service->queueSize);
    newEndpoint->toService.head = 0;
    newEndpoint->toService.tail = 0;

    newEndpoint->nextEndpoint = service->clientLinkedList;
    service->clientLinkedList = newEndpoint;
    if (!service->clientLinkedList_last) {
        service->clientLinkedList_last = newEndpoint;
    }
    service->clientLinkedList_last->nextEndpoint = newEndpoint;

    *endpoint = newEndpoint;
    returnValue = true;

exit:
    UNLOCK_INT(&g_service_lock);
    return returnValue;
}

bool SRV_service_send(ServiceEndpoint* endpoint, ServiceEndpoint* senderEndpoint, const u8* data, u64 size) {
    if (size > MAX_MESSAGE_SIZE || size <= 0)
        return false;

    bool returnValue = false;
    LOCK_INT(&g_service_lock);


    // @TODO Validate endpoint handle/pointer. Maybe we can do it in syscall instead?
    //    A user process may accidently or intentionally pass another process's endpoint that
    //    they guessed the address/id of or received from the other process through file or other method.
    //    Very strange of course but we must validated that endpoint actually exists and that it is owned
    //    by the user process making the syscall.

    RingBuffer* buffer;
    if (endpoint->isService) {
        buffer = &endpoint->toClient;
    } else {
        buffer = &endpoint->toService;
    }

    u32 available;
    if (buffer->head >= buffer->tail) {
        available = buffer->head - buffer->tail + buffer->capacity;
    } else {
        available = buffer->tail - buffer->head;
    }
    
    // We check less than or equal because filling up buffer fully makes it ambiguous whether it's empty or full since head == tail.
    // So we can never be ful land if head == tail then we're empty.
    if (available <= MESSAGE_HEADER_LENGTH + size)
        goto exit;

    u16 payloadSize = size;
    ringbuf_write(buffer, &payloadSize, sizeof(payloadSize));
    ringbuf_write(buffer, data, payloadSize);

    returnValue = true;

exit:
    UNLOCK_INT(&g_service_lock);
    return returnValue;
}

void dbg() {

}

bool SRV_service_recv(ServiceEndpoint* _endpoint, ServiceEndpoint** senderEndpoint, u8** data, u64* size, u64 timeout_ns) {
    bool returnValue = false;
    LOCK_INT(&g_service_lock);

    if (senderEndpoint) {
        *senderEndpoint = NULL;
    }
    *data = NULL;
    *size = 0;

    // @TODO Validate endpoint handle/pointer. Maybe we can do it in syscall instead?

    RingBuffer* buffer;
    if (_endpoint->isService) {
        Service* service = _endpoint->service;
        ServiceEndpoint* firstEndpoint = service->clientLinkedList;
        while (1) {
            ServiceEndpoint* endpoint = service->clientLinkedList;
            if (!endpoint) {
                // No endpoints
                goto exit;
            }
            buffer = &endpoint->toService;
            
            u32 available;
            if (buffer->head >= buffer->tail) {
                available = buffer->head - buffer->tail;
            } else {
                available = buffer->tail - buffer->head + buffer->capacity;
            }

            if (available <= 2) {
                if (available > 0 && available <= 2) {
                    kernel_bug();
                    buffer->tail = (buffer->tail + 2) % buffer->capacity;
                }

                // Check next endpoint
                service->clientLinkedList_last = service->clientLinkedList;
                service->clientLinkedList = service->clientLinkedList->nextEndpoint;

                if (firstEndpoint == service->clientLinkedList) {
                    goto exit;
                }
                continue;
            }

            dbg();

            u16 payloadSize;
            ringbuf_read(buffer, &payloadSize, sizeof(payloadSize));
            ringbuf_read(buffer, endpoint->phys_recvBuffer, payloadSize);

            *data = endpoint->phys_recvBuffer; // should be identity mapped
            *size = payloadSize;
            *senderEndpoint = endpoint;

            service->clientLinkedList_last = service->clientLinkedList;
            service->clientLinkedList = service->clientLinkedList->nextEndpoint;

            returnValue = true;
            break;
        }
    } else {
        ServiceEndpoint* endpoint = _endpoint;
        buffer = &endpoint->toClient;

        u32 available;
        if (buffer->head >= buffer->tail) {
            available = buffer->head - buffer->tail;
        } else {
            available = buffer->tail - buffer->head + buffer->capacity;
        }

        if (available <= 0) {
            // Nothing to read
            goto exit;
        }
        if (available <= 2) {
            kernel_bug();
            buffer->tail = (buffer->tail + 2) % buffer->capacity;
            goto exit;
        }

        u16 payloadSize;
        ringbuf_read(buffer, &payloadSize, sizeof(payloadSize));
        ringbuf_read(buffer, data, payloadSize);

        *data = endpoint->phys_recvBuffer; // should be identity mapped
        *size = payloadSize;

        returnValue = true;
    }

exit:
    UNLOCK_INT(&g_service_lock);
    return returnValue;
}
