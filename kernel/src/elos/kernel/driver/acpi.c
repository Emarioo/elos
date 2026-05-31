#include "elos/kernel/driver/acpi.h"

#include "elos/common/string.h"
#include "elos/kernel_console.h"

// #include "elos/kernel/pmem/paging.h"
#include "elos/physical_memory.h"

#include "elos/common/intrinsics.h"

#include "elos/cpu.h"

#define printf(...) KCON_printf(__VA_ARGS__)
// #define debug(...) KCON_printf(__VA_ARGS__)
#define debug(...)




static int reset_addressSpace;
static u64 reset_address;
static u8  reset_value;


u32 acpi_lapic_ids[64];
u32 acpi_lapic_ids_len;

u64 acpi_lapic_address;
IOAPIC_Info acpi_ioapic_array[4];
int acpi_ioapic_array_len;
u64 acpi_hpet_address;



void acpi_init(BootAPI* boot_api) {

    ACPI_SDTHeader* rsdt = boot_api->rsdt;


    // char sig[10] = {};
    // char oem[10] = {};
    // memcpy(sig, xsdp->Signature, sizeof(xsdp->Signature));
    // memcpy(oem, xsdp->OEMID, sizeof(xsdp->OEMID));

    // if (xsdp->Revision == 2) {
    //     printf("XSDP sig=%s oemid=%s rev=%d xsdt=%x\n", sig, oem, xsdp->Revision, xsdp->XsdtAddress);
    // } else {
    //     printf("RSDP sig=%s oemid=%s rev=%d rsdt=%x\n", sig, oem, xsdp->Revision, xsdp->RsdtAddress);
    //     printf("(rsdp not supported, only ACPI 2.0)\n");
    //     return;
    // }

    // rsdt is the physical address. We have to map it to access it's fields.
    PMEM_map_memory(g_kernelPageTable, rsdt, rsdt, sizeof(ACPI_SDTHeader), PMEM_FLAG_NONE);

    if (memcmp(rsdt->Signature, "XSDT", 4)) {
        printf("Only XSDT is supported, RSDT is ignored\n");
        return;
    }
    
    // Now we can read the length of the header and map the pointers
    // to SDT headers following the RSDT.
    PMEM_map_memory(g_kernelPageTable, rsdt, rsdt, rsdt->Length, PMEM_FLAG_NONE);
    
    int array_of_sdt_len = (rsdt->Length - sizeof(*rsdt)) / sizeof(u64);
    u64* array_of_sdt = (u64*)((char*)rsdt + sizeof(*rsdt));


    ACPI_SDTHeader* fadt_header = NULL;
    ACPI_SDTHeader* madt_header = NULL;
    ACPI_SDTHeader* hpet_header = NULL;

    for (int i=0;i<array_of_sdt_len;i++) {
        ACPI_SDTHeader* header = (ACPI_SDTHeader*)array_of_sdt[i];
        
        // First map the header itself
        PMEM_map_memory(g_kernelPageTable, header, header, sizeof(ACPI_SDTHeader), PMEM_FLAG_NONE);
        // Then read length of the SDT and map it's content.
        PMEM_map_memory(g_kernelPageTable, header, header, header->Length, PMEM_FLAG_NONE);

        char name[5];
        memcpy(name, header->Signature, 4);
        name[4] = 0;
        if (!memcmp(header->Signature, "FACP", 4)) {
            fadt_header = header;
        } else if (!memcmp(header->Signature, "APIC", 4)) {
            madt_header = header;
        } else if (!memcmp(header->Signature, "HPET", 4)) {
            hpet_header = header;
        }
    }

    if (madt_header) {
        MADT_header* madt = (MADT_header*)((char*)madt_header + sizeof(ACPI_SDTHeader));
        debug("MADT LAPIC address: %x\n", madt->lapic_address);
        debug("MADT flags: %x\n", madt->flags);

        acpi_lapic_address = madt->lapic_address;
        PMEM_map_memory(g_kernelPageTable, (void*)acpi_lapic_address, (void*)acpi_lapic_address, PAGE_SIZE, PMEM_FLAG_NOT_CACHED);

        u8* entries = (u8*)madt + sizeof(MADT_header);
        int entries_size = madt_header->Length - sizeof(ACPI_SDTHeader) - sizeof(MADT_header);
        int head = 0;
        while (head < entries_size) {
            int start_head = head;
            MADT_entry_header* entry_base = (MADT_entry_header*)&entries[head];
            head+=2;

            switch (entry_base->entryType) {
                case MADT_ENTRY_LAPIC: {
                    MADT_lapic* entry = (MADT_lapic*)entry_base;
                    debug("LAPIC (type=%d len=%d)\n", entry->entryType, entry->entryLength);
                    debug(" acpiProcessorID: %d\n", entry->acpiProcessorID);
                    debug(" apicID: %d\n", entry->apicID);
                    debug(" flags: %x\n", entry->flags);

                    acpi_lapic_ids[acpi_lapic_ids_len] = entry->apicID;
                    acpi_lapic_ids_len++;
                } break;
                case MADT_ENTRY_IOAPIC: {
                    MADT_ioapic* entry = (MADT_ioapic*)entry_base;
                    debug("IOAPIC (type=%d len=%d)\n", entry->entryType, entry->entryLength);
                    debug(" ioapicID: %d\n", entry->ioapicID);
                    debug(" ioapicAddress: %x\n", entry->ioapicAddress);
                    debug(" globalSystemInterruptBase: %x\n", entry->globalSystemInterruptBase);
                    
                    acpi_ioapic_array[acpi_ioapic_array_len].address = entry->ioapicAddress;
                    acpi_ioapic_array[acpi_ioapic_array_len].interruptBaseNumber = entry->globalSystemInterruptBase;
                    acpi_ioapic_array_len++;

                    PMEM_map_memory(g_kernelPageTable, (void*)(u64)entry->ioapicAddress, (void*)(u64)entry->ioapicAddress, PAGE_SIZE, PMEM_FLAG_NOT_CACHED);
                } break;
                case MADT_ENTRY_IOAPIC_INTERRUPT_SRC_OVERRIDE: {
                    MADT_ioapic_interrupt_source_override* entry = (MADT_ioapic_interrupt_source_override*)entry_base;
                    debug("IOAPIC int.src.ovr. (type=%d len=%d)\n", entry->entryType, entry->entryLength);
                    debug(" busSource: %d\n", entry->busSource);
                    debug(" irqSource: %d\n", entry->irqSource);
                    debug(" globalSystemInterrupt: %d\n", entry->globalSystemInterrupt);
                    debug(" flags: %x\n", entry->flags);
                } break;
                case MADT_ENTRY_IOAPIC_NON_MASKABLE_INTERRUPT_SRC: {
                    MADT_ioapic_non_maskable_interrupt_source* entry = (MADT_ioapic_non_maskable_interrupt_source*)entry_base;
                    debug("IOAPIC nonmask.int.src. (type=%d len=%d)\n", entry->entryType, entry->entryLength);
                    debug(" nmiSource: %d\n", entry->nmiSource);
                    debug(" flags: %x\n", entry->flags);
                    debug(" globalSystemInterrupt: %d\n", entry->globalSysteminterrupt);
                } break;
                case MADT_ENTRY_LAPIC_NON_MASKABLE_INTERRUPT: {
                    MADT_lapic_non_maskable_interrupt* entry = (MADT_lapic_non_maskable_interrupt*)entry_base;
                    debug("LAPIC nonmask.int. (type=%d len=%d)\n", entry->entryType, entry->entryLength);
                    debug(" acpiProcessorID: %d\n", entry->acpiProcessorID);
                    debug(" flags: %x\n", entry->flags);
                    debug(" LINT: %d\n", entry->lint);
                } break;
                case MADT_ENTRY_LAPIC_ADDRESS_OVERRIDE: {
                    MADT_lapic_address_override* entry = (MADT_lapic_address_override*)entry_base;
                    debug("LAPIC addr.ovr. (type=%d len=%d)\n", entry->entryType, entry->entryLength);
                    debug(" address64: %d\n", entry->address64);
                    
                    if (entry->address64 != acpi_lapic_address) {
                        acpi_lapic_address = entry->address64;
                        PMEM_map_memory(g_kernelPageTable, (void*)acpi_lapic_address, (void*)acpi_lapic_address, PAGE_SIZE, PMEM_FLAG_NOT_CACHED);
                    }
                } break;
                case MADT_ENTRY_LOCAL_X2APIC: {
                    MADT_local_x2apic* entry = (MADT_local_x2apic*)entry_base;
                    debug("Lx2APIC (type=%d len=%d)\n", entry->entryType, entry->entryLength);
                    debug(" x2apicID: %d\n", entry->x2apicID);
                    debug(" flags: %x\n", entry->flags);
                    debug(" acpiID: %d\n", entry->acpiID);
                } break;
            }

            head = start_head + entry_base->entryLength;
        }
    }

    if (fadt_header) {
        FADT* fadt = (FADT*)((char*)fadt_header + sizeof(ACPI_SDTHeader));

        if ((char*)&fadt->ResetValue - (char*)fadt_header < fadt_header->Length) {
            // Some FADT are smaller and may not have reset capability.

            debug("fadt.ResetReg.AccessSize: %d\n", fadt->ResetReg.AccessSize);
            debug("fadt.ResetReg.Address: %x\n", fadt->ResetReg.Address);
            debug("fadt.ResetReg.AddressSpace: %d\n", fadt->ResetReg.AddressSpace);
            debug("fadt.ResetValue: %d\n", fadt->ResetValue);
            
            #define ADDRESS_SPACE_SYSTEM_MEMORY 0
            #define ADDRESS_SPACE_SYSTEM_IO 0
            if (fadt->ResetReg.AddressSpace == ADDRESS_SPACE_SYSTEM_MEMORY) { 
                bool mapped = PMEM_map_memory(g_kernelPageTable, (void*)fadt->ResetReg.Address, (void*)fadt->ResetReg.Address, 1, PMEM_FLAG_NOT_CACHED);
                if (mapped) {
                    reset_addressSpace = ADDRESS_SPACE_SYSTEM_MEMORY;
                    reset_address = fadt->ResetReg.Address;
                    reset_value = fadt->ResetValue;
                }
            } else if (fadt->ResetReg.AddressSpace == ADDRESS_SPACE_SYSTEM_IO) { 
                reset_addressSpace = ADDRESS_SPACE_SYSTEM_IO;
                reset_address = fadt->ResetReg.Address;
                reset_value = fadt->ResetValue;
            }
        }
    }
    
    if (hpet_header) {
        HPET* hpet = (HPET*)((char*)hpet_header + sizeof(ACPI_SDTHeader));

        if (hpet->address.AddressSpace != ADDRESS_SPACE_SYSTEM_MEMORY) {
            printf("HPET does not use MMIO\n");
        } else {
            acpi_hpet_address = hpet->address.Address;

            PMEM_map_memory(g_kernelPageTable, (void*)acpi_hpet_address, (void*)acpi_hpet_address, PAGE_SIZE, PMEM_FLAG_NOT_CACHED);

        }
    }
}


void acpi_system_reset() {

    if (reset_address) {
        if (reset_addressSpace == ADDRESS_SPACE_SYSTEM_MEMORY) {
            *(u8*)reset_address = reset_value;
        } else if (reset_addressSpace == ADDRESS_SPACE_SYSTEM_IO) {
            outb((u16)reset_address, reset_value);
        } else {
            printf("Invalid reset_addressSpace=%d\n", reset_addressSpace);
        }
    } else {
        // Fallback to PS/2 power line reset
        while (inb(0x64) & 0x2) ; // Wait for empty input buffer

        outb(0x64, 0xFE); // Send reset command to keyboard controller
    }

    printf("SYSTEM RESET any time now...\n");
    while (1) __asm__ ( "hlt\n" );
}
