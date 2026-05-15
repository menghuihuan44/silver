#ifndef SILVER_IR_VALUE_H
#define SILVER_IR_VALUE_H

#include "silver/ir/ir_type.h"
#include "silver/support/arena.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 值种类
typedef enum {
    IR_VALUE_NONE = 0,      // 未初始化
    IR_VALUE_CONSTANT,      // 常量
    IR_VALUE_INSTRUCTION,   // 指令结果
    IR_VALUE_ARGUMENT,      // 函数参数
    IR_VALUE_BLOCK,         // 基本块
    IR_VALUE_GLOBAL,        // 全局变量
    IR_VALUE_FUNCTION,      // 函数引用
    
    IR_VALUE_COUNT
} IRValueKind;

// 值ID（在值数组中的索引）
typedef uint32_t IRValueId;
#define IR_VALUE_ID_INVALID 0xFFFF

// IR值结构 - 16字节定长（确保缓存友好）
typedef struct IRValue {
    IRValueKind kind : 8;       // 值种类
    IRTypeId type_id : 24;      // 类型ID（最多支持16M类型）
    
    union {
        // 常量值
        struct {
            union {
                int64_t int_val;        // 整数常量
                double float_val;       // 浮点常量
                uint64_t raw_val;       // 原始位模式
            };
        };
        
        // 指令结果
        struct {
            uint32_t inst_id;           // 指令ID
            uint32_t value_number;      // GVN值编号（用于优化）
        };
        
        // 函数参数
        struct {
            uint32_t param_index;       // 参数索引
            uint16_t param_attr;        // 参数属性
        };
        
        // 基本块
        struct {
            uint32_t block_id;          // 基本块ID
            uint32_t block_flags;       // 块属性标志
        };
        
        // 全局变量
        struct {
            const char *global_name;    // 全局变量名（指向Arena中的字符串）
            uint32_t global_flags;      // 全局属性标志
        };
        
        // 函数
        struct {
            const char *func_name;      // 函数名（指向Arena中的字符串）
            uint32_t func_flags;        // 函数属性标志
        };
    };
    
    uint32_t use_count;             // 使用计数（用于DCE）
} IRValue;

// 全局变量/函数标志
enum {
    IR_GLOBAL_CONST = (1 << 0),     // 常量
    IR_GLOBAL_EXTERN = (1 << 1),    // 外部定义
    IR_GLOBAL_WEAK = (1 << 2),      // 弱符号
    IR_GLOBAL_LOCAL = (1 << 3),     // 局部可见
    IR_GLOBAL_THREAD_LOCAL = (1 << 4), // 线程局部存储
};

// 值池 - 管理所有SSA值
typedef struct {
    IRValue *values;            // 值数组（在Arena中分配）
    uint32_t num_values;        // 当前值数量
    uint32_t capacity;          // 容量
    Arena *arena;
    
    // 常量缓存（用于常量去重）
    // 简单哈希表，避免重复创建相同常量
    #define CONST_CACHE_SIZE 256
    struct {
        IRValueId ids[CONST_CACHE_SIZE];
    } const_cache;
} IRValuePool;

// 初始化值池
void ir_value_pool_init(IRValuePool *pool, Arena *arena);

// 创建常量值
IRValueId ir_value_create_int_const(IRValuePool *pool, IRTypeId type, int64_t value);
IRValueId ir_value_create_float_const(IRValuePool *pool, IRTypeId type, double value);
IRValueId ir_value_create_null_ptr(IRValuePool *pool, IRTypeId type);

// 创建其他类型的值
IRValueId ir_value_create_instruction(IRValuePool *pool, IRTypeId type, uint32_t inst_id);
IRValueId ir_value_create_argument(IRValuePool *pool, IRTypeId type, uint32_t param_index);
IRValueId ir_value_create_block(IRValuePool *pool, uint32_t block_id);
IRValueId ir_value_create_global(IRValuePool *pool, IRTypeId type, const char *name);
IRValueId ir_value_create_function(IRValuePool *pool, IRTypeId type, const char *name);

// 获取值
IRValue *ir_value_get(IRValuePool *pool, IRValueId id);

// 检查值是否有效
bool ir_value_is_valid(const IRValuePool *pool, IRValueId id);

// 检查值是否为常量
bool ir_value_is_constant(const IRValuePool *pool, IRValueId id);

// 检查值是否为零常量
bool ir_value_is_zero(const IRValuePool *pool, IRValueId id);

// 检查值是否为常数-1
bool ir_value_is_all_ones(const IRValuePool *pool, IRValueId id);

// 获取常量的整数值（如果可能）
bool ir_value_get_int_constant(const IRValuePool *pool, IRValueId id, int64_t *out);

// 获取常量的浮点值（如果可能）
bool ir_value_get_float_constant(const IRValuePool *pool, IRValueId id, double *out);

// 值打印
const char *ir_value_to_string(const IRValuePool *pool, IRValueId id,
                                const IRTypeTable *types,
                                char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // SILVER_IR_VALUE_H