#ifndef SILVER_CODEGEN_MACHINE_H
#define SILVER_CODEGEN_MACHINE_H

#include "silver/ir/ir_module.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 目标平台枚举
typedef enum {
    SILVER_TARGET_X86_64,
    SILVER_TARGET_X86_32,
    SILVER_TARGET_ARM64,
    SILVER_TARGET_ARM32,
    SILVER_TARGET_RISCV64,
    SILVER_TARGET_RISCV32,
    SILVER_TARGET_COUNT
} SilverTargetArch;

// 机器操作码（统一表示，各平台是子集）
typedef enum {
    // 通用操作
    MACH_NOP = 0,
    MACH_COPY,
    
    // 整数算术
    MACH_ADD, MACH_ADD_IMM,
    MACH_SUB, MACH_SUB_IMM,
    MACH_MUL, MACH_MUL_IMM,
    MACH_DIV, MACH_DIV_IMM,
    MACH_UDIV, MACH_UDIV_IMM,
    MACH_REM, MACH_UREM,
    MACH_NEG,
    
    // 位操作
    MACH_AND, MACH_AND_IMM,
    MACH_OR, MACH_OR_IMM,
    MACH_XOR, MACH_XOR_IMM,
    MACH_SHL, MACH_SHL_IMM,
    MACH_LSHR, MACH_LSHR_IMM,
    MACH_ASHR, MACH_ASHR_IMM,
    MACH_NOT,
    
    // 浮点算术
    MACH_FADD, MACH_FSUB,
    MACH_FMUL, MACH_FDIV,
    MACH_FNEG, MACH_FABS,
    MACH_FSQRT,
    
    // 比较
    MACH_CMP, MACH_CMP_IMM,
    MACH_FCMP,
    MACH_TEST, MACH_TEST_IMM,
    
    // 条件设置
    MACH_SETCC,    // 根据条件码设置字节
    MACH_SETCC_IMM,
    
    // 类型转换
    MACH_MOVSX,    // 符号扩展移动
    MACH_MOVZX,    // 零扩展移动
    MACH_CVTSI2SS, // 整数转单精度
    MACH_CVTSI2SD, // 整数转双精度
    MACH_CVTSS2SI, // 单精度转整数
    MACH_CVTSD2SI, // 双精度转整数
    MACH_CVTSS2SD, // 单精度转双精度
    MACH_CVTSD2SS, // 双精度转单精度
    
    // 内存操作
    MACH_MOV, MACH_MOV_IMM,
    MACH_LOAD, MACH_LOAD_IMM,
    MACH_STORE, MACH_STORE_IMM,
    MACH_LEA,      // 加载有效地址
    MACH_PUSH, MACH_POP,
    
    // 控制流
    MACH_JMP,
    MACH_JCC,      // 条件跳转
    MACH_CALL,
    MACH_RET,
    MACH_INDIRECT_JMP,
    MACH_INDIRECT_CALL,
    
    // 特殊
    MACH_CDQ,      // 符号扩展(RAX->RDX:RAX)
    MACH_CQO,      // 符号扩展(RAX->RDX:RAX) 64位
    MACH_SYSCALL,
    MACH_INT3,     // 断点
    
    MACH_OP_COUNT
} MachineOpcode;

// 机器寄存器
typedef enum {
    // 通用寄存器（x86-64命名，但用作统一表示）
    REG_NONE = 0,
    
    // 调用者保存寄存器
    REG_RAX, REG_RCX, REG_RDX, REG_RSI, REG_RDI, REG_R8, REG_R9, REG_R10, REG_R11,
    // 被调用者保存寄存器
    REG_RBX, REG_R12, REG_R13, REG_R14, REG_R15,
    // 特殊寄存器
    REG_RSP, REG_RBP, REG_RIP,
    
    // 浮点寄存器
    REG_XMM0, REG_XMM1, REG_XMM2, REG_XMM3,
    REG_XMM4, REG_XMM5, REG_XMM6, REG_XMM7,
    REG_XMM8, REG_XMM9, REG_XMM10, REG_XMM11,
    REG_XMM12, REG_XMM13, REG_XMM14, REG_XMM15,
    
    REG_COUNT,
    
    // 别名
    REG_EAX = REG_RAX, REG_ECX = REG_RCX, REG_EDX = REG_RDX,
    REG_ESI = REG_RSI, REG_EDI = REG_RDI,
    REG_EBX = REG_RBX, REG_ESP = REG_RSP, REG_EBP = REG_RBP,
} MachineRegister;

// 寄存器类别
typedef enum {
    REG_CLASS_NONE = 0,
    REG_CLASS_GPR,      // 通用寄存器（整数）
    REG_CLASS_FPR,      // 浮点寄存器
    REG_CLASS_SPECIAL,  // 特殊寄存器（SP, BP等）
} RegisterClass;

// 条件码
typedef enum {
    COND_NONE = 0,
    COND_E,      // 等于/零
    COND_NE,     // 不等于/非零
    COND_L,      // 小于（有符号）
    COND_LE,     // 小于等于（有符号）
    COND_G,      // 大于（有符号）
    COND_GE,     // 大于等于（有符号）
    COND_B,      // 低于（无符号）
    COND_BE,     // 低于等于（无符号）
    COND_A,      // 高于（无符号）
    COND_AE,     // 高于等于（无符号）
    COND_S,      // 符号标志
    COND_NS,     // 无符号标志
    COND_O,      // 溢出
    COND_NO,     // 无溢出
    COND_P,      // 奇偶校验
    COND_NP,     // 无奇偶校验
    COND_ALWAYS, // 无条件
    
    COND_COUNT
} MachineCondition;

// 机器指令 - 32位定长编码
typedef struct {
    uint32_t opcode : 10;      // 机器操作码
    uint32_t rd     : 5;       // 目标寄存器
    uint32_t rn     : 5;       // 源寄存器1
    uint32_t rm     : 5;       // 源寄存器2
    uint32_t imm    : 7;       // 立即数/扩展位
    // 额外的立即数可以跟随在指令后面
} MachineInst;

_Static_assert(sizeof(MachineInst) == 4, "MachineInst must be 32 bits");

// 机器指令扩展（用于存储超出32位的立即数）
typedef struct {
    MachineInst base;
    uint64_t extended_imm;  // 扩展立即数
    int32_t displacement;   // 内存偏移
} MachineInstExt;

// 寄存器描述
typedef struct {
    MachineRegister reg;
    RegisterClass reg_class;
    const char *name;
    uint8_t encoding;           // 硬件编码
    bool is_caller_saved;
    bool is_callee_saved;
    bool is_special;            // 特殊用途寄存器
    uint32_t hints;             // 使用提示
} RegisterDesc;

// 寄存器集合（位图）
typedef uint64_t RegisterSet;
#define REG_SET_EMPTY 0
#define REG_SET_ALL 0xFFFFFFFFFFFFFFFFULL

static inline bool reg_set_has(RegisterSet set, MachineRegister reg) {
    return (set & (1ULL << reg)) != 0;
}

static inline RegisterSet reg_set_add(RegisterSet set, MachineRegister reg) {
    return set | (1ULL << reg);
}

static inline RegisterSet reg_set_remove(RegisterSet set, MachineRegister reg) {
    return set & ~(1ULL << reg);
}

// 调用约定
typedef struct {
    RegisterSet caller_saved;       // 调用者保存寄存器
    RegisterSet callee_saved;       // 被调用者保存寄存器
    RegisterSet argument_regs[8];   // 参数传递寄存器（按参数位置）
    RegisterSet return_reg;         // 返回值寄存器
    RegisterSet return_reg_hi;      // 高位返回值寄存器（用于128位值）
    uint32_t max_int_args;          // 最大整数参数数量
    uint32_t max_float_args;        // 最大浮点参数数量
    uint32_t stack_alignment;       // 栈对齐
    uint32_t red_zone_size;         // 红区大小（x86-64特有）
} CallingConvention;

// 目标平台描述
typedef struct SilverTarget SilverTarget;
struct SilverTarget {
    SilverTargetArch arch;
    const char *name;
    const char *triple;
    bool is_64_bit;
    uint32_t pointer_size;
    uint32_t max_alignment;
    
    // 寄存器信息
    const RegisterDesc *registers;
    uint32_t num_registers;
    
    // 调用约定
    CallingConvention cc;
    
    // 可用寄存器（按类别）
    RegisterSet available_gpr;
    RegisterSet available_fpr;
    
    // 特殊寄存器
    MachineRegister sp_reg;     // 栈指针
    MachineRegister bp_reg;     // 基址指针
    MachineRegister ip_reg;     // 指令指针
    MachineRegister flag_reg;   // 标志寄存器
    
    // 指令编码器
    bool (*encode)(const SilverTarget *target, const MachineInstExt *inst,
                   uint8_t *buffer, uint32_t *length);
    
    // 指令发射器
    uint32_t (*emit_prologue)(const SilverTarget *target, 
                               IRFunction *func, uint8_t *buffer);
    uint32_t (*emit_epilogue)(const SilverTarget *target,
                               IRFunction *func, uint8_t *buffer);
    
    // 平台特定数据
    void *platform_data;
};

// 获取寄存器描述
const RegisterDesc *machine_get_register_desc(const SilverTarget *target, 
                                               MachineRegister reg);

// 获取寄存器名称
const char *machine_reg_name(const SilverTarget *target, MachineRegister reg);

// 创建寄存器掩码
RegisterSet machine_reg_mask(MachineRegister reg);
RegisterSet machine_reg_mask_range(MachineRegister start, MachineRegister end);

// 获取机器操作码名称
const char *machine_opcode_name(MachineOpcode opcode);

// 获取条件码名称
const char *machine_cond_name(MachineCondition cond);

// 获取条件码的反向
MachineCondition machine_cond_invert(MachineCondition cond);

#ifdef __cplusplus
}
#endif

#endif // SILVER_CODEGEN_MACHINE_H