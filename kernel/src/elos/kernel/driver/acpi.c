#include "elos/kernel/driver/acpi.h"

#include "elos/common/string.h"
#include "elos/kernel_console.h"

#include "elos/kernel/pmem/paging.h"

#define printf(...) KCON_printf(__VA_ARGS__)

void acpi_init(BootAPI* boot_api) {
    XSDP* xsdp = boot_api->rsdp;

    // Map it just in case.
    map_page(xsdp, xsdp);

    char sig[10] = {};
    char oem[10] = {};
    memcpy(sig, xsdp->Signature, sizeof(xsdp->Signature));
    memcpy(oem, xsdp->OEMID, sizeof(xsdp->OEMID));

    if (xsdp->Revision == 2) {
        printf("XSDP sig=%s oemid=%s rev=%d xsdt=%x\n", sig, oem, xsdp->Revision, xsdp->XsdtAddress);
    } else {
        printf("RSDP sig=%s oemid=%s rev=%d rsdt=%x\n", sig, oem, xsdp->Revision, xsdp->RsdtAddress);
        printf("(rsdp not supported, only ACPI 2.0)\n");
        return;
    }

    ACPI_SDTHeader* sdt_header = (ACPI_SDTHeader*)xsdp->XsdtAddress;

    ACPI_SDTHeader* fadt = NULL;
    ACPI_SDTHeader* madt = NULL;

    int count = (sdt_header->Length - sizeof(ACPI_SDTHeader)) / 8;
    for (int i=0;i<count;i++) {
        ACPI_SDTHeader* header = &((ACPI_SDTHeader*)((char*)sdt_header + sizeof(ACPI_SDTHeader)))[i];

        if (!memcmp(header->Signature, "FACP", 4)) {
            fadt = header;
        } else if (!memcmp(header->Signature, "APIC", 4)) {
            madt = header;
        }
    }


    
}
