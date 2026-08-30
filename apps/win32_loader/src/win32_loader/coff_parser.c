#include "win32_loader/coff_parser.h"

#include <stdbool.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
    Resolves RVA in section to file offset.
    Returns 0 if RVA not inside section.
*/
static size_t rva_to_file_offset(COFF_Section_Header* section, size_t rva) {
    if (rva >= section->VirtualAddress && rva < section->VirtualAddress + section->VirtualSize) {
        return section->PointerToRawData + rva - section->VirtualAddress;
    }
    return 0;
}

void dump_coff(const char* path) {
    FILE* file = NULL;
    uint8_t* fileData = NULL;
    int res;

    file = fopen(path, "rb");
    if (!file) {
        printf("Could not open %s\n", path);
        goto exit;
    }   


    res = fseek(file, 0, SEEK_END);
    if (res != 0) {
        printf("Could not seek %s\n", path);
        goto exit;
    }
    size_t fileSize = ftell(file);
    res = fseek(file, 0, SEEK_SET);
    if (res != 0) {
        printf("Could not seek %s\n", path);
        goto exit;
    }

    fileData = malloc(fileSize);

    size_t readBytes = fread(fileData, 1, fileSize, file);
    if (readBytes != fileSize) {
        printf("Could not read %s, %zu != %zu\n", path, fileSize, readBytes);
        goto exit;
    }

    fclose(file);
    file = NULL;



    

    COFF_File_Header* header = (void*)fileData;

    
    printf("Header %s\n", path);

    uint32_t signatureOffset = *(uint32_t*)(fileData + 0x3c);
    if (signatureOffset + 4 < fileSize) {
        const char* signature = (const char*)(fileData + signatureOffset);
        if (!strncmp(signature, "PE\0\0", 4)) {
            header = (void*)(signature + 4);
        }
    }
    
    size_t offset_to_sections = (size_t)header + COFF_File_Header_SIZE + header->SizeOfOptionalHeader - (size_t)fileData;

    printf(" machine              0x%x\n", header->Machine);
    printf(" numbrOfSections      0x%x\n", header->NumberOfSections);
    printf(" timeDateStamp        0x%x\n", header->TimeDateStamp);
    printf(" pointerToSymbolTable 0x%x\n", header->PointerToSymbolTable);
    printf(" numberOfSymbols      0x%x\n", header->NumberOfSymbols);
    printf(" sizeOfOptionalheader 0x%x\n", header->SizeOfOptionalHeader);
    printf(" characteristics      0x%x\n", header->Characteristics);

    bool is64bit = false;

    if (header->SizeOfOptionalHeader >= COFF_Optional_Header_64_SIZE) {
        COFF_Optional_Header* optionalHeader = (void*)((char*)header + COFF_File_Header_SIZE);

        printf("Optional Header\n");

        printf(" Magic 0x%x\n", optionalHeader->Magic);
        printf(" MajorLinkerVersion 0x%x\n", optionalHeader->MajorLinkerVersion);
        printf(" MinorLinkerVersion 0x%x\n", optionalHeader->MinorLinkerVersion);
        printf(" SizeOfCode 0x%x\n", optionalHeader->SizeOfCode);
        printf(" SizeOfInitializedData 0x%x\n", optionalHeader->SizeOfInitializedData);
        printf(" SizeOfUninitializedData 0x%x\n", optionalHeader->SizeOfUninitializedData);
        printf(" AddressOfEntryPoint 0x%x\n", optionalHeader->AddressOfEntryPoint);
        printf(" BaseOfCode 0x%x\n", optionalHeader->BaseOfCode);

        is64bit = optionalHeader->Magic == COFF_OPTIONAL_HEADER_MAGIC_PE32_PLUS;

        if (is64bit) {
            printf(" BaseOfData 0x%x\n", optionalHeader->BaseOfData);
            if (header->SizeOfOptionalHeader >= COFF_Optional_Header_64_SIZE + COFF_Optional_Windows_Header_64_SIZE) {
                COFF_Optional_Windows_Header_64* windowsHeader = (void*)((char*)header + COFF_File_Header_SIZE + COFF_Optional_Header_64_SIZE);

            }
        } else {
            if (header->SizeOfOptionalHeader >= COFF_Optional_Header_32_SIZE + COFF_Optional_Windows_Header_32_SIZE) {
                COFF_Optional_Windows_Header_32* windowsHeader = (void*)((char*)header + COFF_File_Header_SIZE + COFF_Optional_Header_32_SIZE);

                printf("\n");

                printf(" ImageBase 0x%x\n", windowsHeader->ImageBase);
                printf(" SectionAlignment 0x%x\n", windowsHeader->SectionAlignment);
                printf(" FileAlignment 0x%x\n", windowsHeader->FileAlignment);
                printf(" MajorOperatingSystemVersion 0x%x\n", windowsHeader->MajorOperatingSystemVersion);
                printf(" MinorOperatingSystemVersion 0x%x\n", windowsHeader->MinorOperatingSystemVersion);
                printf(" MajorImageVersion 0x%x\n", windowsHeader->MajorImageVersion);
                printf(" MinorImageVersion 0x%x\n", windowsHeader->MinorImageVersion);
                printf(" MajorSubsystemVersion 0x%x\n", windowsHeader->MajorSubsystemVersion);
                printf(" MinorSubsystemVersion 0x%x\n", windowsHeader->MinorSubsystemVersion);
                printf(" Win32VersionValue 0x%x\n", windowsHeader->Win32VersionValue);
                printf(" SizeOfImage 0x%x\n", windowsHeader->SizeOfImage);
                printf(" SizeOfHeaders 0x%x\n", windowsHeader->SizeOfHeaders);
                printf(" CheckSum 0x%x\n", windowsHeader->CheckSum);
                printf(" Subsystem 0x%x\n", windowsHeader->Subsystem);
                printf(" DllCharacteristics 0x%x\n", windowsHeader->DllCharacteristics);
                printf(" SizeOfStackReserve 0x%x\n", windowsHeader->SizeOfStackReserve);
                printf(" SizeOfStackCommit 0x%x\n", windowsHeader->SizeOfStackCommit);
                printf(" SizeOfHeapReserve 0x%x\n", windowsHeader->SizeOfHeapReserve);
                printf(" SizeOfHeapCommit 0x%x\n", windowsHeader->SizeOfHeapCommit);
                printf(" LoaderFlags 0x%x\n", windowsHeader->LoaderFlags);
                printf(" NumberOfRvaAndSizes 0x%x\n", windowsHeader->NumberOfRvaAndSizes);
                printf("\n");

                uint32_t baseOffset = COFF_File_Header_SIZE + COFF_Optional_Header_32_SIZE + COFF_Optional_Windows_Header_32_SIZE;
                COFF_Image_Data_Directory* dataDirs = (void*)((char*)header + COFF_File_Header_SIZE + COFF_Optional_Header_32_SIZE + COFF_Optional_Windows_Header_32_SIZE);

                const char* dirNames[] = {
                    "ExportTable",
                    "ImportTable",
                    "ResourceTable",
                    "ExceptionTable",
                    "CertificateTable",
                    "BaseRelocationTable",
                    "Debug",
                    "Architecture",
                    "GlobalPtr",
                    "TLSTable",
                    "LoadConfigTable",
                    "BoundImport",
                    "IAT",
                    "DelayImportDescriptor",
                    "CLRRuntimeHeader",
                };
                int max_entries = sizeof(dirNames)/sizeof(*dirNames);
                // int entries_count = (header->SizeOfOptionalHeader - baseOffset) / sizeof(COFF_Image_Data_Directory);
                int entries_count = windowsHeader->NumberOfRvaAndSizes;
                if (entries_count > max_entries)
                    entries_count = max_entries;


                for (int i=0;i<entries_count;i++) {
                    COFF_Image_Data_Directory* dataDir = &dataDirs[i];
                    printf(" %s virt=%u size=%u\n", dirNames[i], dataDir->VirtualAddress, dataDir->Size);
                }
                printf("\n");
            }
        }
    }


    char* stringData = (char*)fileData + header->PointerToSymbolTable + header->NumberOfSymbols * COFF_Symbol_Record_SIZE;
    uint32_t stringSize = *(uint32_t*)stringData;


    COFF_Section_Header* sections = (void*)(fileData + offset_to_sections);

    char buffer[9];

    COFF_Section_Header* idata = NULL;

    for (int si=0;si<header->NumberOfSections;si++) {
        COFF_Section_Header* section = &sections[si];

        const char* sectionName = NULL;
        if (section->Name[0] == '/') {
            uint32_t index = atoi(section->Name+1);
            if (index) {
                sectionName = stringData + index;
            }
        } else {
            memcpy(buffer, section->Name, 8);
            buffer[8] = '\0';
            sectionName = buffer;
        }

        if (!strcmp(sectionName, ".idata")) {
            idata = section;
        }

        printf("Section %s\n", sectionName);
        printf(" VirtualSize 0x%x\n", section->VirtualSize);
        printf(" VirtualAddress 0x%x\n", section->VirtualAddress);
        printf(" SizeOfRawData 0x%x\n", section->SizeOfRawData);
        printf(" PointerToRawData 0x%x\n", section->PointerToRawData);
        printf(" PointerToRelocations 0x%x\n", section->PointerToRelocations); 
        printf(" PointerToLineNumbers 0x%x\n", section->PointerToLineNumbers);
        printf(" NumberOfRelocations 0x%x\n", section->NumberOfRelocations);
        printf(" NumberOfLineNumbers 0x%x\n", section->NumberOfLineNumbers);
        printf(" Characteristics 0x%x\n", section->Characteristics);
        printf("\n");
    }

    if (idata) {
        printf("Import Table\n");

        COFF_Import_Directory_Table* tables = (void*)(fileData + idata->PointerToRawData);
        
        int ti = 0;
        while (1) {
            COFF_Import_Directory_Table* table = &tables[ti];
            ti++;

            if (table->ImportAddressTableRVA == 0) {
                // NULL entry means end of table.
                break;
            }

            const char* dllName = NULL;
            uint8_t* importLookupTable = NULL;
            uint8_t* importAddressTable = NULL;

            // The name is usually in .idata section.
            // But maybe it doesn't have to be so check other sections too?
            size_t tempOffset;
            tempOffset = rva_to_file_offset(idata, table->NameRVA);
            if (tempOffset) {
                dllName = (char*)fileData + tempOffset;
            }
            tempOffset = rva_to_file_offset(idata, table->ImportLookupTableRVA);
            if (tempOffset) {
                importLookupTable = fileData + tempOffset;
            }
            tempOffset = rva_to_file_offset(idata, table->ImportAddressTableRVA);
            if (tempOffset) {
                importAddressTable = fileData + tempOffset;
            }

            printf(" DLL %s\n", dllName);

            printf("  ImportLookupTableRVA 0x%x\n", table->ImportLookupTableRVA);
            printf("  TimeDateStamp 0x%x\n", table->TimeDateStamp);
            printf("  ForwarderChain 0x%x\n", table->ForwarderChain);
            printf("  NameRVA 0x%x\n", table->NameRVA);
            printf("  ImportAddressTableRVA 0x%x\n", table->ImportAddressTableRVA);

            if (!is64bit) {
                printf("  Lookup Table\n");
                int index = 0;
                while (1) {
                    uint32_t entry = *(uint32_t*)(importLookupTable + index * 4);

                    if (entry == 0) {
                        // NULL entry means end of array.
                        break;
                    }
                    if (entry & 0x80000000) {
                        // ordinal
                    } else {
                        typedef struct {
                            uint16_t Hint;
                            char     Name[];
                        } COFF_NameEntry;

                        COFF_NameEntry* entryData = (void*)fileData + rva_to_file_offset(idata, entry & 0x7FFFFFFF);

                        printf("   %u 0x%x %s\n", index, entry, entryData->Name);
                    }

                    index++;
                }

                // Address Table has the same entries as the lookup Table.
                // When loading we overwrite the Address Table with corresponding address
                // per lookup table.

                // printf("  Address Table\n");
                // index = 0;
                // while (1) {
                //     uint32_t entry = *(uint32_t*)(importAddressTable + index * 4);

                //     if (entry == 0) {
                //         // NULL entry means end of array.
                //         break;
                //     }

                //     printf("   %u 0x%x\n", index, entry);
                    
                //     index++;
                // }
            } else {
                
            }


            printf("\n");
        }
    }

    // COFF_Section_Header* sections = (void*)(fileData + header->PointerToSymbolTable);

exit:
    if (fileData) {
        free(fileData);
    }
    if (file) {
        fclose(file);
    }
}


