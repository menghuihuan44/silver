#ifndef SILVER_TARGET_ARM_ENCODE_H
#define SILVER_TARGET_ARM_ENCODE_H

#include "silver/target/arm/arm.h"
#include <stdint.h>

// ARM64编码器辅助函数
typedef struct {
    uint8_t *buffer;
    uint32_t capacity;
    uint32_t length;
} ARM64Encoder;

void arm64_encoder_init(ARM64Encoder *enc, uint8_t *buffer, uint32_t capacity);
bool arm64_emit_word(ARM64Encoder *enc, uint32_t word);

// ARM64指令编码函数
bool arm64_encode_add_imm(ARM64Encoder *enc, uint8_t sf, uint8_t rd, 
                          uint8_t rn, uint32_t imm, bool set_flags);
bool arm64_encode_sub_imm(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint32_t imm, bool set_flags);
bool arm64_encode_add_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm);
bool arm64_encode_sub_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm);
bool arm64_encode_and_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm);
bool arm64_encode_orr_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm);
bool arm64_encode_eor_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm);
bool arm64_encode_lsl_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm);
bool arm64_encode_lsr_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm);
bool arm64_encode_asr_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm);
bool arm64_encode_movz(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                       uint16_t imm, uint8_t shift);
bool arm64_encode_movk(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                       uint16_t imm, uint8_t shift);
bool arm64_encode_ldr(ARM64Encoder *enc, uint8_t sf, uint8_t rt,
                      uint8_t rn, int32_t offset);
bool arm64_encode_str(ARM64Encoder *enc, uint8_t sf, uint8_t rt,
                      uint8_t rn, int32_t offset);
bool arm64_encode_ldrb(ARM64Encoder *enc, uint8_t rt, uint8_t rn, int32_t offset);
bool arm64_encode_strb(ARM64Encoder *enc, uint8_t rt, uint8_t rn, int32_t offset);
bool arm64_encode_ldp(ARM64Encoder *enc, uint8_t sf, uint8_t rt, uint8_t rt2,
                      uint8_t rn, int32_t offset);
bool arm64_encode_stp(ARM64Encoder *enc, uint8_t sf, uint8_t rt, uint8_t rt2,
                      uint8_t rn, int32_t offset);
bool arm64_encode_b(ARM64Encoder *enc, int32_t offset);
bool arm64_encode_bl(ARM64Encoder *enc, int32_t offset);
bool arm64_encode_br(ARM64Encoder *enc, uint8_t rn);
bool arm64_encode_blr(ARM64Encoder *enc, uint8_t rn);
bool arm64_encode_ret(ARM64Encoder *enc, uint8_t rn);
bool arm64_encode_b_cond(ARM64Encoder *enc, ARM64Condition cond, int32_t offset);
bool arm64_encode_cbz(ARM64Encoder *enc, uint8_t sf, uint8_t rt, int32_t offset);
bool arm64_encode_cbnz(ARM64Encoder *enc, uint8_t sf, uint8_t rt, int32_t offset);
bool arm64_encode_madd(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                       uint8_t rn, uint8_t rm, uint8_t ra);
bool arm64_encode_sdiv(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                       uint8_t rn, uint8_t rm);
bool arm64_encode_udiv(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                       uint8_t rn, uint8_t rm);
bool arm64_encode_nop(ARM64Encoder *enc);
bool arm64_encode_svc(ARM64Encoder *enc, uint16_t imm);
bool arm64_encode_fadd(ARM64Encoder *enc, uint8_t sz, uint8_t rd, 
                       uint8_t rn, uint8_t rm);
bool arm64_encode_fsub(ARM64Encoder *enc, uint8_t sz, uint8_t rd,
                       uint8_t rn, uint8_t rm);
bool arm64_encode_fmul(ARM64Encoder *enc, uint8_t sz, uint8_t rd,
                       uint8_t rn, uint8_t rm);
bool arm64_encode_fdiv(ARM64Encoder *enc, uint8_t sz, uint8_t rd,
                       uint8_t rn, uint8_t rm);
bool arm64_encode_scvtf(ARM64Encoder *enc, uint8_t sz, uint8_t rd, uint8_t rn);
bool arm64_encode_fcvtzs(ARM64Encoder *enc, uint8_t sz, uint8_t rd, uint8_t rn);
bool arm64_encode_fcmp(ARM64Encoder *enc, uint8_t sz, uint8_t rn, uint8_t rm);
bool arm64_encode_ldr_float(ARM64Encoder *enc, uint8_t sz, uint8_t rt,
                             uint8_t rn, int32_t offset);
bool arm64_encode_str_float(ARM64Encoder *enc, uint8_t sz, uint8_t rt,
                             uint8_t rn, int32_t offset);

#endif // SILVER_TARGET_ARM_ENCODE_H