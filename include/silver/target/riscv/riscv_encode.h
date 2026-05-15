#ifndef SILVER_TARGET_RISCV_ENCODE_H
#define SILVER_TARGET_RISCV_ENCODE_H

#include "silver/target/riscv/riscv.h"
#include <stdint.h>
#include <stdbool.h>

// RISC-V浮点操作码
#define RISCV_OP_FP_LOAD  0x07  // FLW, FLD
#define RISCV_OP_FP_STORE 0x27  // FSW, FSD
#define RISCV_OP_FP       0x53  // FADD, FSUB, FMUL, FDIV等
#define RISCV_OP_FP_FMADD 0x43  // FMADD
#define RISCV_OP_FP_FMSUB 0x47  // FMSUB

// RISC-V64编码器
typedef struct {
    uint8_t *buffer;
    uint32_t capacity;
    uint32_t length;
} RISCVEncoder;

void riscv_encoder_init(RISCVEncoder *enc, uint8_t *buffer, uint32_t capacity);
bool riscv_emit_word(RISCVEncoder *enc, uint32_t word);

// R-type指令
bool riscv_encode_rtype(RISCVEncoder *enc, uint8_t opcode, uint8_t rd, 
                        uint8_t funct3, uint8_t rs1, uint8_t rs2, uint8_t funct7);
bool riscv_encode_add(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_sub(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_sll(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_slt(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_sltu(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_xor(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_srl(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_sra(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_or(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_and(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_mul(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_div(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_divu(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_rem(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_remu(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);

// I-type指令
bool riscv_encode_itype(RISCVEncoder *enc, uint8_t opcode, uint8_t rd,
                        uint8_t funct3, uint8_t rs1, int32_t imm);
bool riscv_encode_addi(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t imm);
bool riscv_encode_slli(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t shamt);
bool riscv_encode_slti(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t imm);
bool riscv_encode_sltiu(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t imm);
bool riscv_encode_xori(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t imm);
bool riscv_encode_srli(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t shamt);
bool riscv_encode_srai(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t shamt);
bool riscv_encode_ori(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t imm);
bool riscv_encode_andi(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t imm);
bool riscv_encode_jalr(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset);

// 加载指令
bool riscv_encode_lb(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset);
bool riscv_encode_lh(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset);
bool riscv_encode_lw(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset);
bool riscv_encode_ld(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset);
bool riscv_encode_lbu(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset);
bool riscv_encode_lhu(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset);

// 存储指令
bool riscv_encode_sb(RISCVEncoder *enc, uint8_t rs2, uint8_t rs1, int32_t offset);
bool riscv_encode_sh(RISCVEncoder *enc, uint8_t rs2, uint8_t rs1, int32_t offset);
bool riscv_encode_sw(RISCVEncoder *enc, uint8_t rs2, uint8_t rs1, int32_t offset);
bool riscv_encode_sd(RISCVEncoder *enc, uint8_t rs2, uint8_t rs1, int32_t offset);

// 分支指令
bool riscv_encode_btype(RISCVEncoder *enc, uint8_t opcode, uint8_t funct3,
                        uint8_t rs1, uint8_t rs2, int32_t offset);
bool riscv_encode_beq(RISCVEncoder *enc, uint8_t rs1, uint8_t rs2, int32_t offset);
bool riscv_encode_bne(RISCVEncoder *enc, uint8_t rs1, uint8_t rs2, int32_t offset);
bool riscv_encode_blt(RISCVEncoder *enc, uint8_t rs1, uint8_t rs2, int32_t offset);
bool riscv_encode_bge(RISCVEncoder *enc, uint8_t rs1, uint8_t rs2, int32_t offset);
bool riscv_encode_bltu(RISCVEncoder *enc, uint8_t rs1, uint8_t rs2, int32_t offset);
bool riscv_encode_bgeu(RISCVEncoder *enc, uint8_t rs1, uint8_t rs2, int32_t offset);

// U-type指令
bool riscv_encode_lui(RISCVEncoder *enc, uint8_t rd, int32_t imm);
bool riscv_encode_auipc(RISCVEncoder *enc, uint8_t rd, int32_t imm);

// J-type指令
bool riscv_encode_jal(RISCVEncoder *enc, uint8_t rd, int32_t offset);

// 伪指令
bool riscv_encode_nop(RISCVEncoder *enc);
bool riscv_encode_mv(RISCVEncoder *enc, uint8_t rd, uint8_t rs);
bool riscv_encode_not(RISCVEncoder *enc, uint8_t rd, uint8_t rs);
bool riscv_encode_neg(RISCVEncoder *enc, uint8_t rd, uint8_t rs);
bool riscv_encode_li(RISCVEncoder *enc, uint8_t rd, int64_t imm);
bool riscv_encode_la(RISCVEncoder *enc, uint8_t rd, int64_t addr);
bool riscv_encode_call(RISCVEncoder *enc, int32_t offset);
bool riscv_encode_ret(RISCVEncoder *enc);
bool riscv_encode_j(RISCVEncoder *enc, int32_t offset);
bool riscv_encode_jr(RISCVEncoder *enc, uint8_t rs1);
bool riscv_encode_fadd_d(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_fsub_d(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_fmul_d(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_fdiv_d(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2);
bool riscv_encode_fld(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset);
bool riscv_encode_fsd(RISCVEncoder *enc, uint8_t rs2, uint8_t rs1, int32_t offset);
bool riscv_encode_fcvt_d_l(RISCVEncoder *enc, uint8_t rd, uint8_t rs1);
bool riscv_encode_fcvt_l_d(RISCVEncoder *enc, uint8_t rd, uint8_t rs1);

#endif // SILVER_TARGET_RISCV_ENCODE_H