#include "silver/link/elf.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

// ============================================================
// 节名称字符串表构建
// ============================================================
static uint32_t elf_build_shstrtab(char *buffer) {
    uint32_t offset = 0;
    buffer[offset++] = '\0';                          // 0: ""
    strcpy(buffer + offset, ".text"); offset += 6;    // 1: ".text"
    strcpy(buffer + offset, ".data"); offset += 6;    // 7: ".data"
    strcpy(buffer + offset, ".bss");  offset += 5;    // 13: ".bss"
    strcpy(buffer + offset, ".symtab"); offset += 8;  // 18: ".symtab"
    strcpy(buffer + offset, ".strtab"); offset += 8;  // 26: ".strtab"
    strcpy(buffer + offset, ".shstrtab"); offset += 10; // 34: ".shstrtab"
    strcpy(buffer + offset, ".rela.text"); offset += 11; // 44: ".rela.text"
    strcpy(buffer + offset, ".debug_info"); offset += 12; // 55: ".debug_info"
    return offset;
}

// ============================================================
// 写入ELF头（所有字段在写入前确定，不需要回填）
// ============================================================
static void elf_write_header(SilverBuffer *output, const SilverTarget *target,
                              uint16_t shnum, uint16_t shstrndx, uint64_t shoff) {
    Elf64_Ehdr ehdr;
    memset(&ehdr, 0, sizeof(ehdr));

    ehdr.e_ident[0] = 0x7F;
    ehdr.e_ident[1] = 'E';
    ehdr.e_ident[2] = 'L';
    ehdr.e_ident[3] = 'F';
    ehdr.e_ident[4] = ELFCLASS64;
    ehdr.e_ident[5] = ELFDATA2LSB;
    ehdr.e_ident[6] = EV_CURRENT;
    ehdr.e_ident[7] = ELF_OSABI_NONE;

    ehdr.e_type = ET_REL;
    ehdr.e_version = EV_CURRENT;

    switch (target->arch) {
        case SILVER_TARGET_X86_64:  ehdr.e_machine = EM_X86_64; break;
        case SILVER_TARGET_ARM64:   ehdr.e_machine = EM_AARCH64; break;
        case SILVER_TARGET_RISCV64: ehdr.e_machine = EM_RISCV; break;
        default:                    ehdr.e_machine = EM_X86_64; break;
    }

    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = 0;
    ehdr.e_phnum = 0;
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    ehdr.e_shnum = shnum;
    ehdr.e_shstrndx = shstrndx;
    ehdr.e_shoff = (Elf64_Off)shoff;

    silver_buffer_write(output, &ehdr, sizeof(ehdr));
}

// ============================================================
// 写入节头
// ============================================================
static void elf_write_section_header(SilverBuffer *output,
                                      uint32_t sh_name, uint32_t sh_type,
                                      uint64_t sh_flags, uint64_t sh_addr,
                                      uint64_t sh_offset, uint64_t sh_size,
                                      uint32_t sh_link, uint32_t sh_info,
                                      uint64_t sh_addralign, uint64_t sh_entsize) {
    Elf64_Shdr shdr;
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_name = sh_name;
    shdr.sh_type = sh_type;
    shdr.sh_flags = sh_flags;
    shdr.sh_addr = sh_addr;
    shdr.sh_offset = (Elf64_Off)sh_offset;
    shdr.sh_size = sh_size;
    shdr.sh_link = sh_link;
    shdr.sh_info = sh_info;
    shdr.sh_addralign = sh_addralign;
    shdr.sh_entsize = sh_entsize;
    silver_buffer_write(output, &shdr, sizeof(shdr));
}

// ============================================================
// 构建符号表和字符串表（合并，确保 st_name 正确）
// ============================================================
static void elf_build_symtab_strtab(SilverBuffer *symtab, SilverBuffer *strtab,
                                     IRModule *module) {
    // 符号0：空符号
    Elf64_Sym sym;
    memset(&sym, 0, sizeof(sym));
    silver_buffer_write(symtab, &sym, sizeof(sym));

    // 字符串表：空字符串
    silver_buffer_write_u8(strtab, 0);

    // 函数符号
    for (uint32_t i = 0; i < module->num_functions; i++) {
        const char *name = module->functions[i].name ?
                           module->functions[i].name : "";
        uint32_t str_off = (uint32_t)silver_buffer_length(strtab);
        silver_buffer_write(strtab, name, strlen(name) + 1);

        memset(&sym, 0, sizeof(sym));
        sym.st_name = str_off;
        sym.st_info = (STB_GLOBAL << 4) | STT_FUNC;
        sym.st_shndx = 1;  // .text
        silver_buffer_write(symtab, &sym, sizeof(sym));
    }

    // 全局变量符号
    for (uint32_t i = 0; i < module->num_globals; i++) {
        const char *name = module->globals[i].name ?
                           module->globals[i].name : "";
        uint32_t str_off = (uint32_t)silver_buffer_length(strtab);
        silver_buffer_write(strtab, name, strlen(name) + 1);

        memset(&sym, 0, sizeof(sym));
        sym.st_name = str_off;
        sym.st_info = (STB_GLOBAL << 4) | STT_OBJECT;
        sym.st_shndx = 2;  // .data
        sym.st_size = module->globals[i].size;
        silver_buffer_write(symtab, &sym, sizeof(sym));
    }
}

// ============================================================
// 主ELF写入函数（预先计算所有偏移，一次性写入，无需回填）
// ============================================================
bool elf_write_object(IRModule *module,
                      const SilverCompileResult *result,
                      const SilverTarget *target,
                      SilverBuffer *output) {
    if (!module || !result || !target || !output) return false;

    // 各段大小
    uint32_t text_size = silver_buffer_length(result->code);
    uint32_t data_size = silver_buffer_length(result->data);
    uint32_t debug_size = result->debug_info ?
                          (uint32_t)silver_buffer_length(result->debug_info) : 0;

    // 构建节名称字符串表
    char shstrtab[256];
    uint32_t shstrtab_size = elf_build_shstrtab(shstrtab);

    // 构建符号表和字符串表
    SilverBuffer *symtab = silver_buffer_create(4096);
    SilverBuffer *strtab = silver_buffer_create(4096);
    if (!symtab || !strtab) {
        if (symtab) silver_buffer_destroy(symtab);
        if (strtab) silver_buffer_destroy(strtab);
        return false;
    }
    elf_build_symtab_strtab(symtab, strtab, module);

    uint32_t symtab_size = (uint32_t)silver_buffer_length(symtab);
    uint32_t strtab_size = (uint32_t)silver_buffer_length(strtab);

    // 节数量
    uint32_t num_sections = 7; // null, .text, .data, .bss, .symtab, .strtab, .shstrtab
    if (debug_size > 0) num_sections++; // .debug_info

    // ============================================================
    // 预先计算所有偏移（在写入任何数据之前）
    // ============================================================
    uint32_t header_size = sizeof(Elf64_Ehdr);
    uint32_t shdr_size = num_sections * sizeof(Elf64_Shdr);

    uint32_t text_offset   = header_size;
    uint32_t data_offset   = text_offset + text_size;
    uint32_t symtab_offset = data_offset + data_size;
    uint32_t strtab_offset = symtab_offset + symtab_size;
    uint32_t shstrtab_offset = strtab_offset + strtab_size;
    uint32_t debug_offset  = shstrtab_offset + shstrtab_size;
    uint32_t shdr_offset   = (debug_size > 0) ? (debug_offset + debug_size) : shstrtab_offset + shstrtab_size;

    uint16_t shstrndx = (uint16_t)(num_sections - 1);
    // .symtab 的 sh_link 指向 .strtab，即节索引 5
    uint32_t strtab_index = 5;

    // ============================================================
    // 写入ELF头（一次性，所有字段正确，不需要回填）
    // ============================================================
    elf_write_header(output, target, (uint16_t)num_sections, shstrndx, shdr_offset);

    // ============================================================
    // 写入各段内容
    // ============================================================

    // .text
    silver_buffer_write(output, silver_buffer_data(result->code), text_size);

    // .data
    if (data_size > 0) {
        silver_buffer_write(output, silver_buffer_data(result->data), data_size);
    }

    // .symtab
    silver_buffer_write(output, silver_buffer_data(symtab), symtab_size);

    // .strtab
    silver_buffer_write(output, silver_buffer_data(strtab), strtab_size);

    // .shstrtab
    silver_buffer_write(output, shstrtab, shstrtab_size);

    // .debug_info
    if (debug_size > 0) {
        silver_buffer_write(output, silver_buffer_data(result->debug_info), debug_size);
    }

    // ============================================================
    // 写入节头表（所有偏移已在上面计算好）
    // ============================================================

    // SHT_NULL (索引0)
    elf_write_section_header(output, 0, SHT_NULL, 0, 0, 0, 0, 0, 0, 0, 0);

    // .text (索引1)
    elf_write_section_header(output, 1, SHT_PROGBITS,
                              SHF_ALLOC | SHF_EXECINSTR,
                              0, text_offset, text_size,
                              0, 0, 16, 0);

    // .data (索引2)
    elf_write_section_header(output, 7, SHT_PROGBITS,
                              SHF_ALLOC | SHF_WRITE,
                              0, data_offset, data_size,
                              0, 0, 8, 0);

    // .bss (索引3)
    elf_write_section_header(output, 13, SHT_NOBITS,
                              SHF_ALLOC | SHF_WRITE,
                              0, 0, 0,
                              0, 0, 8, 0);

    // .symtab (索引4)
    elf_write_section_header(output, 18, SHT_SYMTAB,
                              0,
                              0, symtab_offset, symtab_size,
                              strtab_index, 1, 8, sizeof(Elf64_Sym));

    // .strtab (索引5)
    elf_write_section_header(output, 26, SHT_STRTAB,
                              0,
                              0, strtab_offset, strtab_size,
                              0, 0, 1, 0);

    // .shstrtab (索引6，最后一个索引是 shstrndx)
    elf_write_section_header(output, 34, SHT_STRTAB,
                              0,
                              0, shstrtab_offset, shstrtab_size,
                              0, 0, 1, 0);

    // .debug_info (索引7，如果存在)
    if (debug_size > 0) {
        elf_write_section_header(output, 55, SHT_PROGBITS,
                                 0,
                                 0, debug_offset, debug_size,
                                 0, 0, 1, 0);
    }

    // 清理
    silver_buffer_destroy(symtab);
    silver_buffer_destroy(strtab);

    return true;
}