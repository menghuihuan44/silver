#ifndef SILVER_TARGET_RISCV_H
#define SILVER_TARGET_RISCV_H

#include "silver/codegen/machine.h"
#include "silver/codegen/isel.h"
#include "silver/ir/ir_module.h"

#ifdef __cplusplus
extern "C" {
#endif

// RISC-V64寄存器定义
// RISC-V有32个通用寄存器（X0-X31）和32个浮点寄存器（F0-F31）
typedef enum {
    RISCV_REG_X0  = 0,   // 零寄存器
    RISCV_REG_X1  = 1,   // RA (返回地址)
    RISCV_REG_X2  = 2,   // SP (栈指针)
    RISCV_REG_X3  = 3,   // GP (全局指针)
    RISCV_REG_X4  = 4,   // TP (线程指针)
    RISCV_REG_X5  = 5,   // T0 (临时)
    RISCV_REG_X6  = 6,   // T1 (临时)
    RISCV_REG_X7  = 7,   // T2 (临时)
    RISCV_REG_X8  = 8,   // S0/FP (保存/帧指针)
    RISCV_REG_X9  = 9,   // S1 (保存)
    RISCV_REG_X10 = 10,  // A0 (参数/返回值)
    RISCV_REG_X11 = 11,  // A1 (参数/返回值)
    RISCV_REG_X12 = 12,  // A2 (参数)
    RISCV_REG_X13 = 13,  // A3 (参数)
    RISCV_REG_X14 = 14,  // A4 (参数)
    RISCV_REG_X15 = 15,  // A5 (参数)
    RISCV_REG_X16 = 16,  // A6 (参数)
    RISCV_REG_X17 = 17,  // A7 (参数)
    RISCV_REG_X18 = 18,  // S2 (保存)
    RISCV_REG_X19 = 19,  // S3 (保存)
    RISCV_REG_X20 = 20,  // S4 (保存)
    RISCV_REG_X21 = 21,  // S5 (保存)
    RISCV_REG_X22 = 22,  // S6 (保存)
    RISCV_REG_X23 = 23,  // S7 (保存)
    RISCV_REG_X24 = 24,  // S8 (保存)
    RISCV_REG_X25 = 25,  // S9 (保存)
    RISCV_REG_X26 = 26,  // S10 (保存)
    RISCV_REG_X27 = 27,  // S11 (保存)
    RISCV_REG_X28 = 28,  // T3 (临时)
    RISCV_REG_X29 = 29,  // T4 (临时)
    RISCV_REG_X30 = 30,  // T5 (临时)
    RISCV_REG_X31 = 31,  // T6 (临时)
    
    // 浮点寄存器 (映射到32-63)
    RISCV_REG_F0  = 32, RISCV_REG_F1  = 33,
    RISCV_REG_F2  = 34, RISCV_REG_F3  = 35,
    RISCV_REG_F4  = 36, RISCV_REG_F5  = 37,
    RISCV_REG_F6  = 38, RISCV_REG_F7  = 39,
    RISCV_REG_F8  = 40, RISCV_REG_F9  = 41,
    RISCV_REG_F10 = 42, RISCV_REG_F11 = 43,
    RISCV_REG_F12 = 44, RISCV_REG_F13 = 45,
    RISCV_REG_F14 = 46, RISCV_REG_F15 = 47,
    RISCV_REG_F16 = 48, RISCV_REG_F17 = 49,
    RISCV_REG_F18 = 50, RISCV_REG_F19 = 51,
    RISCV_REG_F20 = 52, RISCV_REG_F21 = 53,
    RISCV_REG_F22 = 54, RISCV_REG_F23 = 55,
    RISCV_REG_F24 = 56, RISCV_REG_F25 = 57,
    RISCV_REG_F26 = 58, RISCV_REG_F27 = 59,
    RISCV_REG_F28 = 60, RISCV_REG_F29 = 61,
    RISCV_REG_F30 = 62, RISCV_REG_F31 = 63,
    
    RISCV_REG_COUNT = 64
} RISC_VRegister;

// RISC-V指令格式（所有指令32位定长）

// R-type: funct7 | rs2 | rs1 | funct3 | rd | opcode
typedef struct {
    uint32_t opcode : 7;
    uint32_t rd     : 5;
    uint32_t funct3 : 3;
    uint32_t rs1    : 5;
    uint32_t rs2    : 5;
    uint32_t funct7 : 7;
} __attribute__((packed)) RISCVRType;

// I-type: imm[11:0] | rs1 | funct3 | rd | opcode
typedef struct {
    uint32_t opcode : 7;
    uint32_t rd     : 5;
    uint32_t funct3 : 3;
    uint32_t rs1    : 5;
    uint32_t imm    : 12;
} __attribute__((packed)) RISCVIType;

// S-type: imm[11:5] | rs2 | rs1 | funct3 | imm[4:0] | opcode
typedef struct {
    uint32_t opcode : 7;
    uint32_t imm_lo : 5;
    uint32_t funct3 : 3;
    uint32_t rs1    : 5;
    uint32_t rs2    : 5;
    uint32_t imm_hi : 7;
} __attribute__((packed)) RISCVSType;

// B-type: imm[12|10:5] | rs2 | rs1 | funct3 | imm[4:1|11] | opcode
typedef struct {
    uint32_t opcode : 7;
    uint32_t imm_11 : 1;
    uint32_t imm_4_1: 4;
    uint32_t funct3 : 3;
    uint32_t rs1    : 5;
    uint32_t rs2    : 5;
    uint32_t imm_10_5: 6;
    uint32_t imm_12 : 1;
} __attribute__((packed)) RISCVBType;

// U-type: imm[31:12] | rd | opcode
typedef struct {
    uint32_t opcode : 7;
    uint32_t rd     : 5;
    uint32_t imm    : 20;
} __attribute__((packed)) RISCVUType;

// J-type: imm[20|10:1|11|19:12] | rd | opcode
typedef struct {
    uint32_t opcode : 7;
    uint32_t rd     : 5;
    uint32_t imm_19_12 : 8;
    uint32_t imm_11  : 1;
    uint32_t imm_10_1 : 10;
    uint32_t imm_20  : 1;
} __attribute__((packed)) RISCVJType;

// RISC-V操作码
enum {
    RISCV_OP_LUI      = 0x37,  // 0110111
    RISCV_OP_AUIPC    = 0x17,  // 0010111
    RISCV_OP_JAL      = 0x6F,  // 1101111
    RISCV_OP_JALR     = 0x67,  // 1100111
    RISCV_OP_BRANCH   = 0x63,  // 1100011
    RISCV_OP_LOAD     = 0x03,  // 0000011
    RISCV_OP_STORE    = 0x23,  // 0100011
    RISCV_OP_ALU_IMM  = 0x13,  // 0010011
    RISCV_OP_ALU      = 0x33,  // 0110011
    RISCV_OP_FENCE    = 0x0F,  // 0001111
    RISCV_OP_SYSTEM   = 0x73,  // 1110011
    RISCV_OP_FP       = 0x53,  // 1010011 (浮点)
    RISCV_OP_AMO      = 0x2F,  // 0101111 (原子)
};

// RISC-V funct3编码
enum {
    RISCV_F3_ADD_SUB  = 0,  // ADD/SUB (funct7区分)
    RISCV_F3_SLL      = 1,
    RISCV_F3_SLT      = 2,
    RISCV_F3_SLTU     = 3,
    RISCV_F3_XOR      = 4,
    RISCV_F3_SRL_SRA  = 5,  // SRL/SRA (funct7区分)
    RISCV_F3_OR       = 6,
    RISCV_F3_AND      = 7,
    
    // 加载/存储
    RISCV_F3_BYTE     = 0,
    RISCV_F3_HALF     = 1,
    RISCV_F3_WORD     = 2,
    RISCV_F3_DOUBLE   = 3,
    RISCV_F3_BYTEU    = 4,
    RISCV_F3_HALFU    = 5,
    
    // 分支
    RISCV_F3_BEQ      = 0,
    RISCV_F3_BNE      = 1,
    RISCV_F3_BLT      = 4,
    RISCV_F3_BGE      = 5,
    RISCV_F3_BLTU     = 6,
    RISCV_F3_BGEU     = 7,
};

// RISC-V funct7编码
enum {
    RISCV_F7_ADD      = 0x00,
    RISCV_F7_SUB      = 0x20,
    RISCV_F7_SRL      = 0x00,
    RISCV_F7_SRA      = 0x20,
    RISCV_F7_MUL      = 0x01,
    RISCV_F7_DIV      = 0x01,  // 带funct3区分
    RISCV_F7_DIVU     = 0x01,
    RISCV_F7_REM      = 0x01,
    RISCV_F7_REMU     = 0x01,
};

// RISC-V乘除法funct3
enum {
    RISCV_F3_MUL      = 0,
    RISCV_F3_MULH     = 1,
    RISCV_F3_MULHSU   = 2,
    RISCV_F3_MULHU    = 3,
    RISCV_F3_DIV      = 4,
    RISCV_F3_DIVU     = 5,
    RISCV_F3_REM      = 6,
    RISCV_F3_REMU     = 7,
};

// 创建RISC-V64目标
SilverTarget *riscv64_target_create(void);

// 销毁RISC-V64目标
void riscv64_target_destroy(SilverTarget *target);

// RISC-V64指令编码
bool riscv64_encode_instruction(const SilverTarget *target,
                                 const MachineInstExt *inst,
                                 uint8_t *buffer, uint32_t *length);

// RISC-V64函数序言
uint32_t riscv64_emit_prologue(const SilverTarget *target,
                                IRFunction *func, uint8_t *buffer);

// RISC-V64函数尾声
uint32_t riscv64_emit_epilogue(const SilverTarget *target,
                                IRFunction *func, uint8_t *buffer);

// RISC-V64匹配表
extern const MatchEntry riscv64_match_table[];
extern const uint32_t riscv64_match_table_size;

// RISC-V寄存器映射
uint8_t riscv64_reg_encoding(MachineRegister reg);

#ifdef __cplusplus
}
#endif

#endif // SILVER_TARGET_RISCV_H