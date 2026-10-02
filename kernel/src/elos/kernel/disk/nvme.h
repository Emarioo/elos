#pragma once

#include "elos/common/types.h"

#include "elos/kernel/disk/disk_internal.h"


#pragma pack(push, 1)
typedef struct {
    // 0x0
    uint64_t CAP;
    uint32_t VS;
    uint32_t INTMS;

    // 0x10
    uint32_t INTMC;
    uint32_t CC;
    uint32_t _reserved0;
    uint32_t CSTS;

    // 0x20
    uint32_t _reserved1;
    uint32_t AQA;
    uint64_t ASQ;

    // 0x30
    uint64_t ACQ;
} NVME_Registers;
#pragma pack(pop)

#define NVME_CC_BIT_ENABLE (1 << 0)

#define NVME_CAP_BIT_NVM_COMMAND_SET ((u64)1 << 37)

#define NVME_CSTS_BIT_READY (1 << 0)


typedef struct {
    u64 address;
    u64 size;
} NVME_Queue;


typedef struct {
    u32 command;
    u32 nsid;
    u32 _reserved0[2];
    u64 metadata_pointer;
    u64 data_pointer[2];
    u32 body[6];
} NVME_SubmissionQueueEntry;

typedef struct {
    u32 command;
    u32 _reserved0;
    u16 submission_queue_head_pointer;
    u16 submission_queue_identifier;
    u16 command_identifier;
    u16 status;
} NVME_CompletionQueueEntry;

#define NVME_HAS_PHASE_BIT(STATUS) ((STATUS) & 0x1)

typedef enum {
    NVME_CMD_CREATE_IO_SUBMISSION_QUEUE = 0x1,
    NVME_CMD_CREATE_IO_COMPLETION_QUEUE = 0x5,
    NVME_CMD_IDENTIFY                   = 0x6,
} NVME_Admin_Command;

typedef enum {
    NVME_CMD_WRITE = 0x1,
    NVME_CMD_READ  = 0x2,
} NVME_IO_Command;

typedef struct NVME_Context NVME_Context;
struct NVME_Context {
    volatile NVME_Registers* regs;

    u32 DRSTRD;
    u32 MPS;
    u32 MPSMIN;
    u32 MPSMAX;

    u32 first_nsid;
    u32 sectorSize;

    volatile u32 buffer_lock;

    void* tempSector;
    void* tempMemoryPage;

    u32 submission_tail;
    u32 completion_head;

    u32 io_submission_tail;
    u32 io_completion_head;

    NVME_Queue asq;
    NVME_Queue acq;

    NVME_Queue io_sq;
    NVME_Queue io_cq;
};


bool nvme_init(DiskDevice* device);
bool nvme_read(DiskDevice* device, u64 byteOffset, u64 byteSize, void* buffer);
bool nvme_write(DiskDevice* device, u64 byteOffset, u64 byteSize, const void* buffer);
