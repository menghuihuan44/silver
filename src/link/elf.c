#include "silver/link/elf.h"
#include <string.h>
#include <stdio.h>

// 写入ELF头
static void elf_write_header(SilverBuffer *output, const SilverTarget *target) {
    Elf64_Ehdr ehdr;
    memset(&ehdr, 0, sizeof(ehdr));
    
    // ELF标识
    ehdr.e_ident[0] = ELF_MAG0;
    ehdr.e_ident[1] = ELF_MAG1;
    ehdr.e_ident[2] = ELF_MAG2;
    ehdr.e_ident[3] = ELF_MAG3;
    ehdr.e_ident[4] = ELFCLASS64;    // 64位
    ehdr.e_ident[5] = ELFDATA2LSB;   // 小端
    ehdr.e_ident[6] = EV_CURRENT;
    ehdr.e_ident[7] = ELF_OSABI_NONE;
    
    ehdr.e_type = ET_REL;  // 可重定位目标文件
    ehdr.e_version = EV_CURRENT;
    
    // 设置机器类型
    switch (target->arch) {
        case SILVER_TARGET_X86_64:
            ehdr.e_machine = EM_X86_64;
            break;
        case SILVER_TARGET_ARM64:
            ehdr.e_machine = EM_AARCH64;
            break;
        case SILVER_TARGET_RISCV64:
            ehdr.e_machine = EM_RISCV;
            break;
        default:
            ehdr.e_machine = EM_X86_64;
            break;
    }
    
    ehdr.e_ehsize = sizeof(Elf64_Ehdr);
    ehdr.e_phentsize = 0;   // 目标文件没有程序头
    ehdr.e_phnum = 0;
    ehdr.e_shentsize = sizeof(Elf64_Shdr);
    
    // 稍后回填节头数量
    ehdr.e_shnum = 0;
    ehdr.e_shstrndx = 0;
    
    // 写入
    silver_buffer_write(output, &ehdr, sizeof(ehdr));
}

// 写入节头表
static void elf_write_section_headers(SilverBuffer *output,
                                      uint32_t num_sections,
                                      uint32_t shstrtab_offset,
                                      uint32_t shstrtab_size) {
    Elf64_Shdr shdr;
    
    // 节0: SHT_NULL
    memset(&shdr, 0, sizeof(shdr));
    silver_buffer_write(output, &shdr, sizeof(shdr));
    
    // .text节
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_name = 1;  // 字符串表中的偏移
    shdr.sh_type = SHT_PROGBITS;
    shdr.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    shdr.sh_addr = 0;
    shdr.sh_offset = 0;  // 稍后回填
    shdr.sh_size = 0;    // 稍后回填
    shdr.sh_link = 0;
    shdr.sh_info = 0;
    shdr.sh_addralign = 16;
    shdr.sh_entsize = 0;
    silver_buffer_write(output, &shdr, sizeof(shdr));
    
    // .data节
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_name = 7;  // ".data"
    shdr.sh_type = SHT_PROGBITS;
    shdr.sh_flags = SHF_ALLOC | SHF_WRITE;
    shdr.sh_offset = 0;
    shdr.sh_size = 0;
    shdr.sh_addralign = 8;
    silver_buffer_write(output, &shdr, sizeof(shdr));
    
    // .bss节
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_name = 13;  // ".bss"
    shdr.sh_type = SHT_NOBITS;
    shdr.sh_flags = SHF_ALLOC | SHF_WRITE;
    shdr.sh_offset = 0;
    shdr.sh_size = 0;
    shdr.sh_addralign = 8;
    silver_buffer_write(output, &shdr, sizeof(shdr));
    
    // .symtab节（符号表）
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_name = 18;  // ".symtab"
    shdr.sh_type = SHT_SYMTAB;
    shdr.sh_flags = 0;
    shdr.sh_offset = 0;
    shdr.sh_size = 0;
    shdr.sh_link = 0;  // 关联的字符串表节索引
    shdr.sh_info = 1;  // 第一个非局部符号的索引
    shdr.sh_addralign = 8;
    shdr.sh_entsize = sizeof(Elf64_Sym);
    silver_buffer_write(output, &shdr, sizeof(shdr));
    
    // .strtab节（字符串表）
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_name = 26;  // ".strtab"
    shdr.sh_type = SHT_STRTAB;
    shdr.sh_flags = 0;
    shdr.sh_offset = 0;
    shdr.sh_size = 0;
    shdr.sh_addralign = 1;
    silver_buffer_write(output, &shdr, sizeof(shdr));
    
    // .shstrtab节（节名称字符串表）
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_name = 34;  // ".shstrtab"
    shdr.sh_type = SHT_STRTAB;
    shdr.sh_flags = 0;
    shdr.sh_offset = shstrtab_offset;
    shdr.sh_size = shstrtab_size;
    shdr.sh_addralign = 1;
    silver_buffer_write(output, &shdr, sizeof(shdr));
}

// 创建节名称字符串表
static void elf_create_shstrtab(char *buffer, size_t *size) {
    // 索引0: 空字符串
    int offset = 0;
    buffer[offset++] = '\0';
    
    // 索引1: ".text"
    strcpy(buffer + offset, ".text");
    offset += 6;
    
    // 索引7: ".data"
    strcpy(buffer + offset, ".data");
    offset += 6;
    
    // 索引13: ".bss"
    strcpy(buffer + offset, ".bss");
    offset += 5;
    
    // 索引18: ".symtab"
    strcpy(buffer + offset, ".symtab");
    offset += 8;
    
    // 索引26: ".strtab"
    strcpy(buffer + offset, ".strtab");
    offset += 8;
    
    // 索引34: ".shstrtab"
    strcpy(buffer + offset, ".shstrtab");
    offset += 10;
    
    // 索引44: ".rela.text"
    strcpy(buffer + offset, ".rela.text");
    offset += 11;
    
    // 索引55: ".debug_info"
    strcpy(buffer + offset, ".debug_info");
    offset += 12;
    
    *size = offset;
}

// 方案：先构建字符串表，记录每个名称的偏移，然后创建符号表
static void elf_create_symbols_and_strtab(SilverBuffer *symtab, SilverBuffer *strtab, 
                                           IRModule *module) {
    // 空符号（索引0）
    Elf64_Sym sym;
    memset(&sym, 0, sizeof(sym));
    silver_buffer_write(symtab, &sym, sizeof(sym));
    
    // 字符串表：空字符串
    silver_buffer_write_u8(strtab, 0);
    
    // 为每个函数创建符号和字符串
    for (uint32_t i = 0; i < module->num_functions; i++) {
        const char *name = module->functions[i].name ? module->functions[i].name : "";
        
        // 记录当前字符串偏移
        uint32_t str_offset = (uint32_t)silver_buffer_length(strtab);
        silver_buffer_write(strtab, name, strlen(name) + 1);
        
        // 创建符号
        memset(&sym, 0, sizeof(sym));
        sym.st_name = str_offset;
        sym.st_info = (STB_GLOBAL << 4) | STT_FUNC;
        sym.st_shndx = 1;
        silver_buffer_write(symtab, &sym, sizeof(sym));
    }
    
    // 为每个全局变量创建符号和字符串
    for (uint32_t i = 0; i < module->num_globals; i++) {
        const char *name = module->globals[i].name ? module->globals[i].name : "";
        
        uint32_t str_offset = (uint32_t)silver_buffer_length(strtab);
        silver_buffer_write(strtab, name, strlen(name) + 1);
        
        memset(&sym, 0, sizeof(sym));
        sym.st_name = str_offset;
        sym.st_info = (STB_GLOBAL << 4) | STT_OBJECT;
        sym.st_shndx = 2;
        sym.st_size = module->globals[i].size;
        silver_buffer_write(symtab, &sym, sizeof(sym));
    }
}

// 主ELF写入函数
bool elf_write_object(IRModule *module,
                      const SilverCompileResult *result,
                      const SilverTarget *target,
                      SilverBuffer *output) {
    if (!module || !result || !target || !output) return false;
    
    // 计算各个段的大小
    uint32_t text_size = silver_buffer_length(result->code);
    uint32_t data_size = silver_buffer_length(result->data);
    uint32_t debug_size = result->debug_info ? 
        silver_buffer_length(result->debug_info) : 0;
    
    // 创建节名称字符串表
    char shstrtab[256];
    size_t shstrtab_size;
    elf_create_shstrtab(shstrtab, &shstrtab_size);
    
    // 创建符号表和字符串表（临时缓冲区）
    SilverBuffer *symtab = silver_buffer_create(4096);
    SilverBuffer *strtab = silver_buffer_create(4096);
    
    elf_create_symbols_and_strtab(symtab, strtab, module); 

    uint32_t symtab_size = silver_buffer_length(symtab);
    uint32_t strtab_size = silver_buffer_length(strtab);
    
    // 计算总大小
    uint32_t num_sections = 7;  // null, .text, .data, .bss, .symtab, .strtab, .shstrtab
    if (debug_size > 0) num_sections++;
    
    uint32_t header_size = sizeof(Elf64_Ehdr);
    uint32_t shdr_size = num_sections * sizeof(Elf64_Shdr);
    uint32_t total_size = header_size + text_size + data_size + 
                          symtab_size + strtab_size + shstrtab_size + 
                          debug_size + shdr_size;
    
    // 计算各段的偏移
    uint32_t offset = header_size;
    uint32_t text_offset = offset;
    offset += text_size;
    uint32_t data_offset = offset;
    offset += data_size;
    uint32_t symtab_offset = offset;
    offset += symtab_size;
    uint32_t strtab_offset = offset;
    offset += strtab_size;
    uint32_t shstrtab_offset = offset;
    offset += shstrtab_size;
    uint32_t debug_offset = offset;
    offset += debug_size;
    uint32_t shdr_offset = offset;
    
    // 写入ELF头
    elf_write_header(output, target);
    
    // 回填节头信息
    Elf64_Ehdr *ehdr = (Elf64_Ehdr *)silver_buffer_data(output);
    ehdr->e_shoff = shdr_offset;
    ehdr->e_shnum = num_sections;
    ehdr->e_shstrndx = num_sections - 1;  // 最后一个节是.shstrtab
    
    // 写入代码段
    silver_buffer_write(output, silver_buffer_data(result->code), text_size);
    
    // 写入数据段
    if (data_size > 0) {
        silver_buffer_write(output, silver_buffer_data(result->data), data_size);
    }
    
    // 写入符号表
    silver_buffer_write(output, silver_buffer_data(symtab), symtab_size);
    silver_buffer_destroy(symtab);
    
    // 写入字符串表
    silver_buffer_write(output, silver_buffer_data(strtab), strtab_size);
    silver_buffer_destroy(strtab);
    
    // 写入节名称字符串表
    silver_buffer_write(output, shstrtab, shstrtab_size);
    
    // 写入调试信息
    if (debug_size > 0) {
        silver_buffer_write(output, 
            silver_buffer_data(result->debug_info), debug_size);
    }
    
    // 写入节头表
    // .text
    Elf64_Shdr shdr;
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_name = 1;
    shdr.sh_type = SHT_PROGBITS;
    shdr.sh_flags = SHF_ALLOC | SHF_EXECINSTR;
    shdr.sh_offset = text_offset;
    shdr.sh_size = text_size;
    shdr.sh_addralign = 16;
    silver_buffer_write(output, &shdr, sizeof(shdr));
    
    // .data
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_name = 7;
    shdr.sh_type = SHT_PROGBITS;
    shdr.sh_flags = SHF_ALLOC | SHF_WRITE;
    shdr.sh_offset = data_offset;
    shdr.sh_size = data_size;
    shdr.sh_addralign = 8;
    silver_buffer_write(output, &shdr, sizeof(shdr));
    
    // .bss (空)
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_name = 13;
    shdr.sh_type = SHT_NOBITS;
    shdr.sh_flags = SHF_ALLOC | SHF_WRITE;
    shdr.sh_offset = 0;
    shdr.sh_size = 0;
    shdr.sh_addralign = 8;
    silver_buffer_write(output, &shdr, sizeof(shdr));
    
    // .symtab
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_name = 18;
    shdr.sh_type = SHT_SYMTAB;
    shdr.sh_offset = symtab_offset;
    shdr.sh_size = symtab_size;
    shdr.sh_link = 5;  // .strtab节索引
    shdr.sh_info = 1;
    shdr.sh_addralign = 8;
    shdr.sh_entsize = sizeof(Elf64_Sym);
    silver_buffer_write(output, &shdr, sizeof(shdr));
    
    // .strtab
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_name = 26;
    shdr.sh_type = SHT_STRTAB;
    shdr.sh_offset = strtab_offset;
    shdr.sh_size = strtab_size;
    shdr.sh_addralign = 1;
    silver_buffer_write(output, &shdr, sizeof(shdr));
    
    // .shstrtab
    memset(&shdr, 0, sizeof(shdr));
    shdr.sh_name = 34;
    shdr.sh_type = SHT_STRTAB;
    shdr.sh_offset = shstrtab_offset;
    shdr.sh_size = shstrtab_size;
    shdr.sh_addralign = 1;
    silver_buffer_write(output, &shdr, sizeof(shdr));
    
    // .debug_info (如果存在)
    if (debug_size > 0) {
        memset(&shdr, 0, sizeof(shdr));
        shdr.sh_name = 55;
        shdr.sh_type = SHT_PROGBITS;
        shdr.sh_offset = debug_offset;
        shdr.sh_size = debug_size;
        shdr.sh_addralign = 1;
        silver_buffer_write(output, &shdr, sizeof(shdr));
    }
    
    return true;
}