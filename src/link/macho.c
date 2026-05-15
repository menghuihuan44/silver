#include "silver/link/macho.h"
#include <string.h>

// 写入Mach-O头
static void macho_write_header(SilverBuffer *output, const SilverTarget *target) {
    MachOHeader64 header;
    memset(&header, 0, sizeof(header));
    
    header.magic = MH_MAGIC_64;
    
    switch (target->arch) {
        case SILVER_TARGET_X86_64:
            header.cputype = CPU_TYPE_X86_64;
            header.cpusubtype = 3;  // x86_64 all
            break;
        case SILVER_TARGET_ARM64:
            header.cputype = CPU_TYPE_ARM64;
            header.cpusubtype = 0;  // ARM64 all
            break;
        default:
            header.cputype = CPU_TYPE_X86_64;
            header.cpusubtype = 3;
            break;
    }
    
    header.filetype = MH_OBJECT;
    header.ncmds = 1;  // 至少一个段命令
    header.flags = MH_SUBSECTIONS_VIA_SYMBOLS;
    
    silver_buffer_write(output, &header, sizeof(header));
}

// 写入段命令和节
static void macho_write_segment(SilverBuffer *output,
                                 const char *segname,
                                 const char *sectname,
                                 const uint8_t *data, uint64_t size,
                                 uint64_t vmaddr, uint32_t section_flags) {
    MachOSegmentCommand64 seg;
    memset(&seg, 0, sizeof(seg));
    
    seg.cmd = LC_SEGMENT_64;
    seg.cmdsize = sizeof(MachOSegmentCommand64) + sizeof(MachOSection64);
    memcpy(seg.segname, segname, strlen(segname));
    seg.vmaddr = vmaddr;
    seg.vmsize = size;
    seg.fileoff = 0;  // 稍后计算
    seg.filesize = size;
    seg.maxprot = 7;  // rwx
    seg.initprot = 7;
    seg.nsects = 1;
    
    silver_buffer_write(output, &seg, sizeof(seg));
    
    // 节头
    MachOSection64 sect;
    memset(&sect, 0, sizeof(sect));
    strncpy(sect.sectname, sectname, 16);
    strncpy(sect.segname, segname, 16);
    sect.addr = vmaddr;
    sect.size = size;
    sect.offset = 0;  // 稍后计算
    sect.align = 4;   // 16字节对齐 (2^4)
    sect.flags = section_flags;
    
    silver_buffer_write(output, &sect, sizeof(sect));
}

// 计算节对齐的偏移
static uint32_t align_offset(uint32_t offset, uint32_t alignment) {
    return (offset + alignment - 1) & ~(alignment - 1);
}

bool macho_write_object(IRModule *module,
                        const SilverCompileResult *result,
                        const SilverTarget *target,
                        SilverBuffer *output) {
    if (!module || !result || !target || !output) return false;
    
    uint32_t text_size = silver_buffer_length(result->code);
    uint32_t data_size = silver_buffer_length(result->data);
    
    // 计算段命令大小
    uint32_t text_seg_cmd_size = sizeof(MachOSegmentCommand64) + sizeof(MachOSection64);
    uint32_t data_seg_cmd_size = sizeof(MachOSegmentCommand64) + sizeof(MachOSection64);
    uint32_t total_cmd_size = text_seg_cmd_size + data_seg_cmd_size;
    
    uint32_t header_size = sizeof(MachOHeader64) + total_cmd_size;
    uint32_t text_offset = header_size;
    uint32_t data_offset = text_offset + text_size;
    
    // 写入头
    MachOHeader64 *header = (MachOHeader64 *)silver_buffer_data(output);
    macho_write_header(output, target);
    
    // 回填命令数量
    header->ncmds = 2;  // 两个段命令
    header->sizeofcmds = total_cmd_size;
    
    // 写入__TEXT段
    macho_write_segment(output, "__TEXT", "__text",
                        silver_buffer_data(result->code), text_size,
                        0, S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS);
    
    // 写入__DATA段
    macho_write_segment(output, "__DATA", "__data",
                        silver_buffer_data(result->data), data_size,
                        0x1000, 0);
    
    // 回填文件偏移
    // __TEXT段命令中的节偏移
    size_t text_sect_offset = sizeof(MachOHeader64) + sizeof(MachOSegmentCommand64);
    MachOSection64 *text_sect = (MachOSection64 *)(silver_buffer_data(output) + text_sect_offset);
    text_sect->offset = text_offset;
    
    // __TEXT段命令的文件偏移
    MachOSegmentCommand64 *text_seg = (MachOSegmentCommand64 *)(silver_buffer_data(output) + sizeof(MachOHeader64));
    text_seg->fileoff = text_offset;
    
    // __DATA段命令中的节偏移
    size_t data_sect_offset = sizeof(MachOHeader64) + sizeof(MachOSegmentCommand64) + 
                               sizeof(MachOSection64) + sizeof(MachOSegmentCommand64);
    MachOSection64 *data_sect = (MachOSection64 *)(silver_buffer_data(output) + data_sect_offset);
    data_sect->offset = data_offset;
    
    // __DATA段命令的文件偏移
    MachOSegmentCommand64 *data_seg = (MachOSegmentCommand64 *)(silver_buffer_data(output) + 
        sizeof(MachOHeader64) + sizeof(MachOSegmentCommand64) + sizeof(MachOSection64));
    data_seg->fileoff = data_offset;
    
    // 写入实际数据
    silver_buffer_write(output, silver_buffer_data(result->code), text_size);
    silver_buffer_write(output, silver_buffer_data(result->data), data_size);
    
    return true;
}