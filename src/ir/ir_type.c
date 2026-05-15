#include "silver/ir/ir_type.h"
#include "silver/support/error.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define INITIAL_TYPE_CAPACITY 128
#define TYPE_GROWTH_FACTOR 2

void ir_type_table_init(IRTypeTable *table, Arena *arena) {
    if (!table) return;
    
    table->num_types = 0;
    table->capacity = 128;
    table->arena = arena;
    
    if (arena) {
        table->types = (IRType *)arena_calloc(arena, 
            table->capacity * sizeof(IRType));
    } else {
        table->types = (IRType *)calloc(table->capacity, sizeof(IRType));
    }
    
    if (!table->types) {
        table->capacity = 0;
        return;
    }
    
    // ============================================================
    // ✅ 预创建所有基本类型
    // ============================================================
    
    // void (ID=0)
    IRTypeId void_id = table->num_types++;
    table->types[void_id].kind = IR_TYPE_VOID;
    table->types[void_id].size = 0;
    table->types[void_id].alignment = 1;
    table->void_type = void_id;
    
    // i1 (ID=1)
    IRTypeId i1_id = table->num_types++;
    table->types[i1_id].kind = IR_TYPE_INT;
    table->types[i1_id].size = 1;
    table->types[i1_id].alignment = 1;
    table->types[i1_id].int_type.width = IR_INT_1;
    table->types[i1_id].int_type.is_signed = false;
    table->i1_type = i1_id;
    
    // i8 (ID=2)
    IRTypeId i8_id = table->num_types++;
    table->types[i8_id].kind = IR_TYPE_INT;
    table->types[i8_id].size = 1;
    table->types[i8_id].alignment = 1;
    table->types[i8_id].int_type.width = IR_INT_8;
    table->types[i8_id].int_type.is_signed = true;
    table->i8_type = i8_id;
    
    // i16 (ID=3)
    IRTypeId i16_id = table->num_types++;
    table->types[i16_id].kind = IR_TYPE_INT;
    table->types[i16_id].size = 2;
    table->types[i16_id].alignment = 2;
    table->types[i16_id].int_type.width = IR_INT_16;
    table->types[i16_id].int_type.is_signed = true;
    table->i16_type = i16_id;
    
    // i32 (ID=4)
    IRTypeId i32_id = table->num_types++;
    table->types[i32_id].kind = IR_TYPE_INT;
    table->types[i32_id].size = 4;
    table->types[i32_id].alignment = 4;
    table->types[i32_id].int_type.width = IR_INT_32;
    table->types[i32_id].int_type.is_signed = true;
    table->i32_type = i32_id;
    
    // i64 (ID=5)
    IRTypeId i64_id = table->num_types++;
    table->types[i64_id].kind = IR_TYPE_INT;
    table->types[i64_id].size = 8;
    table->types[i64_id].alignment = 8;
    table->types[i64_id].int_type.width = IR_INT_64;
    table->types[i64_id].int_type.is_signed = true;
    table->i64_type = i64_id;
    
    // u8 (ID=6)
    IRTypeId u8_id = table->num_types++;
    table->types[u8_id].kind = IR_TYPE_INT;
    table->types[u8_id].size = 1;
    table->types[u8_id].alignment = 1;
    table->types[u8_id].int_type.width = IR_INT_8;
    table->types[u8_id].int_type.is_signed = false;
    table->u8_type = u8_id;
    
    // u16 (ID=7)
    IRTypeId u16_id = table->num_types++;
    table->types[u16_id].kind = IR_TYPE_INT;
    table->types[u16_id].size = 2;
    table->types[u16_id].alignment = 2;
    table->types[u16_id].int_type.width = IR_INT_16;
    table->types[u16_id].int_type.is_signed = false;
    table->u16_type = u16_id;
    
    // u32 (ID=8)
    IRTypeId u32_id = table->num_types++;
    table->types[u32_id].kind = IR_TYPE_INT;
    table->types[u32_id].size = 4;
    table->types[u32_id].alignment = 4;
    table->types[u32_id].int_type.width = IR_INT_32;
    table->types[u32_id].int_type.is_signed = false;
    table->u32_type = u32_id;
    
    // u64 (ID=9)
    IRTypeId u64_id = table->num_types++;
    table->types[u64_id].kind = IR_TYPE_INT;
    table->types[u64_id].size = 8;
    table->types[u64_id].alignment = 8;
    table->types[u64_id].int_type.width = IR_INT_64;
    table->types[u64_id].int_type.is_signed = false;
    table->u64_type = u64_id;
    
    // f32 (ID=10)
    IRTypeId f32_id = table->num_types++;
    table->types[f32_id].kind = IR_TYPE_FLOAT;
    table->types[f32_id].size = 4;
    table->types[f32_id].alignment = 4;
    table->types[f32_id].float_type.width = IR_FLOAT_32;
    table->f32_type = f32_id;
    
    // f64 (ID=11)
    IRTypeId f64_id = table->num_types++;
    table->types[f64_id].kind = IR_TYPE_FLOAT;
    table->types[f64_id].size = 8;
    table->types[f64_id].alignment = 8;
    table->types[f64_id].float_type.width = IR_FLOAT_64;
    table->f64_type = f64_id;
    
    // ptr (ID=12) - 通用指针，指向void
    IRTypeId ptr_id = table->num_types++;
    table->types[ptr_id].kind = IR_TYPE_PTR;
    table->types[ptr_id].size = 8;
    table->types[ptr_id].alignment = 8;
    table->types[ptr_id].ptr_type.pointee = void_id;
    table->ptr_type = ptr_id;
    
    // label (ID=13)
    IRTypeId label_id = table->num_types++;
    table->types[label_id].kind = IR_TYPE_LABEL;
    table->types[label_id].size = 0;
    table->types[label_id].alignment = 1;
    table->label_type = label_id;
    
    // token (ID=14)
    IRTypeId token_id = table->num_types++;
    table->types[token_id].kind = IR_TYPE_TOKEN;
    table->types[token_id].size = 0;
    table->types[token_id].alignment = 1;
    table->token_type = token_id;
}

// 获取类型指针（通过ID）
IRType *ir_type_get(IRTypeTable *table, IRTypeId id) {
    if (!table || id >= table->num_types) return NULL;
    return &table->types[id];
}

static bool ir_type_table_ensure_capacity(IRTypeTable *table, uint32_t needed, Arena *arena) {
    if (!table) return false;
    if (needed <= table->capacity) return true;
    
    uint32_t new_capacity = table->capacity * 2;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }
    
    IRType *new_types;
    if (arena) {
        new_types = (IRType *)arena_calloc(arena, new_capacity * sizeof(IRType));
        if (!new_types) return false;
        memcpy(new_types, table->types, table->num_types * sizeof(IRType));
    } else {
        new_types = (IRType *)realloc(table->types, new_capacity * sizeof(IRType));
        if (!new_types) return false;
        memset(new_types + table->capacity, 0, 
               (new_capacity - table->capacity) * sizeof(IRType));
    }
    
    table->types = new_types;
    table->capacity = new_capacity;
    return true;
}

// 注意：这个函数需要访问IRModule的Arena
// 实际实现中，类型表需要与Arena关联
// 这里先提供核心逻辑，后续会与IRModule集成

static uint32_t ir_type_calculate_size(const IRTypeTable *table, IRTypeId id) {
    const IRType *type = &table->types[id];
    
    switch (type->kind) {
        case IR_TYPE_VOID:
            return 0;
        
        case IR_TYPE_INT:
            return type->int_type.width / 8;
        
        case IR_TYPE_FLOAT:
            return type->float_type.width / 8;
        
        case IR_TYPE_PTR:
            // 指针大小取决于目标平台，这里默认64位
            return 8;
        
        case IR_TYPE_ARRAY:
            if (type->array_type.count == 0) {
                return 0;  // 可变长度数组，大小未知
            }
            return ir_type_calculate_size(table, type->array_type.elem_type) 
                   * type->array_type.count;
        
        case IR_TYPE_STRUCT: {
            uint32_t size = 0;
            uint32_t max_align = 1;
            
            for (uint32_t i = 0; i < type->struct_type.num_fields; i++) {
                IRTypeId field_type = type->struct_type.field_types[i];
                uint32_t field_size = ir_type_calculate_size(table, field_type);
                uint32_t field_align = ir_type_alignment(table, field_type);
                
                // 对齐到字段要求
                if (field_align > 0) {
                    size = (size + field_align - 1) & ~(field_align - 1);
                }
                
                size += field_size;
                
                if (field_align > max_align) {
                    max_align = field_align;
                }
            }
            
            // 整体对齐到最大字段对齐
            if (max_align > 1) {
                size = (size + max_align - 1) & ~(max_align - 1);
            }
            
            return size;
        }
        
        case IR_TYPE_FUNC:
        case IR_TYPE_LABEL:
        case IR_TYPE_TOKEN:
            return 0;  // 这些类型不能直接存储
        
        default:
            return 0;
    }
}

uint32_t ir_type_alignment(const IRTypeTable *table, IRTypeId id) {
    if (id >= table->num_types) return 1;
    
    const IRType *type = &table->types[id];
    
    switch (type->kind) {
        case IR_TYPE_VOID:
            return 1;
        
        case IR_TYPE_INT:
            return type->int_type.width / 8;
        
        case IR_TYPE_FLOAT:
            return type->float_type.width / 8;
        
        case IR_TYPE_PTR:
            return 8;  // 64位指针
        
        case IR_TYPE_ARRAY:
            return ir_type_alignment(table, type->array_type.elem_type);
        
        case IR_TYPE_STRUCT: {
            uint32_t max_align = 1;
            for (uint32_t i = 0; i < type->struct_type.num_fields; i++) {
                uint32_t align = ir_type_alignment(table, type->struct_type.field_types[i]);
                if (align > max_align) {
                    max_align = align;
                }
            }
            return max_align;
        }
        
        case IR_TYPE_FUNC:
        case IR_TYPE_LABEL:
        case IR_TYPE_TOKEN:
            return 1;
        
        default:
            return 1;
    }
}

// 创建指针类型
IRTypeId ir_type_create_ptr(IRTypeTable *table, IRTypeId pointee_type) {
    if (!table) return IR_TYPE_ID_INVALID;
    
    if (!ir_type_table_ensure_capacity(table, table->num_types + 1, table->arena)) {
        return IR_TYPE_ID_INVALID;
    } 
    
    IRTypeId id = table->num_types++;
    IRType *type = &table->types[id];
    memset(type, 0, sizeof(IRType));
    
    type->kind = IR_TYPE_PTR;
    type->size = 8;   // 64位指针默认大小
    type->alignment = 8;
    type->ptr_type.pointee = pointee_type;
    
    return id;
}

// 创建数组类型
IRTypeId ir_type_create_array(IRTypeTable *table, IRTypeId elem_type, uint32_t count) {
    if (!table) return IR_TYPE_ID_INVALID;
    
    if (!ir_type_table_ensure_capacity(table, table->num_types + 1, table->arena)) {
        return IR_TYPE_ID_INVALID;
    } 
    
    IRTypeId id = table->num_types++;
    IRType *type = &table->types[id];
    memset(type, 0, sizeof(IRType));
    
    type->kind = IR_TYPE_ARRAY;
    type->array_type.elem_type = elem_type;
    type->array_type.count = count;
    
    // 计算大小
    if (elem_type < table->num_types) {
        uint32_t elem_size = table->types[elem_type].size;
        if (elem_size > 0) {
            type->size = elem_size * count;
        }
    }
    type->alignment = ir_type_alignment(table, elem_type);
    
    return id;
}

// 创建结构体类型
IRTypeId ir_type_create_struct(IRTypeTable *table, const char *name,
                                uint32_t num_fields, IRTypeId *field_types) {
    if (!table) return IR_TYPE_ID_INVALID;
    
    if (!ir_type_table_ensure_capacity(table, table->num_types + 1, table->arena)) {
        return IR_TYPE_ID_INVALID;
    } 
    
    IRTypeId id = table->num_types++;
    IRType *type = &table->types[id];
    memset(type, 0, sizeof(IRType));
    
    type->kind = IR_TYPE_STRUCT;
    type->struct_type.name = name;
    type->struct_type.num_fields = num_fields;
    type->struct_type.field_types = field_types;  // 假设在Arena中分配
    type->struct_type.field_offsets = NULL;  // 稍后计算
    
    // 计算大小和对齐
    uint32_t offset = 0;
    uint32_t max_align = 1;
    
    if (field_types && num_fields > 0) {
        type->struct_type.field_offsets = 
            (uint32_t *)calloc(num_fields, sizeof(uint32_t));
        
        for (uint32_t i = 0; i < num_fields; i++) {
            uint32_t field_align = ir_type_alignment(table, field_types[i]);
            uint32_t field_size = (field_types[i] < table->num_types) ? 
                                   table->types[field_types[i]].size : 0;
            
            if (field_align > max_align) max_align = field_align;
            
            // 对齐
            offset = (offset + field_align - 1) & ~(field_align - 1);
            type->struct_type.field_offsets[i] = offset;
            offset += field_size;
        }
        
        // 整体对齐
        offset = (offset + max_align - 1) & ~(max_align - 1);
    }
    
    type->size = offset;
    type->alignment = max_align;
    
    return id;
}

// 创建函数类型
IRTypeId ir_type_create_func(IRTypeTable *table, IRTypeId return_type,
                              uint32_t num_params, IRTypeId *param_types,
                              bool is_variadic) {
    if (!table) return IR_TYPE_ID_INVALID;
    
    if (!ir_type_table_ensure_capacity(table, table->num_types + 1, table->arena)) {
        return IR_TYPE_ID_INVALID;
    } 
    
    IRTypeId id = table->num_types++;
    IRType *type = &table->types[id];
    memset(type, 0, sizeof(IRType));
    
    type->kind = IR_TYPE_FUNC;
    type->size = 0;  // 函数类型没有大小
    type->alignment = 1;
    type->func_type.return_type = return_type;
    type->func_type.num_params = num_params;
    type->func_type.param_types = param_types;
    type->func_type.is_variadic = is_variadic;
    
    return id;
}

// 创建整数类型
IRTypeId ir_type_create_int(IRTypeTable *table, IRIntWidth width, bool is_signed) {
    if (!table) return IR_TYPE_ID_INVALID;
    
    if (!ir_type_table_ensure_capacity(table, table->num_types + 1, table->arena)) {
        return IR_TYPE_ID_INVALID;
    } 
    
    IRTypeId id = table->num_types++;
    IRType *type = &table->types[id];
    memset(type, 0, sizeof(IRType));
    
    type->kind = IR_TYPE_INT;
    type->size = width / 8;
    type->alignment = type->size;
    type->int_type.width = width;
    type->int_type.is_signed = is_signed;
    
    return id;
}

// 创建浮点类型
IRTypeId ir_type_create_float(IRTypeTable *table, IRFloatWidth width) {
    if (!table) return IR_TYPE_ID_INVALID;
    
    if (!ir_type_table_ensure_capacity(table, table->num_types + 1, table->arena)) {
        return IR_TYPE_ID_INVALID;
    } 
    
    IRTypeId id = table->num_types++;
    IRType *type = &table->types[id];
    memset(type, 0, sizeof(IRType));
    
    type->kind = IR_TYPE_FLOAT;
    type->size = width / 8;
    type->alignment = type->size;
    type->float_type.width = width;
    
    return id;
}

uint32_t ir_type_size(const IRTypeTable *table, IRTypeId id) {
    if (id >= table->num_types) return 0;
    
    // 如果已经计算过大小，直接返回
    if (table->types[id].size != 0 || table->types[id].kind == IR_TYPE_VOID) {
        return table->types[id].size;
    }
    
    // 计算大小并缓存
    uint32_t size = ir_type_calculate_size(table, id);
    // 注意：这里不能直接修改const的table
    // 在实际实现中需要处理这个缓存逻辑
    return size;
}

bool ir_type_equals(const IRTypeTable *table, IRTypeId a, IRTypeId b) {
    if (a == b) return true;
    if (a >= table->num_types || b >= table->num_types) return false;
    
    const IRType *type_a = &table->types[a];
    const IRType *type_b = &table->types[b];
    
    if (type_a->kind != type_b->kind) return false;
    
    switch (type_a->kind) {
        case IR_TYPE_INT:
            return type_a->int_type.width == type_b->int_type.width &&
                   type_a->int_type.is_signed == type_b->int_type.is_signed;
        
        case IR_TYPE_FLOAT:
            return type_a->float_type.width == type_b->float_type.width;
        
        case IR_TYPE_PTR:
            return ir_type_equals(table, type_a->ptr_type.pointee, 
                                  type_b->ptr_type.pointee);
        
        case IR_TYPE_ARRAY:
            return type_a->array_type.count == type_b->array_type.count &&
                   ir_type_equals(table, type_a->array_type.elem_type,
                                  type_b->array_type.elem_type);
        
        case IR_TYPE_STRUCT:
            if (type_a->struct_type.num_fields != type_b->struct_type.num_fields) {
                return false;
            }
            for (uint32_t i = 0; i < type_a->struct_type.num_fields; i++) {
                if (!ir_type_equals(table, type_a->struct_type.field_types[i],
                                    type_b->struct_type.field_types[i])) {
                    return false;
                }
            }
            return true;
        
        case IR_TYPE_FUNC:
            if (type_a->func_type.num_params != type_b->func_type.num_params ||
                type_a->func_type.is_variadic != type_b->func_type.is_variadic) {
                return false;
            }
            if (!ir_type_equals(table, type_a->func_type.return_type,
                                type_b->func_type.return_type)) {
                return false;
            }
            for (uint32_t i = 0; i < type_a->func_type.num_params; i++) {
                if (!ir_type_equals(table, type_a->func_type.param_types[i],
                                    type_b->func_type.param_types[i])) {
                    return false;
                }
            }
            return true;
        
        default:
            return true;
    }
}

uint32_t ir_type_hash(const IRTypeTable *table, IRTypeId id) {
    if (id >= table->num_types) return 0;
    
    const IRType *type = &table->types[id];
    uint32_t hash = (uint32_t)type->kind;
    
    switch (type->kind) {
        case IR_TYPE_INT:
            hash = hash * 31 + type->int_type.width;
            hash = hash * 31 + (type->int_type.is_signed ? 1 : 0);
            break;
        
        case IR_TYPE_FLOAT:
            hash = hash * 31 + type->float_type.width;
            break;
        
        case IR_TYPE_PTR:
            hash = hash * 31 + ir_type_hash(table, type->ptr_type.pointee);
            break;
        
        case IR_TYPE_ARRAY:
            hash = hash * 31 + ir_type_hash(table, type->array_type.elem_type);
            hash = hash * 31 + type->array_type.count;
            break;
        
        case IR_TYPE_STRUCT:
            for (uint32_t i = 0; i < type->struct_type.num_fields; i++) {
                hash = hash * 31 + ir_type_hash(table, type->struct_type.field_types[i]);
            }
            break;
        
        case IR_TYPE_FUNC:
            hash = hash * 31 + ir_type_hash(table, type->func_type.return_type);
            for (uint32_t i = 0; i < type->func_type.num_params; i++) {
                hash = hash * 31 + ir_type_hash(table, type->func_type.param_types[i]);
            }
            break;
        
        default:
            break;
    }
    
    return hash;
}

const char *ir_type_to_string(const IRTypeTable *table, IRTypeId id,
                               char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return "";
    if (id >= table->num_types) {
        snprintf(buffer, buffer_size, "<invalid>");
        return buffer;
    }
    
    const IRType *type = &table->types[id];
    
    switch (type->kind) {
        case IR_TYPE_VOID:
            snprintf(buffer, buffer_size, "void");
            break;
        
        case IR_TYPE_INT:
            if (type->int_type.is_signed) {
                snprintf(buffer, buffer_size, "i%u", type->int_type.width);
            } else {
                snprintf(buffer, buffer_size, "u%u", type->int_type.width);
            }
            break;
        
        case IR_TYPE_FLOAT:
            snprintf(buffer, buffer_size, "f%u", type->float_type.width);
            break;
        
        case IR_TYPE_PTR: {
            char inner[64];
            snprintf(buffer, buffer_size, "%s*",
                     ir_type_to_string(table, type->ptr_type.pointee, inner, sizeof(inner)));
            break;
        }
        
        case IR_TYPE_ARRAY: {
            char inner[64];
            snprintf(buffer, buffer_size, "[%u x %s]",
                     type->array_type.count,
                     ir_type_to_string(table, type->array_type.elem_type, inner, sizeof(inner)));
            break;
        }
        
        case IR_TYPE_FUNC: {
            char ret[64];
            snprintf(buffer, buffer_size, "%s(",
                     ir_type_to_string(table, type->func_type.return_type, ret, sizeof(ret)));
            
            size_t len = strlen(buffer);
            for (uint32_t i = 0; i < type->func_type.num_params && len < buffer_size - 1; i++) {
                if (i > 0) {
                    buffer[len++] = ',';
                    buffer[len++] = ' ';
                }
                
                char param[64];
                const char *param_str = ir_type_to_string(table, 
                                                          type->func_type.param_types[i], 
                                                          param, sizeof(param));
                size_t param_len = strlen(param_str);
                if (len + param_len < buffer_size - 1) {
                    memcpy(buffer + len, param_str, param_len + 1);
                    len += param_len;
                }
            }
            
            if (type->func_type.is_variadic && len < buffer_size - 4) {
                strcpy(buffer + len, ", ...");
                len += 5;
            }
            
            if (len < buffer_size - 1) {
                buffer[len++] = ')';
                buffer[len] = '\0';
            }
            break;
        }
        
        case IR_TYPE_LABEL:
            snprintf(buffer, buffer_size, "label");
            break;
        
        case IR_TYPE_TOKEN:
            snprintf(buffer, buffer_size, "token");
            break;
        
        default:
            snprintf(buffer, buffer_size, "<unknown>");
            break;
    }
    
    return buffer;
}

bool ir_type_is_storable(const IRTypeTable *table, IRTypeId id) {
    if (id >= table->num_types) return false;
    
    switch (table->types[id].kind) {
        case IR_TYPE_INT:
        case IR_TYPE_FLOAT:
        case IR_TYPE_PTR:
        case IR_TYPE_ARRAY:
        case IR_TYPE_STRUCT:
            return true;
        default:
            return false;
    }
}

bool ir_type_is_integer(const IRTypeTable *table, IRTypeId id) {
    if (id >= table->num_types) return false;
    return table->types[id].kind == IR_TYPE_INT;
}

bool ir_type_is_float(const IRTypeTable *table, IRTypeId id) {
    if (id >= table->num_types) return false;
    return table->types[id].kind == IR_TYPE_FLOAT;
}