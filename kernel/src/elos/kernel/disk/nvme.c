
#include "elos/kernel/disk/nvme.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/physical_memory.h"
#include "elos/kernel_console.h"

#define printf(...) KCON_printf(__VA_ARGS__)


static inline void write_submission_tail(NVME_Context* context, u32 identifier, u32 value) {
    u64  base = (u64)context->regs + 0x1000;
    u32* ptr = (u32*)(base + (2 * identifier + 0) * context->DRSTRD);
    *ptr = value;
}
static inline void write_completion_head(NVME_Context* context, u32 identifier, u64 value) {
    u64  base = (u64)context->regs + 0x1000;
    u32* ptr = (u32*)(base + (2 * identifier + 1) * context->DRSTRD);
    *ptr = value;
}

bool init_admin_queues(NVME_Context* context);
bool init_io_queues(NVME_Context* context);
bool send_identify(NVME_Context* context, DiskDevice* device);

_align(4096) static char identify_data[4096];


bool nvme_init(DiskDevice* device) {

    // @TODO Each NVME controller has namespaces. Each namespace
    //   can be a disk device, similar to ahci.
    
    PCI_ConfigSpace* config = &device->nvme.configSpace;

    void* memory_bar = (void*)((u64)(config->header0.bar0 & ~0xF) | ((u64)config->header0.bar1 << 32));

    bool mapped = PMEM_map_memory(g_kernelPageTable, memory_bar, memory_bar, 4 * PAGE_SIZE, PMEM_FLAG_NOT_CACHED);
    if (!mapped) {
        printf("Could not map NVMe PCI memory\n");
        return false;
    }

    volatile NVME_Registers* regs = memory_bar;

    NVME_Context* context = PMEM_alloc(PAGE_SIZE);
    memset(context, 0, sizeof(*context));
    context->regs = regs;
    device->nvme.nvme_context = context;

    u32 majorVersion = (regs->VS >> 16) & 0xFFFF;
    u32 minorVersion = (regs->VS >> 8) & 0xFF;
    u32 DRSTRD = 4 << ((regs->CAP >> 32) & 0xF);
    u32 MPSMIN = 0x1000 << ((regs->CAP >> 48) & 0xF);
    u32 MPSMAX = 0x1000 << ((regs->CAP >> 52) & 0xF);
    u32 MQES = ((regs->CAP) & 0xFFFF) + 1;
    u32 MPS = 0x1000 << ((regs->CC >> 7) & 0xF);

    printf("BAR0 %p\n", memory_bar);
    printf("  CAP 0x%zx\n", regs->CAP);
    printf("  VS 0x%x\n", regs->VS);
    printf("  INTMS 0x%x\n", regs->INTMS);
    printf("  INTMC 0x%x\n", regs->INTMC);
    printf("  CC 0x%x\n", regs->CC);
    printf("  CSTS 0x%x\n", regs->CSTS);
    printf("  AQA 0x%x\n", regs->AQA);
    printf("  ASQ 0x%zx\n", regs->ASQ);
    printf("  ACQ 0x%zx\n", regs->ACQ);
    
    printf("\n");

    printf("  DRSTRD %u\n", DRSTRD);
    printf("  MPS %u\n", MPS);
    printf("  MQES %u\n", MQES);
    printf("  NVMe v%u.%u\n", majorVersion, minorVersion);

    printf("\n");

    context->DRSTRD = DRSTRD;
    context->MPS = MPS;
    

    // Reset controller
    regs->CC = (regs->CC & ~NVME_CC_BIT_ENABLE);

    // Wait until controller can be enabled again
    while (regs->CSTS & NVME_CSTS_BIT_READY) {
        pause();
    }

    // Configure some controller settings

    u32 wanted_MPS = 0x1000;
    if (wanted_MPS < MPSMIN || wanted_MPS > MPSMAX) {
        printf("nvme: MPS %d is not supported (we don't handle any other)\n", wanted_MPS);
        return false;
    }

    if (0 == (regs->CAP & NVME_CAP_BIT_NVM_COMMAND_SET)) {
        printf("nvme: NVM Command Set not supported.\n");
        return false;
    }

    // 16 byte completion entry size, 64 byte submission entry size
    // bits we don't set indicate: round robin AMS, 4K MPS,
    //   NVM command set, no shutdown notification
    regs->CC = (4 << 20) | (6 << 16);

    write_completion_head(context, 0, 0);
    write_submission_tail(context, 0, 0);


    bool yes;

    yes = init_admin_queues(context);
    if (!yes) {
        // @TODO Free resources
        printf("Could not make admin queues\n");
        return false;
    }
    
    regs->CC = regs->CC | NVME_CC_BIT_ENABLE;

    // Wait until controller is enabled
    while (0 == (regs->CSTS & NVME_CSTS_BIT_READY)) {
        pause();
    }

    printf("nvme: Enabled\n");

    yes = send_identify(context, device);
    if (!yes) {
        // @TODO Free resources
        printf("Could not identify NVME\n");
        return false;
    }

    yes = init_io_queues(context);
    if (!yes) {
        // @TODO Free resources
        printf("Could not make IO queues\n");
        return false;
    }


    return true;
}

bool nvme_read(DiskDevice* device, u64 byteOffset, u64 byteSize, void* buffer) {
    
    u32 queue_identifier = 1;

    NVME_Context* context = device->nvme.nvme_context;

    NVME_SubmissionQueueEntry* entry = (void*)(context->io_sq.address + context->io_submission_tail * sizeof(NVME_SubmissionQueueEntry));
    memset(entry, 0, sizeof(*entry));

    // @TODO Handle alignment on the buffer?
    //   We read a block at a time.
    //   If we specified less, then we need temporary buffer.

    u64 lba = byteOffset / context->blockSize;
    u64 num_blocks = byteSize / context->blockSize;

    entry->command = NVME_CMD_READ;
    entry->data_pointer[0] = (u64)buffer;
    entry->body[0] = lba;
    entry->body[1] = lba >> 32;
    entry->body[3] = num_blocks;
    entry->nsid = context->first_nsid;

    context->io_submission_tail++;
    write_submission_tail(context, queue_identifier, context->io_submission_tail);
    // @NOCHECKIN Is this correct?
    if (context->io_submission_tail == context->io_sq.size + 1) {
        context->io_submission_tail = 0;
    }

    NVME_CompletionQueueEntry* completion = (void*)(context->io_cq.address + context->io_completion_head * sizeof(NVME_CompletionQueueEntry));

    // printf("Waiting for nvme cqe phase bit...\n");
    while (1) {
        int phaseBit = NVME_HAS_PHASE_BIT(completion->status);
        if (!phaseBit) {
            continue;
        }

        // printf("Completed.\n");
        // printf("  command 0x%x\n", completion->command);
        // printf("  submission_q_head %d\n", completion->submission_queue_head_pointer);
        // printf("  submission_q_id %d\n", completion->submission_queue_identifier);
        // printf("  command_id %d\n", completion->command_identifier);
        // printf("  status 0x%x\n", completion->status);

        context->io_completion_head++;
        write_completion_head(context, queue_identifier, context->io_completion_head);
        if (context->io_completion_head == context->io_sq.size + 1) {
            context->io_completion_head = 0;
        }

        break;
    }
    int status = completion->status >> 1;
    if (status != 0) {
        printf("nvme: could not read %d %x\n", status, status);
    }

    return status == 0;
}

bool nvme_write(DiskDevice* device, u64 byteOffset, u64 byteSize, const void* buffer) {
    
    u32 queue_identifier = 1;

    NVME_Context* context = device->nvme.nvme_context;

    NVME_SubmissionQueueEntry* entry = (void*)(context->io_sq.address + context->io_submission_tail * sizeof(NVME_SubmissionQueueEntry));
    memset(entry, 0, sizeof(*entry));

    // @TODO Handle alignment on the buffer?
    //   We read a block at a time.
    //   If we specified less, then we need temporary buffer.

    u64 lba = byteOffset / context->blockSize;
    u64 num_blocks = byteSize / context->blockSize;

    entry->command = NVME_CMD_WRITE;
    entry->data_pointer[0] = (u64)buffer;
    entry->body[0] = lba;
    entry->body[1] = lba >> 32;
    entry->body[3] = num_blocks;
    entry->nsid = context->first_nsid;

    context->io_submission_tail++;
    write_submission_tail(context, queue_identifier, context->io_submission_tail);
    // @NOCHECKIN Is this correct?
    if (context->io_submission_tail == context->io_sq.size + 1) {
        context->io_submission_tail = 0;
    }

    NVME_CompletionQueueEntry* completion = (void*)(context->io_cq.address + context->io_completion_head * sizeof(NVME_CompletionQueueEntry));

    // printf("Waiting for nvme cqe phase bit...\n");
    while (1) {
        int phaseBit = NVME_HAS_PHASE_BIT(completion->status);
        if (!phaseBit) {
            continue;
        }

        // printf("Completed.\n");
        // printf("  command 0x%x\n", completion->command);
        // printf("  submission_q_head %d\n", completion->submission_queue_head_pointer);
        // printf("  submission_q_id %d\n", completion->submission_queue_identifier);
        // printf("  command_id %d\n", completion->command_identifier);
        // printf("  status 0x%x\n", completion->status);

        context->io_completion_head++;
        write_completion_head(context, queue_identifier, context->io_completion_head);
        if (context->io_completion_head == context->io_sq.size + 1) {
            context->io_completion_head = 0;
        }

        break;
    }
    int status = completion->status >> 1;
    if (status != 0) {
        printf("nvme: could not read %d %x\n", status, status);
    }

    return status == 0;
}



bool init_admin_queues(NVME_Context* context) {
	context->acq.address = (uint64_t)PMEM_alloc(PAGE_SIZE);
	if (context->acq.address == 0)
		return false;
    memset((void*)context->acq.address, 0, PAGE_SIZE);
	context->acq.size = 63;
    context->regs->ACQ = context->acq.address;

	context->asq.address = (uint64_t)PMEM_alloc(PAGE_SIZE);
	if (context->asq.address == 0)
		return false;
    memset((void*)context->asq.address, 0, PAGE_SIZE);
	context->asq.size = 63;
    context->regs->ASQ = context->asq.address;

    context->regs->AQA = (context->asq.size & 0xFFF) | ((context->acq.size & 0xFFF) << 16);

	return true;
}


bool send_identify(NVME_Context* context, DiskDevice* device) {

    NVME_SubmissionQueueEntry* entry = (void*)(context->asq.address + context->submission_tail * sizeof(NVME_SubmissionQueueEntry));
    memset(entry, 0, sizeof(*entry));

    entry->command = NVME_CMD_IDENTIFY;
    entry->data_pointer[0] = (u64)&identify_data;
    entry->body[0] = 1; // the controller, 0 = namespace, 2 = namespace list

    context->submission_tail++;
    write_submission_tail(context, 0, context->submission_tail);
    // @NOCHECKIN Is this correct?
    if (context->submission_tail == context->asq.size + 1) {
        context->submission_tail = 0;
    }

    NVME_CompletionQueueEntry* completion = (void*)(context->acq.address + context->completion_head * sizeof(NVME_CompletionQueueEntry));

    // printf("Waiting for nvme cqe phase bit...\n");
    while (1) {
        int phaseBit = NVME_HAS_PHASE_BIT(completion->status);
        if (!phaseBit) {
            continue;
        }

        // printf("Completed.\n");
        // printf("  command 0x%x\n", completion->command);
        // printf("  submission_q_head %d\n", completion->submission_queue_head_pointer);
        // printf("  submission_q_id %d\n", completion->submission_queue_identifier);
        // printf("  command_id %d\n", completion->command_identifier);
        // printf("  status 0x%x\n", completion->status);

        context->completion_head++;
        write_completion_head(context, 0, context->completion_head);
        if (context->completion_head == context->asq.size + 1) {
            context->completion_head = 0;
        }

        break;
    }

    char* modelNumber = identify_data + 24;
    snprintf(device->diskInfo.name, sizeof(device->diskInfo.name),
        "%s", modelNumber);

    u32 MDTS = identify_data[77];

    
    {

        NVME_SubmissionQueueEntry* entry = (void*)(context->asq.address + context->submission_tail * sizeof(NVME_SubmissionQueueEntry));
        memset(entry, 0, sizeof(*entry));

        entry->command = NVME_CMD_IDENTIFY;
        entry->data_pointer[0] = (u64)&identify_data;
        entry->body[0] = 2;
        entry->nsid = 0;

        context->submission_tail++;
        write_submission_tail(context, 0, context->submission_tail);
        // @NOCHECKIN Is this correct?
        if (context->submission_tail == context->asq.size + 1) {
            context->submission_tail = 0;
        }

        NVME_CompletionQueueEntry* completion = (void*)(context->acq.address + context->completion_head * sizeof(NVME_CompletionQueueEntry));

        // printf("Waiting for nvme cqe phase bit...\n");
        while (1) {
            int phaseBit = NVME_HAS_PHASE_BIT(completion->status);
            if (!phaseBit) {
                continue;
            }

            // printf("Completed.\n");
            // printf("  command 0x%x\n", completion->command);
            // printf("  submission_q_head %d\n", completion->submission_queue_head_pointer);
            // printf("  submission_q_id %d\n", completion->submission_queue_identifier);
            // printf("  command_id %d\n", completion->command_identifier);
            // printf("  status 0x%x\n", completion->status);

            context->completion_head++;
            write_completion_head(context, 0, context->completion_head);
            if (context->completion_head == context->asq.size + 1) {
                context->completion_head = 0;
            }

            break;
        }

        u32* nsids = (u32*)identify_data;
        int index = 0;
        while (1) {
            u32 nsid = nsids[index];
            if (nsid == 0) {
                break;
            }
            printf("Namespace %d\n", nsid);
            index++;

            if (!context->first_nsid) {
                context->first_nsid = nsid;
            }
        }
        
        if (!context->first_nsid) {
            // @TODO Free resources
            printf("nvme: Zero namespaces found.\n");
            return false;
        }
    }
    
    {

        NVME_SubmissionQueueEntry* entry = (void*)(context->asq.address + context->submission_tail * sizeof(NVME_SubmissionQueueEntry));
        memset(entry, 0, sizeof(*entry));

        entry->command = NVME_CMD_IDENTIFY;
        entry->data_pointer[0] = (u64)&identify_data;
        entry->body[0] = 0;
        entry->nsid = context->first_nsid;

        context->submission_tail++;
        write_submission_tail(context, 0, context->submission_tail);
        // @NOCHECKIN Is this correct?
        if (context->submission_tail == context->asq.size + 1) {
            context->submission_tail = 0;
        }

        NVME_CompletionQueueEntry* completion = (void*)(context->acq.address + context->completion_head * sizeof(NVME_CompletionQueueEntry));

        // printf("Waiting for nvme cqe phase bit...\n");
        while (1) {
            int phaseBit = NVME_HAS_PHASE_BIT(completion->status);
            if (!phaseBit) {
                continue;
            }

            // printf("Completed.\n");
            // printf("  command 0x%x\n", completion->command);
            // printf("  submission_q_head %d\n", completion->submission_queue_head_pointer);
            // printf("  submission_q_id %d\n", completion->submission_queue_identifier);
            // printf("  command_id %d\n", completion->command_identifier);
            // printf("  status 0x%x\n", completion->status);

            context->completion_head++;
            write_completion_head(context, 0, context->completion_head);
            if (context->completion_head == context->asq.size + 1) {
                context->completion_head = 0;
            }

            break;
        }

        u64  NSIZE = *(u64*)(identify_data + 0);
        u64  NCAP  = *(u64*)(identify_data + 8);
        u8   NLBAF = *(u8*)(identify_data + 25);
        u8   FLBAS = *(u8*)(identify_data + 26);
        u32* LBAFS = (u32*)(identify_data + 128);

        u32 has_metadata = FLBAS & 0x10;
        u32 lbaf_index = FLBAS & 0xF;

        u32 MS = (LBAFS[lbaf_index] & 0xFFFF);
        u32 LBADS = (LBAFS[lbaf_index] >> 16) & 0xFF;

        if (has_metadata && MS != 0) {
            // @TODO Do we need to handle metadata somehow?
            printf("nvme: Namespace has metadata which we don't handle.\n");
            return false;
        }

        context->blockSize = 1 << LBADS;
        // printf("Block Size %d\n", context->blockSize);
    }

    return true;
}

bool init_io_queues(NVME_Context* context) {
	context->io_cq.address = (uint64_t)PMEM_alloc(PAGE_SIZE);
	if (context->io_cq.address == 0)
		return false;
    memset((void*)context->io_cq.address, 0, PAGE_SIZE);
	// context->io_cq.size = (PAGE_SIZE / sizeof(NVME_CompletionQueueEntry)) - 1;
	context->io_cq.size = 63;

	context->io_sq.address = (uint64_t)PMEM_alloc(PAGE_SIZE);
	if (context->io_sq.address == 0)
		return false;
    memset((void*)context->io_sq.address, 0, PAGE_SIZE);
	// context->io_sq.size = (PAGE_SIZE / sizeof(NVME_SubmissionQueueEntry)) - 1;
	context->io_sq.size = 63;

    u32 io_queue_id_first = 1;

    #define QUEUE_FLAG_PHYSICALLY_CONTIGUOUS (1 << 0)
    #define QUEUE_FLAG_ENABLE_INTERRUPTS (1 << 1)

    {
        NVME_SubmissionQueueEntry* entry = (void*)(context->asq.address + context->submission_tail * sizeof(NVME_SubmissionQueueEntry));
        memset(entry, 0, sizeof(*entry));
    
        entry->command = NVME_CMD_CREATE_IO_COMPLETION_QUEUE;
        entry->data_pointer[0] = context->io_cq.address;
        entry->body[0] = (io_queue_id_first) | (context->io_cq.size << 16);
        // high word is interrupt vector. 0 is reserved for admin queues.
        // entry->body[1] = QUEUE_FLAG_PHYSICALLY_CONTIGUOUS | (1 << 16);
        entry->body[1] = QUEUE_FLAG_PHYSICALLY_CONTIGUOUS;
        
        context->submission_tail++;
        write_submission_tail(context, 0, context->submission_tail);
        // @NOCHECKIN Is this correct?
        if (context->submission_tail == context->asq.size + 1) {
            context->submission_tail = 0;
        }

        NVME_CompletionQueueEntry* completion = (void*)(context->acq.address + context->completion_head * sizeof(NVME_CompletionQueueEntry));

        // printf("Waiting for nvme cqe phase bit...\n");
        while (1) {
            int phaseBit = NVME_HAS_PHASE_BIT(completion->status);
            if (!phaseBit) {
                continue;
            }

            int status = completion->status >> 1;

            // printf("Completed.\n");
            // printf("  command 0x%x\n", completion->command);
            // printf("  submission_q_head %d\n", completion->submission_queue_head_pointer);
            // printf("  submission_q_id %d\n", completion->submission_queue_identifier);
            // printf("  command_id %d\n", completion->command_identifier);
            // printf("  status 0x%x\n", completion->status);

            if (status != 0) {
                // @TODO Free resources
                printf("nvme: Failed creating IO completion queue\n");
                return false;
            }


            context->completion_head++;
            write_completion_head(context, 0, context->completion_head);
            if (context->completion_head == context->asq.size + 1) {
                context->completion_head = 0;
            }

            break;
        }
    }

    {
        NVME_SubmissionQueueEntry* entry = (void*)(context->asq.address + context->submission_tail * sizeof(NVME_SubmissionQueueEntry));
        memset(entry, 0, sizeof(*entry));

        entry->command = NVME_CMD_CREATE_IO_SUBMISSION_QUEUE;
        entry->data_pointer[0] = context->io_sq.address;
        entry->body[0] = (io_queue_id_first) | (context->io_sq.size << 16);
        entry->body[1] = QUEUE_FLAG_PHYSICALLY_CONTIGUOUS | (io_queue_id_first << 16);

        context->submission_tail++;
        write_submission_tail(context, 0, context->submission_tail);
        // @NOCHECKIN Is this correct?
        if (context->submission_tail == context->asq.size + 1) {
            context->submission_tail = 0;
        }

        NVME_CompletionQueueEntry* completion = (void*)(context->acq.address + context->completion_head * sizeof(NVME_CompletionQueueEntry));

        // printf("Waiting for nvme cqe phase bit...\n");
        while (1) {
            int phaseBit = NVME_HAS_PHASE_BIT(completion->status);
            if (!phaseBit) {
                continue;
            }

            int status = completion->status >> 1;

            // printf("Completed.\n");
            // printf("  command 0x%x\n", completion->command);
            // printf("  submission_q_head %d\n", completion->submission_queue_head_pointer);
            // printf("  submission_q_id %d\n", completion->submission_queue_identifier);
            // printf("  command_id %d\n", completion->command_identifier);
            // printf("  status 0x%x\n", completion->status);

            if (status != 0) {
                // @TODO Free resources
                printf("nvme: Failed creating IO submission queue\n");
                return false;
            }


            context->completion_head++;
            write_completion_head(context, 0, context->completion_head);
            if (context->completion_head == context->asq.size + 1) {
                context->completion_head = 0;
            }

            break;
        }
    }

	return true;
}
