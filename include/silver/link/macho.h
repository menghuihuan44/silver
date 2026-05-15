#ifndef SILVER_LINK_MACHO_H
#define SILVER_LINK_MACHO_H

#include "silver/ir/ir_module.h"
#include "silver/codegen/machine.h"
#include "silver/support/buffer.h"
#include "silver/silver.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Mach-O 64位格式定义

// 魔数
#define MH_MAGIC_64 0xFEEDFACF
#define MH_CIGAM_64 0xCFFAEDFE

// CPU类型
#define CPU_TYPE_X86_64  0x01000007
#define CPU_TYPE_ARM64   0x0100000C

// 文件类型
#define MH_OBJECT 0x1

// 标志
#define MH_SUBSECTIONS_VIA_SYMBOLS 0x2000

// 加载命令
#define LC_SEGMENT_64       0x19
#define LC_SYMTAB           0x02
#define LC_DYSYMTAB         0x0B
#define LC_BUILD_VERSION    0x32

// Mach-O头
typedef struct {
    uint32_t magic;            // 魔数
    uint32_t cputype;          // CPU类型
    uint32_t cpusubtype;       // CPU子类型
    uint32_t filetype;         // 文件类型
    uint32_t ncmds;            // 加载命令数量
    uint32_t sizeofcmds;       // 加载命令总大小
    uint32_t flags;            // 标志
    uint32_t reserved;         // 保留
} __attribute__((packed)) MachOHeader64;

// 加载命令头
typedef struct {
    uint32_t cmd;              // 命令类型
    uint32_t cmdsize;          // 命令大小
} __attribute__((packed)) MachOLoadCommand;

// 段命令64
typedef struct {
    uint32_t cmd;
    uint32_t cmdsize;
    char     segname[16];
    uint64_t vmaddr;
    uint64_t vmsize;
    uint64_t fileoff;
    uint64_t filesize;
    uint32_t maxprot;
    uint32_t initprot;
    uint32_t nsects;
    uint32_t flags;
} __attribute__((packed)) MachOSegmentCommand64;

// 节头64
typedef struct {
    char     sectname[16];
    char     segname[16];
    uint64_t addr;
    uint64_t size;
    uint32_t offset;
    uint32_t align;
    uint32_t reloff;
    uint32_t nreloc;
    uint32_t flags;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
} __attribute__((packed)) MachOSection64;

// 节标志
#define S_ATTR_PURE_INSTRUCTIONS 0x80000000
#define S_ATTR_SOME_INSTRUCTIONS 0x400
#define S_ATTR_DEBUG             0x02000000

// 重定位
typedef struct {
    uint32_t r_address;
    uint32_t r_symbolnum : 24;
    uint32_t r_pcrel     : 1;
    uint32_t r_length    : 2;
    uint32_t r_extern    : 1;
    uint32_t r_type      : 4;
} __attribute__((packed)) MachORelocation;

// ARM64重定位类型
#define ARM64_RELOC_UNSIGNED      0
#define ARM64_RELOC_BRANCH26      2
#define ARM64_RELOC_PAGE21        3
#define ARM64_RELOC_PAGEOFF12     4

// x86-64重定位类型
#define X86_64_RELOC_UNSIGNED     0
#define X86_64_RELOC_SIGNED       1
#define X86_64_RELOC_BRANCH       2
#define X86_64_RELOC_GOT_LOAD     3

// 符号表
typedef struct {
    uint32_t n_strx;           // 字符串表索引
    uint8_t  n_type;           // 类型
    uint8_t  n_sect;           // 节索引
    uint16_t n_desc;           // 描述
    uint64_t n_value;          // 值
} __attribute__((packed)) MachONList64;

#define N_EXT  0x01  // 外部符号
#define N_PEXT 0x10  // 私有外部符号
#define N_SECT 0x0E  // 节符号
#define N_TYPE 0x0E  // 类型掩码

// Mach-O写入函数
bool macho_write_object(IRModule *module,
                        const SilverCompileResult *result,
                        const SilverTarget *target,
                        SilverBuffer *output);

#ifdef __cplusplus
}
#endif

#endif // SILVER_LINK_MACHO_H