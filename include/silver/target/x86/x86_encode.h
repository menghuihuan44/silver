#ifndef SILVER_TARGET_X86_ENCODE_H
#define SILVER_TARGET_X86_ENCODE_H

#include "silver/target/x86/x86.h"
#include <stdint.h>

// 浮点指令操作码
typedef enum {
    // SSE 双精度浮点
    X86_ADDSD    = 0x58,  // addsd xmm, xmm/m64
    X86_SUBSD    = 0x5C,  // subsd xmm, xmm/m64
    X86_MULSD    = 0x59,  // mulsd xmm, xmm/m64
    X86_DIVSD    = 0x5E,  // divsd xmm, xmm/m64
    X86_SQRTSD   = 0x51,  // sqrtsd xmm, xmm/m64
    
    // SSE 单精度浮点
    X86_ADDSS    = 0x58,  // addss xmm, xmm/m32
    X86_SUBSS    = 0x5C,  // subss xmm, xmm/m32
    X86_MULSS    = 0x59,  // mulss xmm, xmm/m32
    X86_DIVSS    = 0x5E,  // divss xmm, xmm/m32
    X86_SQRTSS   = 0x51,  // sqrtss xmm, xmm/m32
    
    // 比较指令
    X86_COMISD   = 0x2F,  // comisd xmm, xmm/m64
    X86_UCOMISD  = 0x2E,  // ucomisd xmm, xmm/m64
    
    // 转换指令
    X86_CVTSI2SD = 0x2A,  // cvtsi2sd xmm, r/m32
    X86_CVTSI2SS = 0x2A,  // cvtsi2ss xmm, r/m32
    X86_CVTSD2SI = 0x2D,  // cvtsd2si r32, xmm/m64
    X86_CVTSS2SI = 0x2D,  // cvtss2si r32, xmm/m32
    X86_CVTTSD2SI= 0x2C,  // cvttsd2si r32, xmm/m64
    X86_CVTTSS2SI= 0x2C,  // cvttss2si r32, xmm/m32
    
    // 移动指令
    X86_MOVSD    = 0x10,  // movsd xmm, xmm/m64 (以及反向0x11)
    X86_MOVSS    = 0x10,  // movss xmm, xmm/m32
    X86_MOVAPD   = 0x28,  // movapd xmm, xmm/m128
    X86_MOVDQA   = 0x6F,  // movdqa xmm, xmm/m128
    X86_MOVD     = 0x6E,  // movd xmm, r/m32
    
    // SSE指令前缀
    X86_SSE_PREFIX   = 0xF3,  // SSE前缀
    X86_SSE2_PREFIX  = 0x66,  // SSE2前缀
    X86_SSE3_PREFIX  = 0xF2,  // SSE3前缀
} X86SSEOpcode;

// x86-64编码器辅助函数
typedef struct {
    uint8_t *buffer;
    uint32_t capacity;
    uint32_t length;
    bool has_rex;
    uint8_t rex_byte;
    bool has_vex;
    bool has_evex;
} X86Encoder;

// 初始化编码器
void x86_encoder_init(X86Encoder *enc, uint8_t *buffer, uint32_t capacity);

// 发射字节
bool x86_emit_byte(X86Encoder *enc, uint8_t byte);

// 发射多个字节
bool x86_emit_bytes(X86Encoder *enc, const uint8_t *bytes, uint32_t len);

// 发射REX前缀（如果需要）
bool x86_emit_rex(X86Encoder *enc, bool w, uint8_t r, uint8_t x, uint8_t b);

// 发射ModRM字节
bool x86_emit_modrm(X86Encoder *enc, uint8_t mod, uint8_t reg, uint8_t rm);

// 发射SIB字节
bool x86_emit_sib(X86Encoder *enc, uint8_t scale, uint8_t index, uint8_t base);

// 发射立即数（可变长）
bool x86_emit_imm8(X86Encoder *enc, uint8_t imm);
bool x86_emit_imm16(X86Encoder *enc, uint16_t imm);
bool x86_emit_imm32(X86Encoder *enc, uint32_t imm);
bool x86_emit_imm64(X86Encoder *enc, uint64_t imm);

// 寄存器编码
uint8_t x86_reg_encoding(MachineRegister reg);
bool x86_is_extended_reg(MachineRegister reg);  // 是否需要REX
uint8_t x86_rex_b_for_reg(MachineRegister reg); // 获取REX.B位
uint8_t x86_rex_r_for_reg(MachineRegister reg); // 获取REX.R位

// 编码具体的x86指令
bool x86_encode_mov_rr(X86Encoder *enc, MachineRegister dst, MachineRegister src);
bool x86_encode_mov_ri(X86Encoder *enc, MachineRegister dst, uint64_t imm, bool is_64bit);
bool x86_encode_add_rr(X86Encoder *enc, MachineRegister dst, MachineRegister src);
bool x86_encode_add_ri(X86Encoder *enc, MachineRegister dst, int32_t imm);
bool x86_encode_sub_rr(X86Encoder *enc, MachineRegister dst, MachineRegister src);
bool x86_encode_sub_ri(X86Encoder *enc, MachineRegister dst, int32_t imm);
bool x86_encode_imul_rr(X86Encoder *enc, MachineRegister dst, MachineRegister src);
bool x86_encode_idiv_r(X86Encoder *enc, MachineRegister divisor, bool is_64bit);
bool x86_encode_and_rr(X86Encoder *enc, MachineRegister dst, MachineRegister src);
bool x86_encode_or_rr(X86Encoder *enc, MachineRegister dst, MachineRegister src);
bool x86_encode_xor_rr(X86Encoder *enc, MachineRegister dst, MachineRegister src);
bool x86_encode_shl_ri(X86Encoder *enc, MachineRegister dst, uint8_t imm);
bool x86_encode_shr_ri(X86Encoder *enc, MachineRegister dst, uint8_t imm);
bool x86_encode_sar_ri(X86Encoder *enc, MachineRegister dst, uint8_t imm);
bool x86_encode_cmp_rr(X86Encoder *enc, MachineRegister lhs, MachineRegister rhs);
bool x86_encode_cmp_ri(X86Encoder *enc, MachineRegister lhs, int32_t imm);
bool x86_encode_test_rr(X86Encoder *enc, MachineRegister lhs, MachineRegister rhs);
bool x86_encode_setcc(X86Encoder *enc, MachineCondition cond, MachineRegister dst);
bool x86_encode_jmp_rel32(X86Encoder *enc, int32_t offset);
bool x86_encode_jcc_rel32(X86Encoder *enc, MachineCondition cond, int32_t offset);
bool x86_encode_jmp_r(X86Encoder *enc, MachineRegister reg);
bool x86_encode_call_rel32(X86Encoder *enc, int32_t offset);
bool x86_encode_call_r(X86Encoder *enc, MachineRegister reg);
bool x86_encode_ret(X86Encoder *enc);
bool x86_encode_push_r(X86Encoder *enc, MachineRegister reg);
bool x86_encode_pop_r(X86Encoder *enc, MachineRegister reg);
bool x86_encode_lea(X86Encoder *enc, MachineRegister dst, MachineRegister base,
                     MachineRegister index, uint8_t scale, int32_t disp);
bool x86_encode_mov_load(X86Encoder *enc, MachineRegister dst, MachineRegister base,
                          int32_t offset);
bool x86_encode_mov_store(X86Encoder *enc, MachineRegister src, MachineRegister base,
                           int32_t offset);
bool x86_encode_cdq(X86Encoder *enc, bool is_64bit);
bool x86_encode_movsxd(X86Encoder *enc, MachineRegister dst, MachineRegister src);
bool x86_encode_addsd(X86Encoder *enc, MachineRegister dst, MachineRegister src);
bool x86_encode_subsd(X86Encoder *enc, MachineRegister dst, MachineRegister src);
bool x86_encode_mulsd(X86Encoder *enc, MachineRegister dst, MachineRegister src);
bool x86_encode_divsd(X86Encoder *enc, MachineRegister dst, MachineRegister src);
bool x86_encode_ucomisd(X86Encoder *enc, MachineRegister lhs, MachineRegister rhs);
bool x86_encode_cvtsd2si(X86Encoder *enc, MachineRegister dst, MachineRegister src);
bool x86_encode_cvtsi2sd(X86Encoder *enc, MachineRegister dst, MachineRegister src);
bool x86_encode_movsd_load(X86Encoder *enc, MachineRegister dst, 
                            MachineRegister base, int32_t offset);
bool x86_encode_movsd_store(X86Encoder *enc, MachineRegister src,
                             MachineRegister base, int32_t offset);

#endif // SILVER_TARGET_X86_ENCODE_H