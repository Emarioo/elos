/*
    @TODO Interrupts

    @TODO Multiple IO queues
*/

#include "elos/kernel/disk/nvme.h"

#include "elos/common/string.h"
#include "elos/common/intrinsics.h"

#include "elos/physical_memory.h"
#include "elos/kernel_console.h"

#include "elos/cpu.h"

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
bool send_identify(Disk_ScanInfo* scanInfo, NVME_Context* context);

_align(4096) static char identify_data[4096];


#define MAX_NVME_CONTEXTS 20
#define MAX_NVME_NAMESPACES 20

NVME_Context nvmeContexts[MAX_NVME_CONTEXTS];
volatile u32 nvmeContexts_len;

NVME_Context_Namespace nvmeNamespaces[MAX_NVME_NAMESPACES];
volatile u32 nvmeNamespaces_len;



bool nvme_pci_scan(Disk_ScanInfo* scanInfo, PCI_ConfigSpace* config) {
    // @TODO check config->progIF == 0x2 ? i left this comment, is it important?


    void* phys_memory_bar = (void*)((u64)((u64)config->header0.bar0 & ~(u64)0xF) | ((u64)config->header0.bar1 << 32));
    // @TODO We want to reserve this virtual address from somewhere.
    //    Like reserve virtual_mmio.
    void* memory_bar = PMEM_reserve_virtual_mmio(phys_memory_bar, 16 * PAGE_SIZE);
    if (!memory_bar) {
        printf("nvme: Could request uncached memory for physical %p\n", phys_memory_bar, memory_bar);
        return false;
    }

    volatile NVME_Registers* regs = memory_bar;

    u32 ctx_index = __atomic_fetch_add(&nvmeContexts_len, 1, __ATOMIC_SEQ_CST);
    if (ctx_index >= MAX_NVME_CONTEXTS)  {
        return false;
    }
    NVME_Context* context = &nvmeContexts[ctx_index];
    memset(context, 0, sizeof(*context));

    context->config = *config;
    context->regs   = regs;

    // device->nvme.nvme_context = context;

    u32 majorVersion = (regs->VS >> 16) & 0xFFFF;
    u32 minorVersion = (regs->VS >> 8) & 0xFF;
    u32 DRSTRD = 4 << ((regs->CAP >> 32) & 0xF);
    u32 MPSMIN = 0x1000 << ((regs->CAP >> 48) & 0xF);
    u32 MPSMAX = 0x1000 << ((regs->CAP >> 52) & 0xF);
    u32 MQES = ((regs->CAP) & 0xFFFF) + 1;
    u32 MPS = 0x1000 << ((regs->CC >> 7) & 0xF);

    // printf("BAR0 %p\n", memory_bar);
    // printf("  CAP 0x%zx\n", regs->CAP);
    // printf("  VS 0x%x\n", regs->VS);
    // printf("  INTMS 0x%x\n", regs->INTMS);
    // printf("  INTMC 0x%x\n", regs->INTMC);
    // printf("  CC 0x%x\n", regs->CC);
    // printf("  CSTS 0x%x\n", regs->CSTS);
    // printf("  AQA 0x%x\n", regs->AQA);
    // printf("  ASQ 0x%zx\n", regs->ASQ);
    // printf("  ACQ 0x%zx\n", regs->ACQ);
    
    // printf("\n");

    // printf("  DRSTRD %u\n", DRSTRD);
    // printf("  MPS %u\n", MPS);
    // printf("  MQES %u\n", MQES);
    // printf("  NVMe v%u.%u\n", majorVersion, minorVersion);

    // printf("\n");

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

    yes = send_identify(scanInfo, context);
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

    context->phys_tempSector     = PMEM_alloc_phys(context->sectorSize);
    context->tempMemoryPage = PMEM_alloc(context->MPS);

    return true;
}



bool nvme_aligned_read(DiskDevice* device, u64 byteOffset, u64 byteSize, void* phys_buffer) {

    u32 queue_identifier = 1;

    NVME_Context_Namespace* namespaceContext = (NVME_Context_Namespace*)device->userData;
    NVME_Context* context = namespaceContext->context;

    KERNEL_PANIC(byteOffset  % context->sectorSize == 0, "not aligned");
    KERNEL_PANIC(byteSize    % context->sectorSize == 0, "not aligned");
    KERNEL_PANIC(((size_t)phys_buffer & 3) == 0, "not aligned");

    NVME_SubmissionQueueEntry* entry = (void*)(context->io_sq.address + context->io_submission_tail * sizeof(NVME_SubmissionQueueEntry));
    memset(entry, 0, sizeof(*entry));

    // @TODO Handle alignment on the buffer?
    //   We read a block at a time.
    //   If we specified less, then we need temporary buffer.

    u64 lba = byteOffset / context->sectorSize;
    u64 num_blocks = (byteSize + context->sectorSize-1) / context->sectorSize;

    entry->command = NVME_CMD_READ;
    entry->nsid = namespaceContext->nsid;

    u32 max_prp_entries = context->MPS / 8 - 1;
    u32 max_size = max_prp_entries * context->MPS;
    if (byteSize > max_size) {
        printf("nvme_aligned_read: Bytes to read (%zu) is larger than maximum (%zu)\n", byteSize, max_size);
        return false;
    }

    if (byteSize <= context->MPS) {
        entry->data_pointer[0] = (size_t)phys_buffer;
        size_t unaligned_offset = (size_t)phys_buffer % context->MPS;
        if ((size_t)phys_buffer % context->MPS != 0 && unaligned_offset + byteSize > context->MPS) {
            entry->data_pointer[1] = (size_t)phys_buffer + context->MPS - unaligned_offset;
        }
    } else {
        u64* prpList = context->tempMemoryPage;
        size_t unaligned_offset = (size_t)phys_buffer % context->MPS;

        int index = 0;
        int bufOffset = context->MPS - unaligned_offset;
        int prp_entries = (byteSize + context->MPS -1) / context->MPS - 1;
        while (index < prp_entries) {
            prpList[index] = (size_t)phys_buffer + bufOffset;

            index++;
            bufOffset += context->MPS;
        }

        entry->data_pointer[0] = (u64)phys_buffer;
        entry->data_pointer[1] = (u64)prpList;
    }

    entry->body[0] = lba;
    entry->body[1] = lba >> 32;
    entry->body[3] = num_blocks - 1; // zero-based so we reduce one

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

bool nvme_aligned_write(DiskDevice* device, u64 byteOffset, u64 byteSize, const void* phys_buffer) {

    u32 queue_identifier = 1;

    NVME_Context_Namespace* namespaceContext = (NVME_Context_Namespace*)device->userData;
    NVME_Context* context = namespaceContext->context;

    KERNEL_PANIC(byteOffset  % context->sectorSize == 0, "not aligned");
    KERNEL_PANIC(byteSize    % context->sectorSize == 0, "not aligned");
    KERNEL_PANIC(((size_t)phys_buffer & 3) == 0, "not aligned");

    NVME_SubmissionQueueEntry* entry = (void*)(context->io_sq.address + context->io_submission_tail * sizeof(NVME_SubmissionQueueEntry));
    memset(entry, 0, sizeof(*entry));

    // @TODO Handle alignment on the buffer?
    //   We read a block at a time.
    //   If we specified less, then we need temporary buffer.
    // @TODO Handle big/small buffers.
    u64 lba = byteOffset / context->sectorSize;
    u64 num_blocks = byteSize / context->sectorSize;

    entry->command = NVME_CMD_WRITE;
    entry->data_pointer[0] = (u64)phys_buffer;
    entry->body[0] = lba;
    entry->body[1] = lba >> 32;
    entry->body[3] = num_blocks;
    entry->nsid = namespaceContext->nsid;

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


ELOS_Error nvme_read(DiskDevice* device, u64 byteOffset, u64 byteSize, void* phys_buffer) {

    if (((size_t)phys_buffer & 3) != 0) {
        printf("nvme: Buffer is not 4-byte aligned on read.\n");
        return ELOS_ERR_UNKNOWN;
    }

    NVME_Context_Namespace* namespaceContext = (NVME_Context_Namespace*)device->userData;
    NVME_Context* context = namespaceContext->context;

    LOCK(&context->buffer_lock);

    u64 sectorSize = context->sectorSize;

    u64 cur_offset       = byteOffset;
    u64 cur_size         = byteSize;
    u64 cur_bufferOffset = 0;

    bool anyFailed = false;

    while (cur_size > 0) {

        u64   offset       = cur_offset;
        u64   size         = byteSize;
        void* curBuffer    = (char*)phys_buffer + cur_bufferOffset;
        u64   realSize     = size;
        bool  useTempBlock = false;

        if (offset % sectorSize != 0) {
            useTempBlock = true;
            realSize     = sectorSize - offset % sectorSize;
            offset       -= offset % sectorSize;
            size         = sectorSize;
            curBuffer    = context->phys_tempSector;
        }

        if (size < sectorSize) {
            useTempBlock = true;
            size         = sectorSize;
            curBuffer    = context->phys_tempSector;
        }
        
        if (size % sectorSize != 0) {
            size     = size - (size % sectorSize);
            realSize = size;
        }

        // printf("log %zx %zx %d\n", offset, size, context->sectorSize);
        anyFailed = !nvme_aligned_read(device, offset, size, curBuffer);
        if (!anyFailed) {
            break;
        }

        if (useTempBlock) {
            void* vaddr = PMEM_phys_to_kernel((char*)phys_buffer + cur_bufferOffset);
            void* vaddr2 = PMEM_phys_to_kernel(context->phys_tempSector);
            memcpy(vaddr, context->phys_tempSector, realSize);
        }

        cur_offset       += realSize;
        cur_size         -= realSize;
        cur_bufferOffset += realSize;
    }

    UNLOCK(&context->buffer_lock);

    return !anyFailed ? ELOS_OK : ELOS_ERR_UNKNOWN;
}


ELOS_Error nvme_write(DiskDevice* device, u64 byteOffset, u64 byteSize, const void* phys_buffer) {
    if (((size_t)phys_buffer & 3) != 0) {
        printf("nvme: Buffer is not 4-byte aligned on write.\n");
        return ELOS_ERR_UNKNOWN;
    }

    NVME_Context_Namespace* namespaceContext = (NVME_Context_Namespace*)device->userData;
    NVME_Context* context = namespaceContext->context;

    LOCK(&context->buffer_lock);

    u64 sectorSize = context->sectorSize;

    u64 cur_offset       = byteOffset;
    u64 cur_size         = byteSize;
    u64 cur_bufferOffset = 0;

    bool anyFailed = false;

    while (cur_size > 0) {

        u64   offset       = cur_offset;
        u64   size         = byteSize;
        void* curBuffer    = (char*)phys_buffer + cur_bufferOffset;
        u64   realSize     = size;
        bool  useTempBlock = false;

        if (offset % sectorSize != 0) {
            useTempBlock = true;
            realSize     = sectorSize - offset % sectorSize;
            offset       -= offset % sectorSize;
            size         = sectorSize;
            curBuffer    = context->phys_tempSector;
        }

        if (size < sectorSize) {
            useTempBlock = true;
            size         = sectorSize;
            curBuffer    = context->phys_tempSector;
        }
        
        if (size % sectorSize != 0) {
            size     = size - (size % sectorSize);
            realSize = size;
        }

        if (useTempBlock) {
            anyFailed = !nvme_aligned_read(device, offset, size, curBuffer);
            if (!anyFailed) {
                break;
            }
            void* vaddr = PMEM_phys_to_kernel((char*)phys_buffer + cur_bufferOffset);
            void* vaddr2 = PMEM_phys_to_kernel(context->phys_tempSector);
            memcpy(vaddr2, vaddr2, realSize);
        }

        anyFailed = !nvme_aligned_write(device, offset, size, curBuffer);
        if (!anyFailed) {
            break;
        }

        cur_offset       += realSize;
        cur_size         -= realSize;
        cur_bufferOffset += realSize;
    }

    UNLOCK(&context->buffer_lock);

    return !anyFailed ? ELOS_OK : ELOS_ERR_UNKNOWN;
}


bool init_admin_queues(NVME_Context* context) {
	context->acq.address = (uint64_t)PMEM_alloc(PAGE_SIZE);
	if (context->acq.address == 0)
		return false;
    memset((void*)context->acq.address, 0, PAGE_SIZE);
	context->acq.size = 63;
    context->regs->ACQ = (size_t)PMEM_kernel_to_phys((void*)context->acq.address);

	context->asq.address = (uint64_t)PMEM_alloc(PAGE_SIZE);
	if (context->asq.address == 0)
		return false;
    memset((void*)context->asq.address, 0, PAGE_SIZE);
	context->asq.size = 63;
    context->regs->ASQ = (size_t)PMEM_kernel_to_phys((void*)context->asq.address);

    context->regs->AQA = (context->asq.size & 0xFFF) | ((context->acq.size & 0xFFF) << 16);

	return true;
}


bool send_identify(Disk_ScanInfo* scanInfo, NVME_Context* context) {

    NVME_SubmissionQueueEntry* entry = (void*)(context->asq.address + context->submission_tail * sizeof(NVME_SubmissionQueueEntry));
    memset(entry, 0, sizeof(*entry));

    entry->command = NVME_CMD_IDENTIFY;
    entry->data_pointer[0] = (u64)PMEM_kernel_to_phys(&identify_data);
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

    char controller_modelNumber[64];

    char* modelNumber = identify_data + 24;
    int modelLength = 0;
    for (int i=0;i < 64 - 24;i++) {
        char chr = modelNumber[i];
        if (!(chr == ' ' || chr == '\t' || chr == '\n' || chr == '\f' || chr < 32)) {
            modelLength = i + 1;
        }
    }
    
    snprintf(controller_modelNumber, sizeof(controller_modelNumber),
        "%.*s", modelLength, modelNumber);

    u32 MDTS = identify_data[77];

    
    {

        NVME_SubmissionQueueEntry* entry = (void*)(context->asq.address + context->submission_tail * sizeof(NVME_SubmissionQueueEntry));
        memset(entry, 0, sizeof(*entry));

        entry->command = NVME_CMD_IDENTIFY;
        entry->data_pointer[0] = (u64)PMEM_kernel_to_phys(&identify_data);
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
            // printf("Namespace %d\n", nsid);
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
        entry->data_pointer[0] = (u64)PMEM_kernel_to_phys(&identify_data);
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

        // @TODO This should be in namespace struct, not context struct.
        context->sectorSize = 1 << LBADS;
        // printf("Block Size %d\n", context->sectorSize);

        u32 ctx_index = __atomic_fetch_add(&nvmeNamespaces_len, 1, __ATOMIC_SEQ_CST);
        if (ctx_index >= MAX_NVME_NAMESPACES) {
            return false;
        }
        NVME_Context_Namespace* contextNamespace = &nvmeNamespaces[ctx_index];

        contextNamespace->context = context;
        contextNamespace->nsid = context->first_nsid;
        
        DiskDevice* device = disk_reserve_device();
        if (!device) {
            // no more disk devices to reserve.
            return true;
        }
        device->type = DISK_TYPE_NVME;
        device->userData = contextNamespace;

        snprintf(device->diskInfo.name, sizeof(device->diskInfo.name),
            "%.*s", modelLength, controller_modelNumber);

        if (scanInfo->count < scanInfo->maxCount) {
            scanInfo->devices[scanInfo->count] = device;
            scanInfo->count++;
        } else {
            // Free device slot
            device->type = DISK_TYPE_NONE;
            return true;
        }

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
        entry->data_pointer[0] = (size_t)PMEM_kernel_to_phys((void*)context->io_cq.address);
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
        entry->data_pointer[0] = (size_t)PMEM_kernel_to_phys((void*)context->io_sq.address);
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
