#include "silver/link/pe.h"
#include <string.h>
#include <time.h>

// 写入COFF头
static void pe_write_coff_header(SilverBuffer *output, const SilverTarget *target) {
    COFFHeader header;
    memset(&header, 0, sizeof(header));
    
    // 设置机器类型
    switch (target->arch) {
        case SILVER_TARGET_X86_64:
            header.Machine = IMAGE_FILE_MACHINE_AMD64;
            break;
        case SILVER_TARGET_ARM64:
            header.Machine = IMAGE_FILE_MACHINE_ARM64;
            break;
        case SILVER_TARGET_RISCV64:
            header.Machine = IMAGE_FILE_MACHINE_RISCV64;
            break;
        default:
            header.Machine = IMAGE_FILE_MACHINE_AMD64;
            break;
    }
    
    header.NumberOfSections = 3;  // .text, .data, .bss
    header.TimeDateStamp = (uint32_t)time(NULL);
    header.PointerToSymbolTable = 0;
    header.NumberOfSymbols = 0;
    header.SizeOfOptionalHeader = sizeof(PEOptionalHeader64);
    header.Characteristics = IMAGE_FILE_RELOCS_STRIPPED;
    
    silver_buffer_write(output, &header, sizeof(header));
}

// 写入PE可选头
static void pe_write_optional_header(SilverBuffer *output) {
    PEOptionalHeader64 opt;
    memset(&opt, 0, sizeof(opt));
    
    opt.Magic = 0x20B;  // PE32+
    opt.MajorLinkerVersion = 1;
    opt.MinorLinkerVersion = 0;
    opt.SectionAlignment = 4096;
    opt.FileAlignment = 512;
    opt.MajorOperatingSystemVersion = 6;
    opt.MinorOperatingSystemVersion = 0;
    opt.MajorSubsystemVersion = 6;
    opt.MinorSubsystemVersion = 0;
    opt.SizeOfHeaders = 512;  // 稍后回填
    opt.Subsystem = 3;  // Windows Console
    opt.SizeOfStackReserve = 0x100000;
    opt.SizeOfStackCommit = 0x1000;
    opt.SizeOfHeapReserve = 0x100000;
    opt.SizeOfHeapCommit = 0x1000;
    opt.NumberOfRvaAndSizes = 16;
    
    silver_buffer_write(output, &opt, sizeof(opt));
}

// 写入节头
static void pe_write_section_headers(SilverBuffer *output) {
    PESectionHeader section;
    
    // .text节
    memset(&section, 0, sizeof(section));
    memcpy(section.Name, ".text", 5);
    section.Characteristics = IMAGE_SCN_CNT_CODE | 
                               IMAGE_SCN_MEM_EXECUTE | 
                               IMAGE_SCN_MEM_READ;
    silver_buffer_write(output, &section, sizeof(section));
    
    // .data节
    memset(&section, 0, sizeof(section));
    memcpy(section.Name, ".data", 5);
    section.Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA |
                               IMAGE_SCN_MEM_READ | 
                               IMAGE_SCN_MEM_WRITE;
    silver_buffer_write(output, &section, sizeof(section));
    
    // .bss节
    memset(&section, 0, sizeof(section));
    memcpy(section.Name, ".bss", 4);
    section.Characteristics = IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE;
    silver_buffer_write(output, &section, sizeof(section));
}

bool pe_write_object(IRModule *module,
                     const SilverCompileResult *result,
                     const SilverTarget *target,
                     SilverBuffer *output) {
    if (!module || !result || !target || !output) return false;
    
    uint32_t text_size = silver_buffer_length(result->code);
    uint32_t data_size = silver_buffer_length(result->data);
    
    // 计算对齐
    uint32_t file_alignment = 512;
    uint32_t section_alignment = 4096;
    
    // 计算各节的文件偏移
    uint32_t headers_size = sizeof(COFFHeader) + sizeof(PEOptionalHeader64) + 
                            3 * sizeof(PESectionHeader);
    headers_size = (headers_size + file_alignment - 1) & ~(file_alignment - 1);
    
    uint32_t text_offset = headers_size;
    uint32_t text_file_size = (text_size + file_alignment - 1) & ~(file_alignment - 1);
    
    uint32_t data_offset = text_offset + text_file_size;
    uint32_t data_file_size = (data_size + file_alignment - 1) & ~(file_alignment - 1);
    
    // 写入头
    pe_write_coff_header(output, target);
    pe_write_optional_header(output);
    pe_write_section_headers(output);
    
    // 对齐到文件对齐边界
    while (silver_buffer_length(output) < headers_size) {
        silver_buffer_write_u8(output, 0);
    }
    
    // 回填节头信息
    size_t section_offset = sizeof(COFFHeader) + sizeof(PEOptionalHeader64);
    
    // .text节
    PESectionHeader text_sec;
    text_sec.VirtualSize = text_size;
    text_sec.VirtualAddress = 0x1000;
    text_sec.SizeOfRawData = text_file_size;
    text_sec.PointerToRawData = text_offset;
    silver_buffer_write_at(output, section_offset, &text_sec, sizeof(text_sec));
    
    // .data节
    PESectionHeader data_sec;
    data_sec.VirtualSize = data_size;
    data_sec.VirtualAddress = 0x2000;
    data_sec.SizeOfRawData = data_file_size;
    data_sec.PointerToRawData = data_offset;
    silver_buffer_write_at(output, section_offset + sizeof(PESectionHeader), 
                          &data_sec, sizeof(data_sec));
    
    // 写入代码
    silver_buffer_write(output, silver_buffer_data(result->code), text_size);
    while (silver_buffer_length(output) < text_offset + text_file_size) {
        silver_buffer_write_u8(output, 0);
    }
    
    // 写入数据
    silver_buffer_write(output, silver_buffer_data(result->data), data_size);
    while (silver_buffer_length(output) < data_offset + data_file_size) {
        silver_buffer_write_u8(output, 0);
    }
    
    return true;
}