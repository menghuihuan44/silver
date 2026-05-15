#ifndef SILVER_LINK_PE_H
#define SILVER_LINK_PE_H

#include "silver/ir/ir_module.h"
#include "silver/codegen/machine.h"
#include "silver/support/buffer.h"
#include "silver/silver.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// PE/COFF格式定义

// COFF文件头
typedef struct {
    uint16_t Machine;               // 目标机器类型
    uint16_t NumberOfSections;      // 节数量
    uint32_t TimeDateStamp;         // 时间戳
    uint32_t PointerToSymbolTable;  // 符号表偏移
    uint32_t NumberOfSymbols;       // 符号数量
    uint16_t SizeOfOptionalHeader;  // 可选头大小
    uint16_t Characteristics;       // 文件特征
} __attribute__((packed)) COFFHeader;

// 机器类型
#define IMAGE_FILE_MACHINE_AMD64  0x8664
#define IMAGE_FILE_MACHINE_ARM64  0xAA64
#define IMAGE_FILE_MACHINE_RISCV64 0x5064

// 文件特征标志
#define IMAGE_FILE_RELOCS_STRIPPED       0x0001
#define IMAGE_FILE_EXECUTABLE_IMAGE      0x0002
#define IMAGE_FILE_LINE_NUMS_STRIPPED    0x0004
#define IMAGE_FILE_LOCAL_SYMS_STRIPPED   0x0008
#define IMAGE_FILE_DEBUG_STRIPPED        0x0200

// PE可选头 (PE32+)
typedef struct {
    uint16_t Magic;                 // PE32+ = 0x20B
    uint8_t  MajorLinkerVersion;
    uint8_t  MinorLinkerVersion;
    uint32_t SizeOfCode;
    uint32_t SizeOfInitializedData;
    uint32_t SizeOfUninitializedData;
    uint32_t AddressOfEntryPoint;
    uint32_t BaseOfCode;
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
    uint16_t Subsystem;
    uint16_t DllCharacteristics;
    uint64_t SizeOfStackReserve;
    uint64_t SizeOfStackCommit;
    uint64_t SizeOfHeapReserve;
    uint64_t SizeOfHeapCommit;
    uint32_t LoaderFlags;
    uint32_t NumberOfRvaAndSizes;
    // 数据目录省略（简化实现）
} __attribute__((packed)) PEOptionalHeader64;

// 节头
typedef struct {
    char     Name[8];               // 节名称
    uint32_t VirtualSize;
    uint32_t VirtualAddress;
    uint32_t SizeOfRawData;
    uint32_t PointerToRawData;
    uint32_t PointerToRelocations;
    uint32_t PointerToLinenumbers;
    uint16_t NumberOfRelocations;
    uint16_t NumberOfLinenumbers;
    uint32_t Characteristics;
} __attribute__((packed)) PESectionHeader;

// 节特征
#define IMAGE_SCN_CNT_CODE              0x00000020
#define IMAGE_SCN_CNT_INITIALIZED_DATA  0x00000040
#define IMAGE_SCN_MEM_EXECUTE           0x20000000
#define IMAGE_SCN_MEM_READ              0x40000000
#define IMAGE_SCN_MEM_WRITE             0x80000000

// PE重定位类型
#define IMAGE_REL_AMD64_ADDR64          1
#define IMAGE_REL_AMD64_ADDR32          2
#define IMAGE_REL_AMD64_REL32           4
#define IMAGE_REL_AMD64_REL32_1         5
#define IMAGE_REL_AMD64_REL32_2         6
#define IMAGE_REL_AMD64_REL32_3         7
#define IMAGE_REL_AMD64_REL32_4         8
#define IMAGE_REL_AMD64_REL32_5         9
#define IMAGE_REL_AMD64_SECTION         10
#define IMAGE_REL_AMD64_SECREL          11

// PE重定位项
typedef struct {
    uint32_t VirtualAddress;
    uint32_t SymbolTableIndex;
    uint16_t Type;
} __attribute__((packed)) PERelocation;

// PE格式写入函数
bool pe_write_object(IRModule *module,
                     const SilverCompileResult *result,
                     const SilverTarget *target,
                     SilverBuffer *output);

#ifdef __cplusplus
}
#endif

#endif // SILVER_LINK_PE_H