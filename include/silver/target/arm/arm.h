#ifndef SILVER_TARGET_ARM_H
#define SILVER_TARGET_ARM_H

#include "silver/codegen/machine.h"
#include "silver/codegen/isel.h"
#include "silver/ir/ir_module.h"

#ifdef __cplusplus
extern "C" {
#endif

// ARM64特定寄存器定义
// ARM64有31个通用寄存器（X0-X30）和32个浮点寄存器（V0-V31）
// 我们映射到通用寄存器枚举

// ARM64寄存器编码（硬件编码）
typedef enum {
    ARM64_REG_X0  = 0,  ARM64_REG_X1  = 1,
    ARM64_REG_X2  = 2,  ARM64_REG_X3  = 3,
    ARM64_REG_X4  = 4,  ARM64_REG_X5  = 5,
    ARM64_REG_X6  = 6,  ARM64_REG_X7  = 7,
    ARM64_REG_X8  = 8,  ARM64_REG_X9  = 9,
    ARM64_REG_X10 = 10, ARM64_REG_X11 = 11,
    ARM64_REG_X12 = 12, ARM64_REG_X13 = 13,
    ARM64_REG_X14 = 14, ARM64_REG_X15 = 15,
    ARM64_REG_X16 = 16, ARM64_REG_X17 = 17,
    ARM64_REG_X18 = 18, ARM64_REG_X19 = 19,
    ARM64_REG_X20 = 20, ARM64_REG_X21 = 21,
    ARM64_REG_X22 = 22, ARM64_REG_X23 = 23,
    ARM64_REG_X24 = 24, ARM64_REG_X25 = 25,
    ARM64_REG_X26 = 26, ARM64_REG_X27 = 27,
    ARM64_REG_X28 = 28, ARM64_REG_X29 = 29,
    ARM64_REG_X30 = 30,  // LR (Link Register)
    
    // 特殊寄存器
    ARM64_REG_XZR = 31,  // 零寄存器
    ARM64_REG_SP  = 31,  // 栈指针（在特定上下文中）
    ARM64_REG_LR  = 30,  // 链接寄存器
    
    // 浮点/向量寄存器
    ARM64_REG_V0  = 32, ARM64_REG_V1  = 33,
    ARM64_REG_V2  = 34, ARM64_REG_V3  = 35,
    ARM64_REG_V4  = 36, ARM64_REG_V5  = 37,
    ARM64_REG_V6  = 38, ARM64_REG_V7  = 39,
    ARM64_REG_V8  = 40, ARM64_REG_V9  = 41,
    ARM64_REG_V10 = 42, ARM64_REG_V11 = 43,
    ARM64_REG_V12 = 44, ARM64_REG_V13 = 45,
    ARM64_REG_V14 = 46, ARM64_REG_V15 = 47,
    ARM64_REG_V16 = 48, ARM64_REG_V17 = 49,
    ARM64_REG_V18 = 50, ARM64_REG_V19 = 51,
    ARM64_REG_V20 = 52, ARM64_REG_V21 = 53,
    ARM64_REG_V22 = 54, ARM64_REG_V23 = 55,
    ARM64_REG_V24 = 56, ARM64_REG_V25 = 57,
    ARM64_REG_V26 = 58, ARM64_REG_V27 = 59,
    ARM64_REG_V28 = 60, ARM64_REG_V29 = 61,
    ARM64_REG_V30 = 62, ARM64_REG_V31 = 63,
    
    ARM64_REG_COUNT = 64
} ARM64Register;

// ARM64指令编码类型
// ARM64指令固定32位宽度

// 数据处理立即数指令编码
typedef struct {
    uint32_t sf  : 1;   // 大小标志 (0=32位, 1=64位)
    uint32_t op  : 2;   // 操作类型
    uint32_t S   : 1;   // 设置标志
    uint32_t _1101 : 4; // 固定位 1101
    uint32_t sh  : 2;   // 移位类型
    uint32_t imm12 : 12; // 12位立即数
    uint32_t Rn  : 5;   // 源寄存器
    uint32_t Rd  : 5;   // 目标寄存器
} __attribute__((packed)) ARM64DataImm;

// 数据处理寄存器指令编码
typedef struct {
    uint32_t sf  : 1;   // 大小标志
    uint32_t _00  : 2;  // 固定位
    uint32_t S   : 1;   // 设置标志
    uint32_t _0101 : 4; // 固定位 0101
    uint32_t shift : 2; // 移位类型
    uint32_t _0   : 1;  // 固定位
    uint32_t Rm  : 5;   // 第二个源寄存器
    uint32_t imm6 : 6;  // 移位量
    uint32_t Rn  : 5;   // 第一个源寄存器
    uint32_t Rd  : 5;   // 目标寄存器
} __attribute__((packed)) ARM64DataReg;

// 加载/存储指令编码
typedef struct {
    uint32_t size : 2;   // 数据大小
    uint32_t _111 : 3;   // 固定位
    uint32_t V   : 1;    // 向量标志
    uint32_t _00  : 2;   // 固定位
    uint32_t opc : 2;    // 操作码
    uint32_t imm12 : 12; // 12位无符号偏移
    uint32_t Rn  : 5;    // 基址寄存器
    uint32_t Rt  : 5;    // 传输寄存器
} __attribute__((packed)) ARM64LoadStore;

// 分支指令编码
typedef struct {
    uint32_t _000101 : 6; // 固定位
    uint32_t imm26 : 26;  // 26位有符号偏移
} __attribute__((packed)) ARM64Branch;

// 条件分支指令编码
typedef struct {
    uint32_t _01010100 : 8; // 固定位
    uint32_t imm19 : 19;    // 19位有符号偏移
    uint32_t _0   : 1;      // 固定位
    uint32_t cond : 4;      // 条件码
} __attribute__((packed)) ARM64CondBranch;

// ARM64条件码
typedef enum {
    ARM64_COND_EQ = 0,  // 等于
    ARM64_COND_NE = 1,  // 不等于
    ARM64_COND_CS = 2,  // 进位设置 (HS)
    ARM64_COND_CC = 3,  // 进位清除 (LO)
    ARM64_COND_MI = 4,  // 负数
    ARM64_COND_PL = 5,  // 正数或零
    ARM64_COND_VS = 6,  // 溢出
    ARM64_COND_VC = 7,  // 无溢出
    ARM64_COND_HI = 8,  // 无符号大于
    ARM64_COND_LS = 9,  // 无符号小于等于
    ARM64_COND_GE = 10, // 有符号大于等于
    ARM64_COND_LT = 11, // 有符号小于
    ARM64_COND_GT = 12, // 有符号大于
    ARM64_COND_LE = 13, // 有符号小于等于
    ARM64_COND_AL = 14, // 总是
    ARM64_COND_NV = 15, // 从不

    // 添加别名
    ARM64_COND_LO = ARM64_COND_CC,  // 无符号小于（同CC）
    ARM64_COND_HS = ARM64_COND_CS,  // 无符号大于等于（同CS）
} ARM64Condition;

// ARM64特定操作码（用于内部编码）
typedef enum {
    // 数据处理立即数
    ARM64_ADD_IMM  = 0,  // add rd, rn, #imm
    ARM64_SUB_IMM  = 1,  // sub rd, rn, #imm
    ARM64_ADDS_IMM = 2,  // adds rd, rn, #imm (设置标志)
    ARM64_SUBS_IMM = 3,  // subs rd, rn, #imm
    
    // 数据处理寄存器
    ARM64_ADD_REG  = 0,  // add rd, rn, rm
    ARM64_SUB_REG  = 1,  // sub rd, rn, rm
    ARM64_AND_REG  = 2,  // and rd, rn, rm
    ARM64_ORR_REG  = 3,  // orr rd, rn, rm
    ARM64_EOR_REG  = 4,  // eor rd, rn, rm
    ARM64_LSL_REG  = 5,  // lsl rd, rn, rm
    ARM64_LSR_REG  = 6,  // lsr rd, rn, rm
    ARM64_ASR_REG  = 7,  // asr rd, rn, rm
    
    // 乘除法
    ARM64_MADD     = 0,  // madd rd, rn, rm, ra
    ARM64_MSUB     = 1,  // msub rd, rn, rm, ra
    ARM64_SDIV     = 2,  // sdiv rd, rn, rm
    ARM64_UDIV     = 3,  // udiv rd, rn, rm
    
    // 加载/存储
    ARM64_LDR      = 0,  // ldr rt, [rn, #offset]
    ARM64_STR      = 1,  // str rt, [rn, #offset]
    ARM64_LDRB     = 2,  // ldrb rt, [rn, #offset]
    ARM64_STRB     = 3,  // strb rt, [rn, #offset]
    
    // 分支
    ARM64_B        = 0,  // b label
    ARM64_BL       = 1,  // bl label
    ARM64_BR       = 2,  // br rn
    ARM64_BLR      = 3,  // blr rn
    ARM64_RET      = 4,  // ret
    ARM64_B_COND   = 5,  // b.cond label
    
    // 特殊
    ARM64_NOP      = 0,  // nop
    ARM64_MOVZ     = 1,  // movz rd, #imm, lsl #shift
    ARM64_MOVK     = 2,  // movk rd, #imm, lsl #shift
    ARM64_MOV_REG  = 3,  // mov rd, rm (实际上是orr rd, xzr, rm)
    ARM64_SVC      = 4,  // svc #imm
} ARM64OpInternal;

// 创建ARM64目标
SilverTarget *arm64_target_create(void);

// 销毁ARM64目标
void arm64_target_destroy(SilverTarget *target);

// ARM64指令编码
bool arm64_encode_instruction(const SilverTarget *target,
                               const MachineInstExt *inst,
                               uint8_t *buffer, uint32_t *length);

// ARM64函数序言
uint32_t arm64_emit_prologue(const SilverTarget *target,
                              IRFunction *func, uint8_t *buffer);

// ARM64函数尾声
uint32_t arm64_emit_epilogue(const SilverTarget *target,
                              IRFunction *func, uint8_t *buffer);

// ARM64匹配表
extern const MatchEntry arm64_match_table[];
extern const uint32_t arm64_match_table_size;

// ARM64寄存器映射（通用寄存器到ARM64硬件编码）
uint8_t arm64_reg_encoding(MachineRegister reg);
bool arm64_is_gpr(MachineRegister reg);
bool arm64_is_fpr(MachineRegister reg);

// ARM64条件码转换
ARM64Condition arm64_cond_from_machine(MachineCondition cond);

#ifdef __cplusplus
}
#endif

#endif // SILVER_TARGET_ARM_H