#ifndef SILVER_DEBUG_DWARF_H
#define SILVER_DEBUG_DWARF_H

#include "silver/ir/ir_module.h"
#include "silver/support/buffer.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// DWARF版本
#define DWARF_VERSION 5

// DWARF标签
enum {
    DW_TAG_compile_unit = 0x11,
    DW_TAG_subprogram = 0x2E,
    DW_TAG_variable = 0x34,
    DW_TAG_formal_parameter = 0x05,
    DW_TAG_base_type = 0x24,
    DW_TAG_pointer_type = 0x0F,
    DW_TAG_const_type = 0x26,
    DW_TAG_volatile_type = 0x35,
    DW_TAG_structure_type = 0x13,
    DW_TAG_array_type = 0x01,
    DW_TAG_enumeration_type = 0x04,
    DW_TAG_subroutine_type = 0x15,
    DW_TAG_typedef = 0x16,
    DW_TAG_member = 0x0D,
    DW_TAG_inheritance = 0x1C,
    DW_TAG_lexical_block = 0x0B,
};

// DWARF属性
enum {
    DW_AT_sibling = 0x01,
    DW_AT_location = 0x02,
    DW_AT_name = 0x03,
    DW_AT_ordering = 0x09,
    DW_AT_byte_size = 0x0B,
    DW_AT_bit_offset = 0x0C,
    DW_AT_bit_size = 0x0D,
    DW_AT_stmt_list = 0x10,
    DW_AT_low_pc = 0x11,
    DW_AT_high_pc = 0x12,
    DW_AT_language = 0x13,
    DW_AT_discr = 0x15,
    DW_AT_discr_value = 0x16,
    DW_AT_visibility = 0x17,
    DW_AT_import = 0x18,
    DW_AT_string_length = 0x19,
    DW_AT_common_reference = 0x1A,
    DW_AT_comp_dir = 0x1B,
    DW_AT_const_value = 0x1C,
    DW_AT_containing_type = 0x1D,
    DW_AT_default_value = 0x1E,
    DW_AT_inline = 0x20,
    DW_AT_is_optional = 0x21,
    DW_AT_lower_bound = 0x22,
    DW_AT_producer = 0x25,
    DW_AT_prototyped = 0x27,
    DW_AT_return_addr = 0x2A,
    DW_AT_start_scope = 0x2C,
    DW_AT_bit_stride = 0x2E,
    DW_AT_upper_bound = 0x2F,
    DW_AT_abstract_origin = 0x31,
    DW_AT_accessibility = 0x32,
    DW_AT_address_class = 0x33,
    DW_AT_artificial = 0x34,
    DW_AT_base_types = 0x35,
    DW_AT_calling_convention = 0x36,
    DW_AT_count = 0x37,
    DW_AT_data_member_location = 0x38,
    DW_AT_decl_column = 0x39,
    DW_AT_decl_file = 0x3A,
    DW_AT_decl_line = 0x3B,
    DW_AT_declaration = 0x3C,
    DW_AT_discr_list = 0x3D,
    DW_AT_encoding = 0x3E,
    DW_AT_external = 0x3F,
    DW_AT_frame_base = 0x40,
    DW_AT_friend = 0x41,
    DW_AT_identifier_case = 0x42,
    DW_AT_macro_info = 0x43,
    DW_AT_namelist_item = 0x44,
    DW_AT_priority = 0x45,
    DW_AT_segment = 0x46,
    DW_AT_specification = 0x47,
    DW_AT_static_link = 0x48,
    DW_AT_type = 0x49,
    DW_AT_use_location = 0x4A,
    DW_AT_variable_parameter = 0x4B,
    DW_AT_virtuality = 0x4C,
    DW_AT_vtable_elem_location = 0x4D,
    DW_AT_allocated = 0x4E,
    DW_AT_associated = 0x4F,
    DW_AT_data_location = 0x50,
    DW_AT_byte_stride = 0x51,
    DW_AT_entry_pc = 0x52,
    DW_AT_use_UTF8 = 0x53,
    DW_AT_extension = 0x54,
    DW_AT_ranges = 0x55,
    DW_AT_trampoline = 0x56,
    DW_AT_call_column = 0x57,
    DW_AT_call_file = 0x58,
    DW_AT_call_line = 0x59,
    DW_AT_description = 0x5A,
    DW_AT_binary_scale = 0x5B,
    DW_AT_decimal_scale = 0x5C,
    DW_AT_small = 0x5D,
    DW_AT_decimal_sign = 0x5E,
    DW_AT_digit_count = 0x5F,
    DW_AT_picture_string = 0x60,
    DW_AT_mutable = 0x61,
    DW_AT_threads_scaled = 0x62,
    DW_AT_explicit = 0x63,
    DW_AT_object_pointer = 0x64,
    DW_AT_endianity = 0x65,
    DW_AT_elemental = 0x66,
    DW_AT_pure = 0x67,
    DW_AT_recursive = 0x68,
};

// DWARF形式编码
enum {
    DW_FORM_addr = 0x01,
    DW_FORM_block2 = 0x03,
    DW_FORM_block4 = 0x04,
    DW_FORM_data2 = 0x05,
    DW_FORM_data4 = 0x06,
    DW_FORM_data8 = 0x07,
    DW_FORM_string = 0x08,
    DW_FORM_block = 0x09,
    DW_FORM_block1 = 0x0A,
    DW_FORM_data1 = 0x0B,
    DW_FORM_flag = 0x0C,
    DW_FORM_sdata = 0x0D,
    DW_FORM_strp = 0x0E,
    DW_FORM_udata = 0x0F,
    DW_FORM_ref_addr = 0x10,
    DW_FORM_ref1 = 0x11,
    DW_FORM_ref2 = 0x12,
    DW_FORM_ref4 = 0x13,
    DW_FORM_ref8 = 0x14,
    DW_FORM_ref_udata = 0x15,
    DW_FORM_indirect = 0x16,
    DW_FORM_sec_offset = 0x17,
    DW_FORM_exprloc = 0x18,
    DW_FORM_flag_present = 0x19,
    DW_FORM_strx = 0x1A,
    DW_FORM_addrx = 0x1B,
    DW_FORM_ref_sup4 = 0x1C,
    DW_FORM_strp_sup = 0x1D,
    DW_FORM_data16 = 0x1E,
    DW_FORM_line_strp = 0x1F,
};

// 语言编码
enum {
    DW_LANG_C89 = 0x01,
    DW_LANG_C = 0x02,
    DW_LANG_Ada83 = 0x03,
    DW_LANG_C_plus_plus = 0x04,
    DW_LANG_Cobol74 = 0x05,
    DW_LANG_Cobol85 = 0x06,
    DW_LANG_Fortran77 = 0x07,
    DW_LANG_Fortran90 = 0x08,
    DW_LANG_Pascal83 = 0x09,
    DW_LANG_Modula2 = 0x0A,
    DW_LANG_Java = 0x0B,
    DW_LANG_C99 = 0x0C,
    DW_LANG_Ada95 = 0x0D,
    DW_LANG_Fortran95 = 0x0E,
    DW_LANG_PL1 = 0x0F,
    DW_LANG_ObjC = 0x10,
    DW_LANG_ObjC_plus_plus = 0x11,
    DW_LANG_UPC = 0x12,
    DW_LANG_D = 0x13,
    DW_LANG_Python = 0x14,
    DW_LANG_OpenCL = 0x15,
    DW_LANG_Go = 0x16,
    DW_LANG_Modula3 = 0x17,
    DW_LANG_Haskell = 0x18,
    DW_LANG_C_plus_plus_03 = 0x19,
    DW_LANG_C_plus_plus_11 = 0x1A,
    DW_LANG_OCaml = 0x1B,
    DW_LANG_Rust = 0x1C,
    DW_LANG_C11 = 0x1D,
    DW_LANG_Swift = 0x1E,
    DW_LANG_Julia = 0x1F,
    DW_LANG_Dylan = 0x20,
    DW_LANG_C_plus_plus_14 = 0x21,
    DW_LANG_Fortran03 = 0x22,
    DW_LANG_Fortran08 = 0x23,
    DW_LANG_RenderScript = 0x24,
};

// DWARF生成器
typedef struct {
    SilverBuffer *debug_info;     // .debug_info节
    SilverBuffer *debug_abbrev;   // .debug_abbrev节
    SilverBuffer *debug_line;     // .debug_line节
    SilverBuffer *debug_str;      // .debug_str节
    SilverBuffer *debug_aranges;  // .debug_aranges节
    
    uint32_t debug_info_offset;   // 当前.debug_info偏移
    uint32_t debug_abbrev_offset; // 当前.debug_abbrev偏移
    uint32_t abbrev_code;          // 缩写码计数器
    
    IRModule *module;              // 关联的IR模块
} DwarfGenerator;

// DWARF生成API
bool dwarf_generate_debug_info(IRModule *module, SilverBuffer *output);
void dwarf_generator_init(DwarfGenerator *gen, IRModule *module);
void dwarf_generator_destroy(DwarfGenerator *gen);

// 写入LEB128编码
void dwarf_write_uleb128(SilverBuffer *buffer, uint64_t value);
void dwarf_write_sleb128(SilverBuffer *buffer, int64_t value);

// 写入DWARF DIE
void dwarf_write_abbrev_entry(DwarfGenerator *gen, uint32_t tag, uint32_t children);
void dwarf_write_attr(DwarfGenerator *gen, uint32_t attr, uint32_t form);
void dwarf_write_die_header(DwarfGenerator *gen, uint32_t abbrev_code);
void dwarf_end_children(DwarfGenerator *gen);

// 写入调试信息
void dwarf_write_compile_unit(DwarfGenerator *gen, const char *producer);
void dwarf_write_subprogram(DwarfGenerator *gen, const char *name,
                            uint64_t low_pc, uint64_t high_pc);
void dwarf_write_variable(DwarfGenerator *gen, const char *name,
                          uint32_t type_ref, uint64_t location);

#ifdef __cplusplus
}
#endif

#endif // SILVER_DEBUG_DWARF_H