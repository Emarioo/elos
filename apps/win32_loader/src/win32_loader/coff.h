/*

    A COFF object file begins with COFF_File_Header.

    An executable begins with dos stub and signature.
    At 0x3c is a u32 offset to the signature in the file.
    After the 4-byte signature (PE\0\0) is the COFF_File_Header.

*/


#pragma once


#include <stdint.h>

// This information comes from this site:
// https://learn.microsoft.com/en-us/windows/win32/debug/pe-format

typedef enum _Machine_Type {
    MACHINE_ZERO = 0,
    IMAGE_FILE_MACHINE_I386  = 0x14c,
    IMAGE_FILE_MACHINE_AMD64 = 0x8664,
    IMAGE_FILE_MACHINE_ARM   = 0x1c0,
    IMAGE_FILE_MACHINE_ARM64 = 0xaa64,
} _Machine_Type;
typedef uint16_t Machine_Type;

typedef enum _COFF_Header_Flag {
    CHARACTERISTICS_ZERO=0,
    IMAGE_FILE_RELOCS_STRIPPED = 0x0001, // Image only, Windows CE, and Microsoft Windows NT and later. This indicates that the file does not contain base relocations and must therefore be loaded at its preferred base address. If the base address is not available, the loader reports an error. The default behavior of the linker is to strip base relocations from executable (EXE) files.
    IMAGE_FILE_EXECUTABLE_IMAGE = 0x0002, // Image only. This indicates that the image file is valid and can be run. If this flag is not set, it indicates a linker error.
    IMAGE_FILE_LINE_NUMS_STRIPPED = 0x0004, // COFF line numbers have been removed. This flag is deprecated and should be zero.
    IMAGE_FILE_LOCAL_SYMS_STRIPPED = 0x0008, // COFF symbol table entries for local symbols have been removed. This flag is deprecated and should be zero.
    IMAGE_FILE_AGGRESSIVE_WS_TRIM = 0x0010, // Obsolete. Aggressively trim working set. This flag is deprecated for Windows 2000 and later and must be zero.
    IMAGE_FILE_LARGE_ADDRESS_AWARE = 0x0020, // Application can handle > 2-GB addresses. = 0x0040, // This flag is reserved for future use.
    IMAGE_FILE_BYTES_REVERSED_LO = 0x0080, // Little endian: the least significant bit (LSB) precedes the most significant bit (MSB) in memory. This flag is deprecated and should be zero.
    IMAGE_FILE_32BIT_MACHINE = 0x0100, // Machine is based on a 32-bit-word architecture.
    IMAGE_FILE_DEBUG_STRIPPED = 0x0200, // Debugging information is removed from the image file.
    IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP = 0x0400, // If the image is on removable media, fully load it and copy it to the swap file.
    IMAGE_FILE_NET_RUN_FROM_SWAP = 0x0800, // If the image is on network media, fully load it and copy it to the swap file.
    IMAGE_FILE_SYSTEM = 0x1000, // The image file is a system file, not a user program.
    IMAGE_FILE_DLL = 0x2000, // The image file is a dynamic-link library (DLL). Such files are considered executable files for almost all purposes, although they cannot be directly run.
    IMAGE_FILE_UP_SYSTEM_ONLY = 0x4000, // The file should be run only on a uniprocessor machine.
    IMAGE_FILE_BYTES_REVERSED_HI = 0x8000, // Big endian: the MSB precedes the LSB in memory. This flag is deprecated and should be zero.
} _COFF_Header_Flag;
typedef uint16_t COFF_Header_Flag;

#pragma pack(push, 1)
#define COFF_File_Header_SIZE 20
typedef struct COFF_File_Header {
    Machine_Type Machine;
    uint16_t NumberOfSections;
    uint32_t TimeDateStamp;
    uint32_t PointerToSymbolTable;
    uint32_t NumberOfSymbols;
    uint16_t SizeOfOptionalHeader; // always zero for object files (not executables)
    COFF_Header_Flag Characteristics;
} COFF_File_Header;
#pragma pack(pop)

#define COFF_OPTIONAL_HEADER_MAGIC_PE32      0x10b
#define COFF_OPTIONAL_HEADER_MAGIC_PE32_PLUS 0x20b

#pragma pack(push, 1)
#define COFF_Optional_Header_32_SIZE 28
#define COFF_Optional_Header_64_SIZE 24
typedef struct COFF_Optional_Header {
    uint16_t Magic;
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
    uint32_t BaseOfData; // Only available for PE32
} COFF_Optional_Header;
#pragma pack(pop)

typedef enum COFF_Subsystem {
    IMAGE_SUBSYSTEM_UNKNOWN = 0,                    // An unknown subsystem
    IMAGE_SUBSYSTEM_NATIVE = 1,                     // Device drivers and native Windows processes
    IMAGE_SUBSYSTEM_WINDOWS_GUI = 2,                // The Windows graphical user interface (GUI) subsystem
    IMAGE_SUBSYSTEM_WINDOWS_CUI = 3,                // The Windows character subsystem
    IMAGE_SUBSYSTEM_OS2_CUI = 5,                    // The OS/2 character subsystem
    IMAGE_SUBSYSTEM_POSIX_CUI = 7,                  // The Posix character subsystem
    IMAGE_SUBSYSTEM_NATIVE_WINDOWS = 8,             // Native Win9x driver
    IMAGE_SUBSYSTEM_WINDOWS_CE_GUI = 9,             // Windows CE
    IMAGE_SUBSYSTEM_EFI_APPLICATION = 10,           // An Extensible Firmware Interface (EFI) application
    IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER = 11,  // An EFI driver with boot services
    IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER = 12,       // An EFI driver with run-time services
    IMAGE_SUBSYSTEM_EFI_ROM = 13,                   // An EFI ROM image
    IMAGE_SUBSYSTEM_XBOX = 14,                      // XBOX
    IMAGE_SUBSYSTEM_WINDOWS_BOOT_APPLICATION = 16,  // Windows boot application.
} _COFF_Subsystem;
typedef uint16_t COFF_Subsystem;

typedef enum _COFF_DLL_Characteristics {
    IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA  = 0x0020,         // Image can handle a high entropy 64-bit virtual address space.
    IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE  = 0x0040,            // DLL can be relocated at load time.
    IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY  = 0x0080,         // Code Integrity checks are enforced.
    IMAGE_DLLCHARACTERISTICS_NX_COMPAT  = 0x0100,               // Image is NX compatible.
    IMAGE_DLLCHARACTERISTICS_NO_ISOLATION  = 0x0200,            // Isolation aware, but do not isolate the image.
    IMAGE_DLLCHARACTERISTICS_NO_SEH  = 0x0400,                  // Does not use structured exception (SE) handling. No SE handler may be called in this image.
    IMAGE_DLLCHARACTERISTICS_NO_BIND  = 0x0800,                 // Do not bind the image.
    IMAGE_DLLCHARACTERISTICS_APPCONTAINER  = 0x1000,            // Image must execute in an AppContainer.
    IMAGE_DLLCHARACTERISTICS_WDM_DRIVER  = 0x2000,              // A WDM driver.
    IMAGE_DLLCHARACTERISTICS_GUARD_CF  = 0x4000,                // Image supports Control Flow Guard.
    IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE  = 0x8000,   // Terminal Server aware.
} _COFF_DLL_Characteristics;
typedef uint16_t COFF_DLL_Characteristics;

#pragma pack(push, 1)
#define COFF_Optional_Windows_Header_32_SIZE      68
typedef struct COFF_Optional_Windows_Header_32 {
    uint32_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    COFF_Subsystem Subsystem;
    uint16_t DllCharacteristics;
    uint32_t SizeOfStackReserve;
    uint32_t SizeOfStackCommit;
    uint32_t SizeOfHeapReserve;
    uint32_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
} COFF_Optional_Windows_Header_32;
#pragma pack(pop)

#pragma pack(push, 1)
#define COFF_Optional_Windows_Header_64_SIZE 88
typedef struct COFF_Optional_Windows_Header_64 {
    uint64_t ImageBase;
    uint32_t SectionAlignment;
    uint32_t FileAlignment;
    uint16_t MajorOperatingSystemVersion;
    uint16_t MinorOperatingSystemVersion;
    uint16_t MajorImageVersion;
    uint16_t MinorImageVersion;
    uint16_t MajorSubsystemVersion;
    uint16_t MinorSubsystemVersion;
    uint32_t Win32VersionValue;
    uint32_t SizeOfImage;
    uint32_t SizeOfHeaders;
    uint32_t CheckSum;
    COFF_Subsystem Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
} COFF_Optional_Windows_Header_64;
#pragma pack(pop)

typedef struct COFF_Image_Data_Directory {
    uint32_t VirtualAddress;
    uint32_t Size;
} COFF_Image_Data_Directory;

#pragma pack(push, 1)
typedef struct COFF_Optional_Data_Directories {
    COFF_Image_Data_Directory ExportTable;
    COFF_Image_Data_Directory ImportTable;
    COFF_Image_Data_Directory ResourceTable;
    COFF_Image_Data_Directory ExceptionTable;
    COFF_Image_Data_Directory CertificateTable;
    COFF_Image_Data_Directory BaseRelocationTable;
    COFF_Image_Data_Directory Debug;
    COFF_Image_Data_Directory Architecture;
    COFF_Image_Data_Directory GlobalPtr;
    COFF_Image_Data_Directory TLSTable;
    COFF_Image_Data_Directory LoadConfigTable;
    COFF_Image_Data_Directory BoundImport;
    COFF_Image_Data_Directory IAT;
    COFF_Image_Data_Directory DelayImportDescriptor;
    COFF_Image_Data_Directory CLRRuntimeHeader;
    COFF_Image_Data_Directory _zero;
} COFF_Optional_Data_Directories;
#pragma pack(pop)

typedef enum Section_Flag {
    SECTION_FLAG_ZERO = 0,

    IMAGE_SCN_TYPE_NO_PAD = 0x00000008, // The section should not be padded to the next boundary. This flag is obsolete and is replaced by IMAGE_SCN_ALIGN_1BYTES. This is valid only for object files.
    IMAGE_SCN_CNT_CODE = 0x00000020, // The section contains executable code.
    IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040, // The section contains initialized data.
    IMAGE_SCN_CNT_UNINITIALIZED_DATA = 0x00000080, // The section contains uninitialized data.
    IMAGE_SCN_LNK_OTHER = 0x00000100, // Reserved for future use.
    IMAGE_SCN_LNK_INFO = 0x00000200, // The section contains comments or other information. The .drectve section has this type. This is valid for object files only.
    IMAGE_SCN_LNK_REMOVE = 0x00000800, // The section will not become part of the image. This is valid only for object files.
    IMAGE_SCN_LNK_COMDAT = 0x00001000, // The section contains COMDAT data. For more information, see COMDAT Sections (Object Only). This is valid only for object files.
    IMAGE_SCN_GPREL = 0x00008000, // The section contains data referenced through the global pointer (GP).
    IMAGE_SCN_MEM_PURGEABLE = 0x00020000, // Reserved for future use.
    IMAGE_SCN_MEM_16BIT = 0x00020000, // Reserved for future use.
    IMAGE_SCN_MEM_LOCKED = 0x00040000, // Reserved for future use.
    IMAGE_SCN_MEM_PRELOAD = 0x00080000, // Reserved for future use.
    IMAGE_SCN_ALIGN_1BYTES = 0x00100000, // Align data on a 1-byte boundary. Valid only for object files.
    IMAGE_SCN_ALIGN_2BYTES = 0x00200000, // Align data on a 2-byte boundary. Valid only for object files.
    IMAGE_SCN_ALIGN_4BYTES = 0x00300000, // Align data on a 4-byte boundary. Valid only for object files.
    IMAGE_SCN_ALIGN_8BYTES = 0x00400000, // Align data on an 8-byte boundary. Valid only for object files.
    IMAGE_SCN_ALIGN_16BYTES = 0x00500000, // Align data on a 16-byte boundary. Valid only for object files.
    IMAGE_SCN_ALIGN_32BYTES = 0x00600000, // Align data on a 32-byte boundary. Valid only for object files.
    IMAGE_SCN_ALIGN_64BYTES = 0x00700000, // Align data on a 64-byte boundary. Valid only for object files.
    IMAGE_SCN_ALIGN_128BYTES = 0x00800000, // Align data on a 128-byte boundary. Valid only for object files.
    IMAGE_SCN_ALIGN_256BYTES = 0x00900000, // Align data on a 256-byte boundary. Valid only for object files.
    IMAGE_SCN_ALIGN_512BYTES = 0x00A00000, // Align data on a 512-byte boundary. Valid only for object files.
    IMAGE_SCN_ALIGN_1024BYTES = 0x00B00000, // Align data on a 1024-byte boundary. Valid only for object files.
    IMAGE_SCN_ALIGN_2048BYTES = 0x00C00000, // Align data on a 2048-byte boundary. Valid only for object files.
    IMAGE_SCN_ALIGN_4096BYTES = 0x00D00000, // Align data on a 4096-byte boundary. Valid only for object files.
    IMAGE_SCN_ALIGN_8192BYTES = 0x00E00000, // Align data on an 8192-byte boundary. Valid only for object files.
    IMAGE_SCN_LNK_NRELOC_OVFL = 0x01000000, // The section contains extended relocations.
    IMAGE_SCN_MEM_DISCARDABLE = 0x02000000, // The section can be discarded as needed.
    IMAGE_SCN_MEM_NOT_CACHED = 0x04000000, // The section cannot be cached.
    IMAGE_SCN_MEM_NOT_PAGED = 0x08000000, // The section is not pageable.
    IMAGE_SCN_MEM_SHARED = 0x10000000, // The section can be shared in memory.
    IMAGE_SCN_MEM_EXECUTE = 0x20000000, // The section can be executed as code.
    IMAGE_SCN_MEM_READ = 0x40000000, // The section can be read.
    IMAGE_SCN_MEM_WRITE = 0x80000000, // The section can be written to.
} Section_Flag;
typedef uint32_t Section_Flags;

#pragma pack(push, 1)
// section table entry
#define COFF_Section_Header_SIZE 40
typedef struct COFF_Section_Header {
    char Name[8]; // null terminated unless name is exactly 8 bytes. Longer names exist in the string table. They are referred to like this "/index\0" (ex, "/23\0")
    uint32_t VirtualSize; // 0 for object files
    uint32_t VirtualAddress; // should be 0 for simplicity, it offsets relocations but isn't necessary.
    uint32_t SizeOfRawData; // size of section data
    uint32_t PointerToRawData; // 4-byte aligned for best performance
    uint32_t PointerToRelocations;
    uint32_t PointerToLineNumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLineNumbers;
    Section_Flags Characteristics;
} COFF_Section_Header;
#pragma pack(pop)

typedef enum _Type_Indicator {
    TYPE_INDICATOR_ZERO = 0,
    IMAGE_REL_AMD64_ABSOLUTE = 0x0000, // The relocation is ignored.
    IMAGE_REL_AMD64_ADDR64 = 0x0001, // The 64-bit VA of the relocation target.
    IMAGE_REL_AMD64_ADDR32 = 0x0002, // The 32-bit VA of the relocation target.
    IMAGE_REL_AMD64_ADDR32NB = 0x0003, // The 32-bit address without an image base (RVA).
    IMAGE_REL_AMD64_REL32 = 0x0004, // The 32-bit relative address from the byte following the relocation.
    IMAGE_REL_AMD64_REL32_1 = 0x0005, // The 32-bit address relative to byte distance 1 from the relocation.
    IMAGE_REL_AMD64_REL32_2 = 0x0006, // The 32-bit address relative to byte distance 2 from the relocation.
    IMAGE_REL_AMD64_REL32_3 = 0x0007, // The 32-bit address relative to byte distance 3 from the relocation.
    IMAGE_REL_AMD64_REL32_4 = 0x0008, // The 32-bit address relative to byte distance 4 from the relocation.
    IMAGE_REL_AMD64_REL32_5 = 0x0009, // The 32-bit address relative to byte distance 5 from the relocation.
    IMAGE_REL_AMD64_SECTION = 0x000A, // The 16-bit section index of the section that contains the target. This is used to support debugging information.
    IMAGE_REL_AMD64_SECREL = 0x000B, // The 32-bit offset of the target from the beginning of its section. This is used to support debugging information and static thread local storage.
    IMAGE_REL_AMD64_SECREL7 = 0x000C, // A 7-bit unsigned offset from the base of the section that contains the target.
    IMAGE_REL_AMD64_TOKEN = 0x000D, // CLR tokens.
    IMAGE_REL_AMD64_SREL32 = 0x000E, // A 32-bit signed span-dependent value emitted into the object.
    IMAGE_REL_AMD64_PAIR = 0x000F, // A pair that must immediately follow every span-dependent value.
    IMAGE_REL_AMD64_SSPAN32 = 0x0010, // A 32-bit signed span-dependent value that is applied at link time.
} _Type_Indicator;
typedef uint16_t Type_Indicator;

#pragma pack(push, 1)
// relocation entry/record
#define COFF_Relocation_SIZE 10
typedef struct COFF_Relocation {
    uint32_t VirtualAddress;
    uint32_t SymbolTableIndex;
    Type_Indicator Type;
} COFF_Relocation;

#pragma pack(pop)
typedef enum _Storage_Class {
    IMAGE_SYM_CLASS_END_OF_FUNCTION = (uint8_t)-1, // A special symbol that represents the end of function, for debugging purposes.
    IMAGE_SYM_CLASS_NULL = 0, // No assigned storage class.
    IMAGE_SYM_CLASS_AUTOMATIC = 1, // The automatic (stack) variable. The Value field specifies the stack frame offset.
    IMAGE_SYM_CLASS_EXTERNAL = 2, // A value that Microsoft tools use for external symbols. The Value field indicates the size if the section number is IMAGE_SYM_UNDEFINED = 0, //  If the section number is not zero, then the Value field specifies the offset within the section.
    IMAGE_SYM_CLASS_STATIC = 3, // The offset of the symbol within the section. If the Value field is zero, then the symbol represents a section name.
    IMAGE_SYM_CLASS_REGISTER = 4, // A register variable. The Value field specifies the register number.
    IMAGE_SYM_CLASS_EXTERNAL_DEF = 5, // A symbol that is defined externally.
    IMAGE_SYM_CLASS_LABEL = 6, // A code label that is defined within the module. The Value field specifies the offset of the symbol within the section.
    IMAGE_SYM_CLASS_UNDEFINED_LABEL = 7, // A reference to a code label that is not defined.
    IMAGE_SYM_CLASS_MEMBER_OF_STRUCT = 8, // The structure member. The Value field specifies the n th member.
    IMAGE_SYM_CLASS_ARGUMENT = 9, // A formal argument (parameter) of a function. The Value field specifies the n th argument.
    IMAGE_SYM_CLASS_STRUCT_TAG = 10, // The structure tag-name entry.
    IMAGE_SYM_CLASS_MEMBER_OF_UNION = 11, // A union member. The Value field specifies the n th member.
    IMAGE_SYM_CLASS_UNION_TAG = 12, // The Union tag-name entry.
    IMAGE_SYM_CLASS_TYPE_DEFINITION = 13, // A Typedef entry.
    IMAGE_SYM_CLASS_UNDEFINED_STATIC = 14, // A static data declaration.
    IMAGE_SYM_CLASS_ENUM_TAG = 15, // An enumerated type tagname entry.
    IMAGE_SYM_CLASS_MEMBER_OF_ENUM = 16, // A member of an enumeration. The Value field specifies the n th member.
    IMAGE_SYM_CLASS_REGISTER_PARAM = 17, // A register parameter.
    IMAGE_SYM_CLASS_BIT_FIELD = 18, // A bit-field reference. The Value field specifies the n th bit in the bit field.
    IMAGE_SYM_CLASS_BLOCK = 100, // A .bb (beginning of block) or .eb (end of block) record. The Value field is the relocatable address of the code location.
    IMAGE_SYM_CLASS_FUNCTION = 101, // A value that Microsoft tools use for symbol records that define the extent of a function: begin function (.bf ), end function ( .ef ), and lines in function ( .lf ). For .lf records, the Value field gives the number of source lines in the function. For .ef records, the Value field gives the size of the function code.
    IMAGE_SYM_CLASS_END_OF_STRUCT = 102, // An end-of-structure entry.
    IMAGE_SYM_CLASS_FILE = 103, // A value that Microsoft tools, as well as traditional COFF format, use for the source-file symbol record. The symbol is followed by auxiliary records that name the file.
    IMAGE_SYM_CLASS_SECTION = 104, // A definition of a section (Microsoft tools use STATIC storage class instead).
    IMAGE_SYM_CLASS_WEAK_EXTERNAL = 105, // A weak external. For more information, see Auxiliary Format 3: Weak Externals.
    IMAGE_SYM_CLASS_CLR_TOKEN = 107, // A CLR token symbol. The name is an ASCII string that consists of the hexadecimal value of the token. For more information, see CLR Token Definition (Object Only).
} _Storage_Class;
typedef uint8_t Storage_Class;

typedef enum _Type_Representation_LSB { _521515 /* TODO: Add types */ } _Type_Representation_LSB;
typedef uint8_t Type_Representation_LSB;

typedef enum _Type_Representation_MSB { 
    // These may be wrong but the object file has 32 as value for main symbol
    // main is supposed to be a function. That would be MSB. MSB is the second byte.
    // 256*2 which is 512. Is that not the value it should and not 32?
    // I am missing something. I guess I don't understand Little endian, MSB and LSB
    IMAGE_SYM_DTYPE_NULL = 0, // No derived type; the symbol is a simple scalar variable.
    IMAGE_SYM_DTYPE_POINTER = 0x10, // The symbol is a pointer to base type.
    IMAGE_SYM_DTYPE_FUNCTION = 0x20, // The symbol is a function that returns a base type.
    IMAGE_SYM_DTYPE_ARRAY = 0x30, // The symbol is an array of base type.
} _Type_Representation_MSB;
typedef uint8_t Type_Representation_MSB;

#pragma pack(push,1)
#define COFF_Symbol_Record_SIZE 18
typedef struct COFF_Symbol_Record {
    union {
        char ShortName[8];
        struct {
            uint32_t zero; // if zero then the name is longer than 8 bytes
            uint32_t offset; // offset into string table
        };
    } Name;
    uint32_t Value;
    int16_t SectionNumber; // Starts from 1. A number less than that has special meaning like unknown/undefined
    // struct {
    //     Type_Representation_MSB complex;
    //     Type_Representation_LSB base;
    // }
    uint16_t Type;
    Storage_Class StorageClass;
    uint8_t NumberOfAuxSymbols;
} COFF_Symbol_Record;

#define Aux_Format_5_SIZE Symbol_Record_SIZE
typedef struct Aux_Format_5 {
    uint32_t Length;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLineNumbers;
    uint32_t CheckSum;
    uint16_t Number;
    uint8_t Selection;
    uint8_t Unused[3];
} Aux_Format_5;
#pragma pack(pop)

// Below are some structs for exceptions on Windows.
// Definitions can be found here: https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170
typedef struct RUNTIME_FUNCTION {
    uint32_t StartAddress;
    uint32_t EndAddress;
    uint32_t UnwindInfoAddress;
} RUNTIME_FUNCTION;

typedef enum UnwindInfoFlags {
    UNW_FLAG_NHANDLER = 0x0,
    UNW_FLAG_EHANDLER = 0x1, // The function has an exception handler that should be called when looking for functions that need to examine exceptions.
    UNW_FLAG_UHANDLER = 0x2, // The function has a termination handler that should be called when unwinding an exception.
    UNW_FLAG_CHAININFO = 0x4, //This unwind info structure is not the primary one for the procedure. Instead, the chained unwind info entry is the contents of a previous RUNTIME_FUNCTION entry. For information, see Chained unwind info structures. If this flag is set, then the UNW_FLAG_EHANDLER and UNW_FLAG_UHANDLER flags must be cleared. Also, the frame register and fixed-stack allocation fields must have the same values as in the primary unwind info.
} UnwindInfoFlags;

typedef struct UNWIND_INFO {
    uint8_t Version : 3;
    uint8_t Flags : 5;
    uint8_t SizeOfProlog;
    uint8_t CountOfUnwindCodes;
    uint8_t FrameRegister : 4;
    uint8_t FrameRegisterOffset : 4; // (scaled)
    // uint16_t UnwindCodesArray[CountOfUnwindCodes];

    // variable	Can either be of form (1):
        // uint32_t AddressOfExceptionHandler;
        // variable	Language-specific handler data (optional)
    // or (2):
        // RUNTIME_FUNCTION ChainedUnwindInfo;
} UNWIND_INFO;

typedef struct UNWIND_CODE {
    uint8_t OffsetInProlog;
    uint8_t UnwindOperationCode : 4;
    uint8_t OperationInfo : 4;
} UNWIND_CODE;

// Read about the operations here: https://learn.microsoft.com/en-us/cpp/build/exception-handling-x64?view=msvc-170#unwind-operation-code
typedef enum UnwindOperation {
    UWOP_PUSH_NONVOL = 0, // 1 node
    UWOP_ALLOC_LARGE = 1, // 2 or 3 nodes
    UWOP_ALLOC_SMALL = 2, // 1 node
    UWOP_SET_FPREG = 3, // 1 node
    UWOP_SAVE_NONVOL = 4, // 2 nodes
    UWOP_SAVE_NONVOL_FAR = 5, // 3 nodes
    UWOP_SAVE_XMM128 = 8, // 2 nodes
    UWOP_SAVE_XMM128_FAR = 9, // 3 nodes
    UWOP_PUSH_MACHFRAME = 10, // 1 node
} UnwindOperation;

typedef enum UnwindOpRegister {
    UWOP_RAX = 0,
    UWOP_RCX = 1,
    UWOP_RDX = 2,
    UWOP_RBX = 3,
    UWOP_RSP = 4,
    UWOP_RBP = 5,
    UWOP_RSI = 6,
    UWOP_RDI = 7,
    UWOP_R8 = 8, // To get R13 do: UWOP_R8 + (8 - N), where N = 13
    // 8 to 15	R8 to R15
} UnwindOpRegister;


#pragma pack(push, 1)
typedef struct COFF_Import_Directory_Table {
    uint32_t ImportLookupTableRVA;
    uint32_t TimeDateStamp;
    uint32_t ForwarderChain;
    uint32_t NameRVA;
    uint32_t ImportAddressTableRVA;
} COFF_Import_Directory_Table;
#pragma pack(pop)
