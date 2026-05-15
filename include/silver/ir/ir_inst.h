#ifndef SILVER_IR_INST_H
#define SILVER_IR_INST_H

#include "silver/ir/ir_type.h"
#include "silver/ir/ir_value.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// IR操作码定义
typedef enum {
    // 终结指令
    IR_OP_RET = 0,          // 返回
    IR_OP_BR,               // 无条件分支
    IR_OP_BRCOND,           // 条件分支
    IR_OP_SWITCH,           // 多路分支
    IR_OP_UNREACHABLE,      // 不可达
    
    // 内存指令
    IR_OP_LOAD,             // 加载
    IR_OP_STORE,            // 存储
    IR_OP_ALLOCA,           // 栈分配
    IR_OP_GEP,              // 获取元素指针
    
    // 算术指令
    IR_OP_ADD,              // 加法
    IR_OP_SUB,              // 减法
    IR_OP_MUL,              // 乘法
    IR_OP_DIV,              // 有符号除法
    IR_OP_UDIV,             // 无符号除法
    IR_OP_REM,              // 有符号取余
    IR_OP_UREM,             // 无符号取余
    IR_OP_NEG,              // 整数取负
    
    // 位操作指令
    IR_OP_AND,              // 按位与
    IR_OP_OR,               // 按位或
    IR_OP_XOR,              // 按位异或
    IR_OP_SHL,              // 左移
    IR_OP_LSHR,             // 逻辑右移
    IR_OP_ASHR,             // 算术右移
    
    // 比较指令
    IR_OP_CMPEQ,            // 等于
    IR_OP_CMPNE,            // 不等于
    IR_OP_CMPLT,            // 有符号小于
    IR_OP_CMPLE,            // 有符号小于等于
    IR_OP_CMPGT,            // 有符号大于
    IR_OP_CMPGE,            // 有符号大于等于
    IR_OP_CMPULT,           // 无符号小于
    IR_OP_CMPULE,           // 无符号小于等于
    IR_OP_CMPUGT,           // 无符号大于
    IR_OP_CMPUGE,           // 无符号大于等于
    
    // 浮点指令
    IR_OP_FADD,             // 浮点加法
    IR_OP_FSUB,             // 浮点减法
    IR_OP_FMUL,             // 浮点乘法
    IR_OP_FDIV,             // 浮点除法
    IR_OP_FREM,             // 浮点取余
    IR_OP_FNEG,             // 浮点取负
    
    // 浮点比较
    IR_OP_FCMPOEQ,          // 有序等于
    IR_OP_FCMPONE,          // 有序不等于
    IR_OP_FCMPOLT,          // 有序小于
    IR_OP_FCMPOLE,          // 有序小于等于
    IR_OP_FCMPOGT,          // 有序大于
    IR_OP_FCMPOGE,          // 有序大于等于
    IR_OP_FCMPUEQ,          // 无序等于
    IR_OP_FCMPUNE,          // 无序不等于
    IR_OP_FCMPULT,          // 无序小于
    IR_OP_FCMPULE,          // 无序小于等于
    IR_OP_FCMPUGT,          // 无序大于
    IR_OP_FCMPUGE,          // 无序大于等于
    
    // 类型转换指令
    IR_OP_TRUNC,            // 整数截断
    IR_OP_ZEXT,             // 零扩展
    IR_OP_SEXT,             // 符号扩展
    IR_OP_FPTOUI,           // 浮点转无符号整数
    IR_OP_FPTOSI,           // 浮点转有符号整数
    IR_OP_UITOFP,           // 无符号整数转浮点
    IR_OP_SITOFP,           // 有符号整数转浮点
    IR_OP_FPTRUNC,          // 浮点截断（如f64->f32）
    IR_OP_FPEXT,            // 浮点扩展（如f32->f64）
    IR_OP_PTRTOI,           // 指针转整数
    IR_OP_ITOPTR,           // 整数转指针
    IR_OP_BITCAST,          // 位模式重解释
    
    // 特殊指令
    IR_OP_PHI,              // φ节点（SSA）
    IR_OP_CALL,             // 函数调用
    IR_OP_SELECT,           // 条件选择
    IR_OP_COPY,             // 复制（用于寄存器分配）
    
    // 原子指令
    IR_OP_ATOMIC_LOAD,      // 原子加载
    IR_OP_ATOMIC_STORE,     // 原子存储
    IR_OP_ATOMIC_RMW,       // 原子读-改-写
    IR_OP_ATOMIC_CAS,       // 原子比较并交换
    IR_OP_FENCE,            // 内存屏障
    
    IR_OP_NOP,              // 空操作

    IR_OP_COUNT
} IROpcode;

// 指令标志
enum {
    IR_FLAG_NONE = 0,
    IR_FLAG_NNSW = (1 << 0),    // 无符号溢出（nuw - no unsigned wrap）
    IR_FLAG_NUW = (1 << 1),     // 无无符号溢出
    IR_FLAG_EXACT = (1 << 2),   // 精确（用于除法）
    IR_FLAG_VOLATILE = (1 << 3), // 易变的
    IR_FLAG_ATOMIC = (1 << 4),  // 原子的
    IR_FLAG_ALIGNED = (1 << 5), // 对齐的
};

// 内存排序（用于原子操作）
typedef enum {
    IR_MEMORY_ORDER_RELAXED = 0,
    IR_MEMORY_ORDER_CONSUME,
    IR_MEMORY_ORDER_ACQUIRE,
    IR_MEMORY_ORDER_RELEASE,
    IR_MEMORY_ORDER_ACQ_REL,
    IR_MEMORY_ORDER_SEQ_CST,
} IRMemoryOrder;

// IR指令 - 定长16字节
typedef struct IRInst {
    IROpcode opcode : 16;       // 操作码
    IRTypeId type_id : 16;      // 结果类型ID
    uint16_t flags;             // 指令标志
    
    uint16_t operand0_id;       // 操作数0的值ID
    uint16_t operand1_id;       // 操作数1的值ID
    uint16_t operand2_id;       // 操作数2的值ID
    
    uint16_t extra;             // 额外数据（取决于操作码）
    // 对于phi: 前驱数量
    // 对于call: 参数数量
    // 对于alloca: 对齐
    
    // 指令在基本块中的原始顺序（用于调试和优化）
    uint16_t original_order;
} IRInst;

// 确保指令大小为16字节
_Static_assert(sizeof(IRInst) == 16, "IRInst must be 16 bytes");

// 指令创建（这些函数由IRBuilder提供，这里只是声明）
// 实际实现在ir_builder.c中

// 获取操作码名称
const char *ir_opcode_name(IROpcode opcode);

// 获取操作码所需的操作数数量
int ir_opcode_num_operands(IROpcode opcode);

// 判断指令是否有副作用
bool ir_opcode_has_side_effects(IROpcode opcode);

// 判断指令是否为终结指令
bool ir_opcode_is_terminator(IROpcode opcode);

// 判断指令是否可交换
bool ir_opcode_is_commutative(IROpcode opcode);

// 判断指令是否为比较指令
bool ir_opcode_is_comparison(IROpcode opcode);

// 获取比较指令的反向操作码
IROpcode ir_opcode_reverse_comparison(IROpcode opcode);

// 指令字符串表示
const char *ir_inst_to_string(const IRInst *inst, const IRTypeTable *types,
                               const IRValuePool *values,
                               char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // SILVER_IR_INST_H