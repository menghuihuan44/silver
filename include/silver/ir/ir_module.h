#ifndef SILVER_IR_MODULE_H
#define SILVER_IR_MODULE_H

#include "silver/ir/ir_type.h"
#include "silver/ir/ir_value.h"
#include "silver/ir/ir_inst.h"
#include "silver/support/arena.h"
#include "silver/support/error.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// 前向声明
typedef struct IRFunction IRFunction;
typedef struct IRBuilder IRBuilder;

// 基本块结构
typedef struct IRBlock {
    uint32_t id;                    // 块ID
    uint32_t first_inst;            // 第一条指令的索引
    uint32_t last_inst;             // 最后一条指令的索引
    uint32_t num_predecessors;      // 前驱块数量
    uint32_t *predecessors;         // 前驱块ID数组（在Arena中分配）
    uint32_t num_successors;        // 后继块数量
    uint32_t *successors;           // 后继块ID数组（在Arena中分配）
    const char *name;               // 块名称（可选，用于调试）
    uint16_t flags;                 // 块标志
    bool is_sealed;                 // 是否已封闭（不再添加指令）
} IRBlock;

// 块标志
enum {
    IR_BLOCK_ENTRY = (1 << 0),      // 入口块
    IR_BLOCK_EXIT = (1 << 1),       // 出口块
    IR_BLOCK_LOOP_HEADER = (1 << 2), // 循环头
};

// 函数结构
typedef struct IRFunction {
    uint32_t id;                    // 函数ID
    const char *name;               // 函数名
    IRTypeId func_type_id;          // 函数类型ID
    
    // 参数
    uint32_t num_args;
    IRValueId *arg_values;          // 参数值ID数组
    
    // 基本块
    IRBlock *blocks;                // 基本块数组（在Arena中分配）
    uint32_t num_blocks;
    uint32_t block_capacity;
    
    // 指令（所有基本块的指令都存在这里）
    IRInst *instructions;           // 指令数组（在Arena中分配）
    uint32_t num_insts;
    uint32_t inst_capacity;

    // 指令索引到结果值ID的快速映射
    IRValueId *inst_to_value;
    uint32_t inst_to_value_capacity;
    
    // 栈帧信息
    uint32_t stack_size;            // 栈帧大小
    uint32_t num_spill_slots;       // 溢出槽数量
    
    // 属性
    uint32_t flags;                 // 函数标志
    
    // 指向所属模块
    struct IRModule *module;
} IRFunction;

// 函数标志
enum {
    IR_FUNC_LEAF = (1 << 0),        // 叶函数（不调用其他函数）
    IR_FUNC_NO_RECURSE = (1 << 1),  // 非递归
    IR_FUNC_NO_UNWIND = (1 << 2),   // 不展开栈
    IR_FUNC_READONLY = (1 << 3),    // 只读
    IR_FUNC_WRITEONLY = (1 << 4),   // 只写
};

// 全局变量结构
typedef struct IRGlobal {
    uint32_t id;                    // 全局变量ID
    const char *name;               // 名称
    IRTypeId type_id;               // 类型ID
    IRValueId init_value;           // 初始值ID（IR_VALUE_ID_INVALID表示未初始化）
    uint32_t alignment;             // 对齐
    uint32_t flags;                 // 标志
    uint64_t size;                  // 大小
} IRGlobal;

// IR模块 - 顶层结构
typedef struct IRModule {
    // 内存管理
    Arena *arena;                   // 模块级Arena
    
    // 错误处理
    SilverErrorContext *error_ctx;  // 错误上下文
    
    // 类型表
    IRTypeTable type_table;         // 类型表
    
    // 值池
    IRValuePool value_pool;         // 全局值池
    
    // 函数
    IRFunction *functions;          // 函数数组（在Arena中分配）
    uint32_t num_functions;
    uint32_t func_capacity;
    
    // 全局变量
    IRGlobal *globals;              // 全局变量数组（在Arena中分配）
    uint32_t num_globals;
    uint32_t global_capacity;
    
    // 模块属性
    const char *module_name;        // 模块名称
    const char *target_triple;      // 目标三元组（如"x86_64-linux-gnu"）
    const char *data_layout;        // 数据布局字符串
    
    // 模块标志
    uint32_t flags;
    
    // 统计信息
    struct {
        uint32_t total_instructions;
        uint32_t total_blocks;
        uint32_t total_values;
        uint32_t optimization_passes_run;
        double compile_time_ms;
    } stats;
} IRModule;

// 模块标志
enum {
    IR_MODULE_PIC = (1 << 0),       // 位置无关代码
    IR_MODULE_PIE = (1 << 1),       // 位置无关可执行文件
    IR_MODULE_DEBUG_INFO = (1 << 2), // 包含调试信息
};

// 创建IR模块
IRModule *ir_module_create(const char *name, const char *target_triple);

// 销毁IR模块
void ir_module_destroy(IRModule *module);

// 模块验证
bool ir_module_verify(IRModule *module);

// 打印模块（调试用）
void ir_module_dump(const IRModule *module, FILE *out);

// 获取模块统计信息
void ir_module_get_stats(const IRModule *module, 
                         char *buffer, size_t buffer_size);

// 函数管理
IRFunction *ir_module_add_function(IRModule *module, const char *name, 
                                   IRTypeId func_type_id);
IRFunction *ir_module_get_function(const IRModule *module, uint32_t id);
IRFunction *ir_module_find_function(const IRModule *module, const char *name);

// 全局变量管理
IRGlobal *ir_module_add_global(IRModule *module, const char *name,
                                IRTypeId type_id, IRValueId init_value);
IRGlobal *ir_module_get_global(const IRModule *module, uint32_t id);
IRGlobal *ir_module_find_global(const IRModule *module, const char *name);

// 基本块管理（函数级别）
IRBlock *ir_function_add_block(IRFunction *func, const char *name);
IRBlock *ir_function_get_block(const IRFunction *func, uint32_t id);

// 指令管理（函数级别）
IRInst *ir_function_add_inst(IRFunction *func, IROpcode opcode, 
                              IRTypeId type_id, uint16_t flags,
                              IRValueId op0, IRValueId op1, IRValueId op2,
                              uint16_t extra);

// 更新基本块的指令范围
void ir_block_update_range(IRBlock *block, uint32_t first_inst, uint32_t last_inst);

#ifdef __cplusplus
}
#endif

#endif // SILVER_IR_MODULE_H