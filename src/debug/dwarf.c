#include "silver/debug/dwarf.h"
#include <string.h>
#include <stdio.h>

static const char SILVER_PRODUCER[] = "Silver Compiler Backend v0.1.0";

void dwarf_write_uleb128(SilverBuffer *buffer, uint64_t value) {
    uint8_t byte;
    do {
        byte = value & 0x7F;
        value >>= 7;
        if (value != 0) {
            byte |= 0x80;
        }
        silver_buffer_write_u8(buffer, byte);
    } while (value != 0);
}

void dwarf_write_sleb128(SilverBuffer *buffer, int64_t value) {
    uint8_t byte;
    bool more = true;
    while (more) {
        byte = value & 0x7F;
        value >>= 7;
        if ((value == 0 && (byte & 0x40) == 0) || 
            (value == -1 && (byte & 0x40) != 0)) {
            more = false;
        } else {
            byte |= 0x80;
        }
        silver_buffer_write_u8(buffer, byte);
    }
}

void dwarf_generator_init(DwarfGenerator *gen, IRModule *module) {
    if (!gen) return;
    
    memset(gen, 0, sizeof(*gen));
    
    gen->debug_info = silver_buffer_create(4096);
    gen->debug_abbrev = silver_buffer_create(1024);
    gen->debug_line = silver_buffer_create(2048);
    gen->debug_str = silver_buffer_create(2048);
    gen->debug_aranges = silver_buffer_create(512);
    
    gen->module = module;
    gen->abbrev_code = 1;
    
    // 写入.debug_str的空字符串
    silver_buffer_write_u8(gen->debug_str, 0);
}

void dwarf_generator_destroy(DwarfGenerator *gen) {
    if (!gen) return;
    
    silver_buffer_destroy(gen->debug_info);
    silver_buffer_destroy(gen->debug_abbrev);
    silver_buffer_destroy(gen->debug_line);
    silver_buffer_destroy(gen->debug_str);
    silver_buffer_destroy(gen->debug_aranges);
}

void dwarf_write_abbrev_entry(DwarfGenerator *gen, uint32_t tag, uint32_t children) {
    dwarf_write_uleb128(gen->debug_abbrev, gen->abbrev_code);
    dwarf_write_uleb128(gen->debug_abbrev, tag);
    dwarf_write_uleb128(gen->debug_abbrev, children);
    gen->abbrev_code++;
}

void dwarf_write_attr(DwarfGenerator *gen, uint32_t attr, uint32_t form) {
    dwarf_write_uleb128(gen->debug_abbrev, attr);
    dwarf_write_uleb128(gen->debug_abbrev, form);
}

void dwarf_end_children(DwarfGenerator *gen) {
    dwarf_write_uleb128(gen->debug_abbrev, 0);
    dwarf_write_uleb128(gen->debug_abbrev, 0);
}

void dwarf_write_die_header(DwarfGenerator *gen, uint32_t abbrev_code) {
    dwarf_write_uleb128(gen->debug_info, abbrev_code);
}

void dwarf_write_compile_unit(DwarfGenerator *gen, const char *producer) {
    // 定义编译单元缩写
    uint32_t cu_abbrev = gen->abbrev_code;
    dwarf_write_abbrev_entry(gen, DW_TAG_compile_unit, 1);  // 有子节点
    dwarf_write_attr(gen, DW_AT_producer, DW_FORM_strp);
    dwarf_write_attr(gen, DW_AT_language, DW_FORM_data2);
    dwarf_write_attr(gen, DW_AT_name, DW_FORM_strp);
    dwarf_write_attr(gen, DW_AT_comp_dir, DW_FORM_strp);
    dwarf_write_attr(gen, DW_AT_low_pc, DW_FORM_addr);
    dwarf_write_attr(gen, DW_AT_high_pc, DW_FORM_addr);
    dwarf_write_attr(gen, DW_AT_stmt_list, DW_FORM_sec_offset);
    dwarf_end_children(gen);
    
    // 写入编译单元DIE
    size_t unit_start = silver_buffer_length(gen->debug_info);
    
    dwarf_write_die_header(gen, cu_abbrev);
    
    // 写入属性值
    size_t producer_offset = silver_buffer_length(gen->debug_str);
    silver_buffer_write(gen->debug_str, producer, strlen(producer) + 1);
    dwarf_write_uleb128(gen->debug_info, producer_offset);
    
    silver_buffer_write_u16_le(gen->debug_info, DW_LANG_C11);
    
    // 名称
    size_t name_offset = silver_buffer_length(gen->debug_str);
    const char *name = gen->module->module_name ? gen->module->module_name : "unknown";
    silver_buffer_write(gen->debug_str, name, strlen(name) + 1);
    dwarf_write_uleb128(gen->debug_info, name_offset);
    
    // 编译目录
    size_t dir_offset = silver_buffer_length(gen->debug_str);
    silver_buffer_write(gen->debug_str, ".", 2);
    dwarf_write_uleb128(gen->debug_info, dir_offset);
    
    // 地址范围（占位符）
    silver_buffer_write_u64_le(gen->debug_info, 0);  // low_pc
    silver_buffer_write_u64_le(gen->debug_info, 0);  // high_pc
    
    // 行号表偏移（占位符）
    silver_buffer_write_u32_le(gen->debug_info, 0);
}

void dwarf_write_subprogram(DwarfGenerator *gen, const char *name,
                            uint64_t low_pc, uint64_t high_pc) {
    // 定义子程序缩写
    uint32_t sp_abbrev = gen->abbrev_code;
    dwarf_write_abbrev_entry(gen, DW_TAG_subprogram, 1);  // 有子节点
    dwarf_write_attr(gen, DW_AT_name, DW_FORM_strp);
    dwarf_write_attr(gen, DW_AT_low_pc, DW_FORM_addr);
    dwarf_write_attr(gen, DW_AT_high_pc, DW_FORM_addr);
    dwarf_write_attr(gen, DW_AT_frame_base, DW_FORM_exprloc);
    dwarf_end_children(gen);
    
    // 写入子程序DIE
    dwarf_write_die_header(gen, sp_abbrev);
    
    // 名称
    size_t name_offset = silver_buffer_length(gen->debug_str);
    silver_buffer_write(gen->debug_str, name, strlen(name) + 1);
    dwarf_write_uleb128(gen->debug_info, name_offset);
    
    // 地址
    silver_buffer_write_u64_le(gen->debug_info, low_pc);
    silver_buffer_write_u64_le(gen->debug_info, high_pc);
    
    // 帧基址表达式
    // DW_OP_call_frame_cfa (0x9C)
    silver_buffer_write_u8(gen->debug_info, 1);  // 表达式长度
    silver_buffer_write_u8(gen->debug_info, 0x9C);  // DW_OP_call_frame_cfa
}

void dwarf_write_variable(DwarfGenerator *gen, const char *name,
                          uint32_t type_ref, uint64_t location) {
    // 定义变量缩写
    uint32_t var_abbrev = gen->abbrev_code;
    dwarf_write_abbrev_entry(gen, DW_TAG_variable, 0);  // 无子节点
    dwarf_write_attr(gen, DW_AT_name, DW_FORM_strp);
    dwarf_write_attr(gen, DW_AT_type, DW_FORM_ref4);
    dwarf_write_attr(gen, DW_AT_location, DW_FORM_exprloc);
    dwarf_end_children(gen);
    
    // 写入变量DIE
    dwarf_write_die_header(gen, var_abbrev);
    
    // 名称
    size_t name_offset = silver_buffer_length(gen->debug_str);
    silver_buffer_write(gen->debug_str, name, strlen(name) + 1);
    dwarf_write_uleb128(gen->debug_info, name_offset);
    
    // 类型引用
    silver_buffer_write_u32_le(gen->debug_info, type_ref);
    
    // 位置表达式
    // DW_OP_fbreg <offset>
    silver_buffer_write_u8(gen->debug_info, 2);  // 表达式长度
    silver_buffer_write_u8(gen->debug_info, 0x91);  // DW_OP_fbreg
    dwarf_write_sleb128(gen->debug_info, (int64_t)location);
}

// 生成完整的DWARF调试信息
bool dwarf_generate_debug_info(IRModule *module, SilverBuffer *output) {
    if (!module || !output) return false;
    
    DwarfGenerator gen;
    dwarf_generator_init(&gen, module);
    
    // 写入编译单元
    dwarf_write_compile_unit(&gen, SILVER_PRODUCER);
    
    // 定义基类型缩写
    uint32_t base_type_abbrev = gen.abbrev_code;
    dwarf_write_abbrev_entry(&gen, DW_TAG_base_type, 0);
    dwarf_write_attr(&gen, DW_AT_name, DW_FORM_strp);
    dwarf_write_attr(&gen, DW_AT_encoding, DW_FORM_data1);
    dwarf_write_attr(&gen, DW_AT_byte_size, DW_FORM_data1);
    dwarf_end_children(&gen);
    
    // 写入常见基类型
    static struct {
        const char *name;
        uint8_t encoding;
        uint8_t byte_size;
    } base_types[] = {
        {"int", 5, 4},      // signed
        {"unsigned int", 7, 4},  // unsigned
        {"char", 6, 1},     // signed char
        {"unsigned char", 8, 1}, // unsigned char
        {"short", 5, 2},
        {"unsigned short", 7, 2},
        {"long", 5, 8},
        {"unsigned long", 7, 8},
        {"float", 4, 4},    // float
        {"double", 4, 8},   // float
        {"long double", 4, 16},
        {"bool", 2, 1},     // boolean
        {NULL, 0, 0}
    };
    
    uint32_t base_type_refs[16];
    int num_base_types = 0;
    
    for (int i = 0; base_types[i].name; i++) {
        base_type_refs[num_base_types] = (uint32_t)silver_buffer_length(gen.debug_info);
        
        dwarf_write_die_header(&gen, base_type_abbrev);
        size_t name_off = silver_buffer_length(gen.debug_str);
        silver_buffer_write(gen.debug_str, base_types[i].name, strlen(base_types[i].name) + 1);
        dwarf_write_uleb128(gen.debug_info, name_off);
        silver_buffer_write_u8(gen.debug_info, base_types[i].encoding);
        silver_buffer_write_u8(gen.debug_info, base_types[i].byte_size);
        
        num_base_types++;
    }
    
    // 为每个函数生成调试信息
    for (uint32_t i = 0; i < module->num_functions; i++) {
        IRFunction *func = &module->functions[i];
        
        const char *func_name = func->name ? func->name : "unnamed";
        dwarf_write_subprogram(&gen, func_name, 0, 0x100);
        
        // 为参数生成变量DIE
        for (uint32_t j = 0; j < func->num_args; j++) {
            char arg_name[32];
            snprintf(arg_name, sizeof(arg_name), "arg%u", j);
            dwarf_write_variable(&gen, arg_name, base_type_refs[0], j * 8);
        }
        
        // 结束子程序
        dwarf_write_uleb128(gen.debug_info, 0);  // null terminator
    }
    
    // 结束编译单元
    dwarf_write_uleb128(gen.debug_info, 0);  // null terminator
    
    // 合并所有调试信息到输出缓冲区
    // 简化的DWARF包装
    const char *section_names[] = {
        ".debug_info",
        ".debug_abbrev",
        ".debug_str",
        ".debug_line",
        ".debug_aranges",
    };
    
    SilverBuffer *sections[] = {
        gen.debug_info,
        gen.debug_abbrev,
        gen.debug_str,
        gen.debug_line,
        gen.debug_aranges,
    };
    
    // 写入段数量
    silver_buffer_write_u32_le(output, 5);
    
    // 写入每个段
    for (int i = 0; i < 5; i++) {
        // 段名称
        const char *name = section_names[i];
        silver_buffer_write(output, name, strlen(name) + 1);
        
        // 段大小和数据
        uint32_t size = (uint32_t)silver_buffer_length(sections[i]);
        silver_buffer_write_u32_le(output, size);
        silver_buffer_write(output, silver_buffer_data(sections[i]), size);
    }
    
    dwarf_generator_destroy(&gen);
    return true;
}