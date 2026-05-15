#ifndef SILVER_LINK_ELF_H
#define SILVER_LINK_ELF_H

#include "silver/ir/ir_module.h"
#include "silver/codegen/machine.h"
#include "silver/support/buffer.h"
#include "silver/silver.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ELF类型定义
typedef uint64_t Elf64_Addr;
typedef uint64_t Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

// ELF64头
typedef struct {
    unsigned char e_ident[16];  // ELF标识
    Elf64_Half    e_type;       // 文件类型
    Elf64_Half    e_machine;    // 目标架构
    Elf64_Word    e_version;    // ELF版本
    Elf64_Addr    e_entry;      // 入口点
    Elf64_Off     e_phoff;      // 程序头表偏移
    Elf64_Off     e_shoff;      // 节头表偏移
    Elf64_Word    e_flags;      // 处理器特定标志
    Elf64_Half    e_ehsize;     // ELF头大小
    Elf64_Half    e_phentsize;  // 程序头表项大小
    Elf64_Half    e_phnum;      // 程序头表项数量
    Elf64_Half    e_shentsize;  // 节头表项大小
    Elf64_Half    e_shnum;      // 节头表项数量
    Elf64_Half    e_shstrndx;   // 节名称字符串表索引
} Elf64_Ehdr;

// ELF64节头
typedef struct {
    Elf64_Word    sh_name;      // 节名称（字符串表中的偏移）
    Elf64_Word    sh_type;      // 节类型
    Elf64_Xword   sh_flags;     // 节标志
    Elf64_Addr    sh_addr;      // 节虚拟地址
    Elf64_Off     sh_offset;    // 节文件偏移
    Elf64_Xword   sh_size;      // 节大小
    Elf64_Word    sh_link;      // 链接索引
    Elf64_Word    sh_info;      // 额外信息
    Elf64_Xword   sh_addralign; // 对齐
    Elf64_Xword   sh_entsize;   // 表项大小（如果包含固定大小项）
} Elf64_Shdr;

// ELF64符号表项
typedef struct {
    Elf64_Word    st_name;      // 符号名称（字符串表中的偏移）
    unsigned char st_info;      // 符号类型和绑定
    unsigned char st_other;     // 可见性
    Elf64_Half    st_shndx;     // 节索引
    Elf64_Addr    st_value;     // 符号值
    Elf64_Xword   st_size;      // 符号大小
} Elf64_Sym;

// ELF64重定位项
typedef struct {
    Elf64_Addr    r_offset;     // 重定位地址
    Elf64_Xword   r_info;       // 重定位类型和符号索引
    Elf64_Sxword  r_addend;     // 加数
} Elf64_Rela;

// ELF常量
#define ELF_MAG0 0x7F
#define ELF_MAG1 'E'
#define ELF_MAG2 'L'
#define ELF_MAG3 'F'

#define ELFCLASS64 2
#define ELFDATA2LSB 1
#define EV_CURRENT 1
#define ELF_OSABI_NONE 0

#define ET_REL 1
#define ET_EXEC 2

// 机器类型
#define EM_X86_64 62
#define EM_AARCH64 183
#define EM_RISCV 243

// 节类型
#define SHT_NULL     0
#define SHT_PROGBITS 1
#define SHT_SYMTAB   2
#define SHT_STRTAB   3
#define SHT_RELA     4
#define SHT_NOBITS   8
#define SHT_REL      9

// 节标志
#define SHF_WRITE     0x1
#define SHF_ALLOC     0x2
#define SHF_EXECINSTR 0x4

// 符号绑定
#define STB_LOCAL  0
#define STB_GLOBAL 1
#define STB_WEAK   2

// 符号类型
#define STT_NOTYPE  0
#define STT_OBJECT  1
#define STT_FUNC    2
#define STT_SECTION 3
#define STT_FILE    4

// 重定位类型 (x86-64)
#define R_X86_64_NONE     0
#define R_X86_64_64       1
#define R_X86_64_PC32     2
#define R_X86_64_32       10
#define R_X86_64_32S      11
#define R_X86_64_PC64     24

// 重定位类型 (AArch64)
#define R_AARCH64_NONE    0
#define R_AARCH64_ABS64   257
#define R_AARCH64_ABS32   258
#define R_AARCH64_CALL26  283
#define R_AARCH64_ADR_PREL_PG_HI21 275

// 重定位类型 (RISC-V)
#define R_RISCV_NONE      0
#define R_RISCV_64        2
#define R_RISCV_32        1
#define R_RISCV_BRANCH    16
#define R_RISCV_JAL       17
#define R_RISCV_CALL      18
#define R_RISCV_PCREL_HI20 23

// ELF格式写入函数
bool elf_write_object(IRModule *module, 
                      const SilverCompileResult *result,
                      const SilverTarget *target,
                      SilverBuffer *output);

#ifdef __cplusplus
}
#endif

#endif // SILVER_LINK_ELF_H