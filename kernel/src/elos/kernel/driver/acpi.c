#include "elos/kernel/driver/acpi.h"

#include "elos/common/string.h"
#include "elos/kernel_console.h"

// #include "elos/kernel/pmem/paging.h"
#include "elos/physical_memory.h"

#define printf(...) KCON_printf(__VA_ARGS__)

void acpi_init(BootAPI* boot_api) {
    return;

    // @TODO Map rsdp to virtual

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

    PMEM_map_memory(rsdt, rsdt, sizeof(ACPI_SDTHeader));

    ACPI_SDTHeader* sdt_header = (ACPI_SDTHeader*)rsdt;

    PMEM_map_memory(rsdt, rsdt, (1+sdt_header->Length) * sizeof(ACPI_SDTHeader));

    ACPI_SDTHeader* fadt = NULL;
    ACPI_SDTHeader* madt = NULL;

    int count = (sdt_header->Length - sizeof(ACPI_SDTHeader)) / 8;
    for (int i=0;i<count;i++) {
        ACPI_SDTHeader* header = &((ACPI_SDTHeader*)((char*)sdt_header + sizeof(ACPI_SDTHeader)))[i];

        char name[5];
        memcpy(name, header->Signature, 4);
        name[4] = 0;
        if (!memcmp(header->Signature, "FACP", 4)) {
            fadt = header;
        } else if (!memcmp(header->Signature, "APIC", 4)) {
            madt = header;
        }
    }


    
}
