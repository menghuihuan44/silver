#ifndef SILVER_IR_BUILDER_H
#define SILVER_IR_BUILDER_H

#include "silver/ir/ir_module.h"
#include "silver/ir/ir_type.h"
#include "silver/ir/ir_value.h"
#include "silver/ir/ir_inst.h"
#include <stdarg.h>

#ifdef __cplusplus
extern "C" {
#endif

// IR构建器 - 提供便捷的IR构建API
typedef struct IRBuilder {
    IRModule *module;               // 所属模块
    IRFunction *current_func;       // 当前函数
    IRBlock *current_block;         // 当前基本块
    
    // 插入点
    bool insert_before_terminator;  // 是否在终结指令前插入
    
    // 快速类型引用（缓存常用类型ID）
    IRTypeId void_type;
    IRTypeId i1_type;
    IRTypeId i8_type;
    IRTypeId i16_type;
    IRTypeId i32_type;
    IRTypeId i64_type;
    IRTypeId f32_type;
    IRTypeId f64_type;
    IRTypeId ptr_type;

    // 值映射：指令索引 -> 值ID
    uint32_t *inst_to_value;     // 映射表
    uint32_t inst_to_value_size; // 映射表大小
    uint32_t next_value_id;      // 下一个值ID
} IRBuilder;

// 创建IR构建器
IRBuilder *ir_builder_create(IRModule *module);

// 销毁IR构建器
void ir_builder_destroy(IRBuilder *builder);

// 设置当前位置
void ir_builder_set_position(IRBuilder *builder, IRFunction *func, IRBlock *block);
void ir_builder_set_insert_point(IRBuilder *builder, IRBlock *block);

// 块操作
IRBlock *ir_builder_create_block(IRBuilder *builder, const char *name);
void ir_builder_seal_block(IRBuilder *builder, IRBlock *block);

// 常量创建
IRValueId ir_builder_int_const(IRBuilder *builder, IRTypeId type, int64_t value);
IRValueId ir_builder_float_const(IRBuilder *builder, IRTypeId type, double value);
IRValueId ir_builder_null(IRBuilder *builder, IRTypeId type);
IRValueId ir_builder_true(IRBuilder *builder);
IRValueId ir_builder_false(IRBuilder *builder);

// 内存操作
IRValueId ir_builder_alloca(IRBuilder *builder, IRTypeId type, uint32_t alignment);
IRValueId ir_builder_load(IRBuilder *builder, IRTypeId type, IRValueId ptr);
IRValueId ir_builder_store(IRBuilder *builder, IRValueId value, IRValueId ptr);
IRValueId ir_builder_gep(IRBuilder *builder, IRTypeId result_type, 
                          IRValueId base, IRValueId offset);

// 算术操作
IRValueId ir_builder_add(IRBuilder *builder, IRValueId lhs, IRValueId rhs);
IRValueId ir_builder_sub(IRBuilder *builder, IRValueId lhs, IRValueId rhs);
IRValueId ir_builder_mul(IRBuilder *builder, IRValueId lhs, IRValueId rhs);
IRValueId ir_builder_div(IRBuilder *builder, IRValueId lhs, IRValueId rhs, bool is_signed);
IRValueId ir_builder_rem(IRBuilder *builder, IRValueId lhs, IRValueId rhs, bool is_signed);
IRValueId ir_builder_neg(IRBuilder *builder, IRValueId val);

// 位操作
IRValueId ir_builder_and(IRBuilder *builder, IRValueId lhs, IRValueId rhs);
IRValueId ir_builder_or(IRBuilder *builder, IRValueId lhs, IRValueId rhs);
IRValueId ir_builder_xor(IRBuilder *builder, IRValueId lhs, IRValueId rhs);
IRValueId ir_builder_shl(IRBuilder *builder, IRValueId lhs, IRValueId rhs);
IRValueId ir_builder_lshr(IRBuilder *builder, IRValueId lhs, IRValueId rhs);
IRValueId ir_builder_ashr(IRBuilder *builder, IRValueId lhs, IRValueId rhs);

// 比较操作
IRValueId ir_builder_icmp(IRBuilder *builder, IROpcode cmp, 
                           IRValueId lhs, IRValueId rhs);
IRValueId ir_builder_fcmp(IRBuilder *builder, IROpcode cmp,
                           IRValueId lhs, IRValueId rhs);

// 类型转换
IRValueId ir_builder_trunc(IRBuilder *builder, IRValueId val, IRTypeId to_type);
IRValueId ir_builder_zext(IRBuilder *builder, IRValueId val, IRTypeId to_type);
IRValueId ir_builder_sext(IRBuilder *builder, IRValueId val, IRTypeId to_type);
IRValueId ir_builder_fptosi(IRBuilder *builder, IRValueId val, IRTypeId to_type);
IRValueId ir_builder_fptoui(IRBuilder *builder, IRValueId val, IRTypeId to_type);
IRValueId ir_builder_sitofp(IRBuilder *builder, IRValueId val, IRTypeId to_type);
IRValueId ir_builder_uitofp(IRBuilder *builder, IRValueId val, IRTypeId to_type);
IRValueId ir_builder_bitcast(IRBuilder *builder, IRValueId val, IRTypeId to_type);

// 控制流
IRValueId ir_builder_ret(IRBuilder *builder, IRValueId val);
IRValueId ir_builder_ret_void(IRBuilder *builder);
IRValueId ir_builder_br(IRBuilder *builder, IRBlock *target);
IRValueId ir_builder_condbr(IRBuilder *builder, IRValueId cond,
                             IRBlock *true_block, IRBlock *false_block);
IRValueId ir_builder_unreachable(IRBuilder *builder);

// PHI节点
IRValueId ir_builder_phi(IRBuilder *builder, IRTypeId type,
                          uint32_t num_incoming,
                          IRValueId *incoming_values,
                          IRBlock **incoming_blocks);

// 函数调用
IRValueId ir_builder_call(IRBuilder *builder, IRTypeId return_type,
                           IRValueId callee, uint32_t num_args, ...);

// 选择（三元运算符）
IRValueId ir_builder_select(IRBuilder *builder, IRValueId cond,
                             IRValueId true_val, IRValueId false_val);

// 复制（用于寄存器分配提示）
IRValueId ir_builder_copy(IRBuilder *builder, IRValueId val);

// 获取当前函数
IRFunction *ir_builder_get_function(const IRBuilder *builder);

// 获取当前块
IRBlock *ir_builder_get_block(const IRBuilder *builder);

// 获取指令对应的值ID（用于ISel/DCE等需要知道指令结果值ID的场景）
IRValueId ir_builder_get_inst_value(IRBuilder *builder, uint32_t inst_idx);

// 检查构建器是否处于有效状态
bool ir_builder_is_valid(const IRBuilder *builder);

#ifdef __cplusplus
}
#endif

#endif // SILVER_IR_BUILDER_H