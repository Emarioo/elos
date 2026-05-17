
#include "stdint.h"


typedef uint64_t Handle;

#define PIPE_STATUS_ENABLED 1

typedef struct {
    void*    ring_buffer;
    uint32_t ring_buffer_size;
    volatile uint32_t status; // tells you whether write handle is shutdown.
    volatile uint32_t lock;
    volatile uint64_t head;
    volatile uint64_t tail;
    // volatile uint64_t head_reserve;
    // volatile uint64_t head_commit;
    // volatile uint64_t tail_reserve;
    // volatile uint64_t tail_commit;
} HandleData;

int create_ringBuffer(uint32_t size, Handle* out_handle);

int write(Handle handle, const void* buffer, uint64_t size);

int read(Handle handle, void* buffer, uint64_t size);

