#include "elos/kernel/driver/acpi.h"

#include "elos/common/string.h"
#include "elos/kernel_console.h"

// #include "elos/kernel/pmem/paging.h"
#include "elos/physical_memory.h"

#include "elos/common/intrinsics.h"

#define printf(...) KCON_printf(__VA_ARGS__)




static int reset_addressSpace;
static u64 reset_address;
static u8  reset_value;


u64 acpi_lapic_address;
u64 acpi_ioapic_address;


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
    PMEM_map_memory(rsdt, rsdt, sizeof(ACPI_SDTHeader), PMEM_FLAG_NONE);

    if (memcmp(rsdt->Signature, "XSDT", 4)) {
        printf("Only XSDT is supported, RSDT is ignored\n");
        return;
    }
    
    // Now we can read the length of the header and map the pointers
    // to SDT headers following the RSDT.
    PMEM_map_memory(rsdt, rsdt, rsdt->Length, PMEM_FLAG_NONE);
    
    int array_of_sdt_len = (rsdt->Length - sizeof(*rsdt)) / sizeof(u64);
    u64* array_of_sdt = (u64*)((char*)rsdt + sizeof(*rsdt));


    ACPI_SDTHeader* fadt = NULL;
    ACPI_SDTHeader* madt = NULL;

    for (int i=0;i<array_of_sdt_len;i++) {
        ACPI_SDTHeader* header = (ACPI_SDTHeader*)array_of_sdt[i];
        
        // First map the header itself
        PMEM_map_memory(header, header, sizeof(ACPI_SDTHeader), PMEM_FLAG_NONE);
        // Then read length of the SDT and map it's content.
        PMEM_map_memory(header, header, header->Length, PMEM_FLAG_NONE);

        char name[5];
        memcpy(name, header->Signature, 4);
        name[4] = 0;
        if (!memcmp(header->Signature, "FACP", 4)) {
            fadt = header;
        } else if (!memcmp(header->Signature, "APIC", 4)) {
            madt = header;
        }
    }

    if (madt) {
        MADT_header* madt_extra = (MADT_header*)((char*)madt + sizeof(ACPI_SDTHeader));
        printf("MADT LAPIC address: %x\n", madt_extra->lapic_address);
        printf("MADT flags: %x\n", madt_extra->flags);

        
        acpi_lapic_address = madt_extra->lapic_address;

        u8* entries = (u8*)madt_extra + sizeof(MADT_header);
        int entries_size = madt->Length - sizeof(ACPI_SDTHeader) - sizeof(MADT_header);
        int head = 0;
        while (head < entries_size) {
            int start_head = head;
            MADT_entry_header* entry_base = (MADT_entry_header*)&entries[head];
            head+=2;

            switch (entry_base->entryType) {
                case MADT_ENTRY_LAPIC: {
                    MADT_lapic* entry = (MADT_lapic*)entry_base;
                    printf("LAPIC (type=%d len=%d)\n", entry->entryType, entry->entryLength);
                    printf(" acpiProcessorID: %d\n", entry->acpiProcessorID);
                    printf(" apicID: %d\n", entry->apicID);
                    printf(" flags: %x\n", entry->flags);
                } break;
                case MADT_ENTRY_IOAPIC: {
                    MADT_ioapic* entry = (MADT_ioapic*)entry_base;
                    printf("IOAPIC (type=%d len=%d)\n", entry->entryType, entry->entryLength);
                    printf(" ioapicID: %d\n", entry->ioapicID);
                    printf(" ioapicAddress: %x\n", entry->ioapicAddress);
                    printf(" globalSystemInterruptBase: %x\n", entry->globalSystemInterruptBase);
                    
                    acpi_ioapic_address = entry->ioapicAddress;
                } break;
                case MADT_ENTRY_IOAPIC_INTERRUPT_SRC_OVERRIDE: {
                    MADT_ioapic_interrupt_source_override* entry = (MADT_ioapic_interrupt_source_override*)entry_base;
                    printf("IOAPIC int.src.ovr. (type=%d len=%d)\n", entry->entryType, entry->entryLength);
                    printf(" busSource: %d\n", entry->busSource);
                    printf(" irqSource: %d\n", entry->irqSource);
                    printf(" globalSystemInterrupt: %d\n", entry->globalSystemInterrupt);
                    printf(" flags: %x\n", entry->flags);
                } break;
                case MADT_ENTRY_IOAPIC_NON_MASKABLE_INTERRUPT_SRC: {
                    MADT_ioapic_non_maskable_interrupt_source* entry = (MADT_ioapic_non_maskable_interrupt_source*)entry_base;
                    printf("IOAPIC nonmask.int.src. (type=%d len=%d)\n", entry->entryType, entry->entryLength);
                    printf(" nmiSource: %d\n", entry->nmiSource);
                    printf(" flags: %x\n", entry->flags);
                    printf(" globalSystemInterrupt: %d\n", entry->globalSysteminterrupt);
                } break;
                case MADT_ENTRY_LAPIC_NON_MASKABLE_INTERRUPT: {
                    MADT_lapic_non_maskable_interrupt* entry = (MADT_lapic_non_maskable_interrupt*)entry_base;
                    printf("LAPIC nonmask.int. (type=%d len=%d)\n", entry->entryType, entry->entryLength);
                    printf(" acpiProcessorID: %d\n", entry->acpiProcessorID);
                    printf(" flags: %x\n", entry->flags);
                    printf(" LINT: %d\n", entry->lint);
                } break;
                case MADT_ENTRY_LAPIC_ADDRESS_OVERRIDE: {
                    MADT_lapic_address_override* entry = (MADT_lapic_address_override*)entry_base;
                    printf("LAPIC addr.ovr. (type=%d len=%d)\n", entry->entryType, entry->entryLength);
                    printf(" address64: %d\n", entry->address64);
                    
                    acpi_lapic_address = entry->address64;
                } break;
                case MADT_ENTRY_LOCAL_X2APIC: {
                    MADT_local_x2apic* entry = (MADT_local_x2apic*)entry_base;
                    printf("Lx2APIC (type=%d len=%d)\n", entry->entryType, entry->entryLength);
                    printf(" x2apicID: %d\n", entry->x2apicID);
                    printf(" flags: %x\n", entry->flags);
                    printf(" acpiID: %d\n", entry->acpiID);
                } break;
            }

            head = start_head + entry_base->entryLength;
        }
    }

    if (fadt) {
        FADT* fadt_extra = (FADT*)((char*)fadt + sizeof(ACPI_SDTHeader));

        if ((char*)&fadt_extra->ResetValue - (char*)fadt < fadt->Length) {
            // Some FADT are smaller and may not have reset capability.

            printf("fadt.ResetReg.AccessSize: %d\n", fadt_extra->ResetReg.AccessSize);
            printf("fadt.ResetReg.Address: %x\n", fadt_extra->ResetReg.Address);
            printf("fadt.ResetReg.AddressSpace: %d\n", fadt_extra->ResetReg.AddressSpace);
            printf("fadt.ResetValue: %d\n", fadt_extra->ResetValue);
            
            #define ADDRESS_SPACE_SYSTEM_MEMORY 0
            #define ADDRESS_SPACE_SYSTEM_IO 0
            if (fadt_extra->ResetReg.AddressSpace == ADDRESS_SPACE_SYSTEM_MEMORY) { 
                bool mapped = PMEM_map_memory((void*)fadt_extra->ResetReg.Address, (void*)fadt_extra->ResetReg.Address, 1, PMEM_FLAG_NOT_CACHED);
                if (mapped) {
                    reset_addressSpace = ADDRESS_SPACE_SYSTEM_MEMORY;
                    reset_address = fadt_extra->ResetReg.Address;
                    reset_value = fadt_extra->ResetValue;
                }
            } else if (fadt_extra->ResetReg.AddressSpace == ADDRESS_SPACE_SYSTEM_IO) { 
                reset_addressSpace = ADDRESS_SPACE_SYSTEM_IO;
                reset_address = fadt_extra->ResetReg.Address;
                reset_value = fadt_extra->ResetValue;
            }
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
