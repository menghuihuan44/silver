#ifndef SILVER_IR_TYPE_H
#define SILVER_IR_TYPE_H

#include "silver/support/arena.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// IR类型种类
typedef enum {
    IR_TYPE_VOID = 0,       // void类型
    IR_TYPE_INT,            // 整数类型
    IR_TYPE_FLOAT,          // 浮点类型
    IR_TYPE_PTR,            // 指针类型
    IR_TYPE_ARRAY,          // 数组类型
    IR_TYPE_STRUCT,         // 结构体类型
    IR_TYPE_FUNC,           // 函数类型
    IR_TYPE_LABEL,          // 标签类型（基本块）
    IR_TYPE_TOKEN,          // 不透明token类型
    
    IR_TYPE_COUNT
} IRTypeKind;

// 整数类型宽度
typedef enum {
    IR_INT_1 = 1,
    IR_INT_8 = 8,
    IR_INT_16 = 16,
    IR_INT_32 = 32,
    IR_INT_64 = 64,
} IRIntWidth;

// 浮点类型宽度
typedef enum {
    IR_FLOAT_32 = 32,       // 单精度
    IR_FLOAT_64 = 64,       // 双精度
} IRFloatWidth;

// 类型标识符（在类型数组中的索引）
typedef uint32_t IRTypeId;
#define IR_TYPE_ID_INVALID 0xFFFF

// 前向声明
typedef struct IRType IRType;
typedef struct IRTypeTable IRTypeTable;
typedef struct IRModule IRModule;

// IR类型结构
struct IRType {
    IRTypeKind kind;            // 类型种类
    uint32_t size;              // 类型大小（字节）
    uint32_t alignment;         // 对齐要求（字节）
    
    // 类型特定信息
    union {
        struct {
            IRIntWidth width;   // 整数宽度
            bool is_signed;     // 是否有符号
        } int_type;
        
        struct {
            IRFloatWidth width; // 浮点宽度
        } float_type;
        
        struct {
            IRTypeId pointee;   // 指向的类型ID
        } ptr_type;
        
        struct {
            IRTypeId elem_type; // 元素类型ID
            uint32_t count;     // 元素数量（0表示可变长度）
        } array_type;
        
        struct {
            uint32_t num_fields;    // 字段数量
            IRTypeId *field_types;  // 字段类型ID数组（在Arena中分配）
            uint32_t *field_offsets;// 字段偏移量数组
            const char *name;       // 结构体名称（可选）
        } struct_type;
        
        struct {
            IRTypeId return_type;   // 返回类型ID
            uint32_t num_params;    // 参数数量
            IRTypeId *param_types;  // 参数类型ID数组（在Arena中分配）
            bool is_variadic;       // 是否可变参数
        } func_type;
    };
};

// 类型表 - 管理所有类型
struct IRTypeTable {
    IRType *types;              // 类型数组（在Arena中分配）
    uint32_t num_types;         // 当前类型数量
    uint32_t capacity;          // 容量
    Arena *arena;
    
    // 预定义类型的ID（快速访问）
    IRTypeId void_type;
    IRTypeId i1_type;
    IRTypeId i8_type;
    IRTypeId i16_type;
    IRTypeId i32_type;
    IRTypeId i64_type;
    IRTypeId u8_type;
    IRTypeId u16_type;
    IRTypeId u32_type;
    IRTypeId u64_type;
    IRTypeId f32_type;
    IRTypeId f64_type;
    IRTypeId ptr_type;          // 通用指针（void*）
    IRTypeId label_type;
    IRTypeId token_type;
};

// 初始化类型表
void ir_type_table_init(IRTypeTable *table, Arena *arena);

// 获取类型
IRType *ir_type_get(IRTypeTable *table, IRTypeId id);

// 获取类型大小
uint32_t ir_type_size(const IRTypeTable *table, IRTypeId id);

// 获取类型对齐
uint32_t ir_type_alignment(const IRTypeTable *table, IRTypeId id);

// 创建新类型（返回类型ID）
IRTypeId ir_type_create_int(IRTypeTable *table, IRIntWidth width, bool is_signed);
IRTypeId ir_type_create_float(IRTypeTable *table, IRFloatWidth width);
IRTypeId ir_type_create_ptr(IRTypeTable *table, IRTypeId pointee_type);
IRTypeId ir_type_create_array(IRTypeTable *table, IRTypeId elem_type, uint32_t count);
IRTypeId ir_type_create_struct(IRTypeTable *table, const char *name,
                                uint32_t num_fields, IRTypeId *field_types);
IRTypeId ir_type_create_func(IRTypeTable *table, IRTypeId return_type,
                              uint32_t num_params, IRTypeId *param_types,
                              bool is_variadic);

// 类型相等性检查
bool ir_type_equals(const IRTypeTable *table, IRTypeId a, IRTypeId b);

// 类型哈希
uint32_t ir_type_hash(const IRTypeTable *table, IRTypeId id);

// 类型字符串表示
const char *ir_type_to_string(const IRTypeTable *table, IRTypeId id, 
                               char *buffer, size_t buffer_size);

// 判断类型是否可存储
bool ir_type_is_storable(const IRTypeTable *table, IRTypeId id);

// 判断类型是否为整数
bool ir_type_is_integer(const IRTypeTable *table, IRTypeId id);

// 判断类型是否为浮点
bool ir_type_is_float(const IRTypeTable *table, IRTypeId id);

// 获取类型种类
static inline IRTypeKind ir_type_get_kind(const IRTypeTable *table, IRTypeId id) {
    if (id >= table->num_types) return IR_TYPE_VOID;
    return table->types[id].kind;
}

#ifdef __cplusplus
}
#endif

#endif // SILVER_IR_TYPE_H