#include "silver/target/arm/arm_encode.h"
#include <string.h>

void arm64_encoder_init(ARM64Encoder *enc, uint8_t *buffer, uint32_t capacity) {
    if (!enc) return;
    enc->buffer = buffer;
    enc->capacity = capacity;
    enc->length = 0;
}

bool arm64_emit_word(ARM64Encoder *enc, uint32_t word) {
    if (!enc || enc->length + 4 > enc->capacity) return false;
    
    // ARM64是小端序
    enc->buffer[enc->length + 0] = (word >> 0)  & 0xFF;
    enc->buffer[enc->length + 1] = (word >> 8)  & 0xFF;
    enc->buffer[enc->length + 2] = (word >> 16) & 0xFF;
    enc->buffer[enc->length + 3] = (word >> 24) & 0xFF;
    enc->length += 4;
    
    return true;
}

// 编码12位立即数（ARM64支持特定的位模式）
static bool encode_imm12(uint64_t imm, uint8_t *imm12_out, uint8_t *shift_out) {
    if (imm < 4096) {
        *imm12_out = (uint8_t)(imm & 0xFFF);
        *shift_out = 0;
        return true;
    }
    
    // 尝试找到有效的编码（12位立即数 + 左移）
    uint64_t mask = 0xFFFF;
    for (int shift = 0; shift <= 48; shift += 16) {
        if (imm == (mask << shift)) {
            *imm12_out = (uint8_t)(mask & 0xFFF);
            *shift_out = shift;
            return true;
        }
    }
    
    // 更复杂的位模式匹配
    // ARM64允许特定的位模式：连续的1或0
    for (int size = 2; size <= 64; size *= 2) {
        for (int pos = 0; pos < size; pos++) {
            uint64_t pattern = 0;
            for (int i = 0; i < size; i++) {
                pattern |= (1ULL << i);
            }
            uint64_t rotated = (pattern << pos) | (pattern >> (size - pos));
            if (imm == rotated && size == 32) {
                // 这个简化处理不完整，完整实现需要更多逻辑
                *imm12_out = (uint8_t)(imm & 0xFFF);
                *shift_out = 0;
                return true;
            }
        }
    }
    
    return false;
}

// ADD (immediate): add rd, rn, #imm
bool arm64_encode_add_imm(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint32_t imm, bool set_flags) {
    if (!enc) return false;
    
    uint8_t imm12, shift;
    if (!encode_imm12(imm, &imm12, &shift)) return false;
    
    uint32_t encoding = 0;
    // Bit 31: sf (0=32-bit, 1=64-bit)
    encoding |= (sf ? 1U : 0U) << 31;
    // Bits 30-29: op (00 for ADD)
    encoding |= 0U << 29;
    // Bit 28: S (set flags)
    encoding |= (set_flags ? 1U : 0U) << 28;
    // Bits 27-24: 10001 (ADD immediate)
    encoding |= 0x8U << 24;  // 1000
    encoding |= (shift ? 1U : 0U) << 24; // 10001 if shift, 10000 if no shift
    // Bits 23-22: shift
    encoding |= ((uint32_t)(shift / 16)) << 22;
    // Bits 21-10: imm12
    encoding |= (uint32_t)imm12 << 10;
    // Bits 9-5: Rn
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    // Bits 4-0: Rd
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// SUB (immediate): sub rd, rn, #imm
bool arm64_encode_sub_imm(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint32_t imm, bool set_flags) {
    if (!enc) return false;
    
    uint8_t imm12, shift;
    if (!encode_imm12(imm, &imm12, &shift)) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 1U : 0U) << 31;
    encoding |= 2U << 29;  // op=10 for SUB
    encoding |= (set_flags ? 1U : 0U) << 28;
    encoding |= 0x8U << 24;
    encoding |= (shift ? 1U : 0U) << 24;
    encoding |= ((uint32_t)(shift / 16)) << 22;
    encoding |= (uint32_t)imm12 << 10;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// ADD (register): add rd, rn, rm
bool arm64_encode_add_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 1U : 0U) << 31;
    encoding |= 0x0B << 24;  // 01011
    encoding |= 0U << 22;     // shift=0
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= 0U << 10;     // imm6=0
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// SUB (register): sub rd, rn, rm
bool arm64_encode_sub_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 1U : 0U) << 31;
    encoding |= 0x0B << 24;  // 01011
    encoding |= 1U << 22;     // shift=1 (SUB)
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= 0U << 10;     // imm6=0
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// AND (register): and rd, rn, rm
bool arm64_encode_and_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 1U : 0U) << 31;
    encoding |= 0x0A << 24;  // 01010
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// ORR (register): orr rd, rn, rm
bool arm64_encode_orr_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 1U : 0U) << 31;
    encoding |= 0x2A << 23;  // 101010
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// EOR (register): eor rd, rn, rm
bool arm64_encode_eor_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 1U : 0U) << 31;
    encoding |= 0x2A << 23;  // 101010
    encoding |= 1U << 21;     // EOR bit
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// LSL (register): lsl rd, rn, rm
bool arm64_encode_lsl_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 1U : 0U) << 31;
    encoding |= 0x1A << 23;  // 11010
    encoding |= 8U << 16;     // LSL op
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// LSR (register): lsr rd, rn, rm
bool arm64_encode_lsr_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 1U : 0U) << 31;
    encoding |= 0x1A << 23;  // 11010
    encoding |= 9U << 16;     // LSR op
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// ASR (register): asr rd, rn, rm
bool arm64_encode_asr_reg(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                          uint8_t rn, uint8_t rm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 1U : 0U) << 31;
    encoding |= 0x1A << 23;  // 11010
    encoding |= 10U << 16;    // ASR op
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// MOVZ: movz rd, #imm, lsl #shift
bool arm64_encode_movz(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                       uint16_t imm, uint8_t shift) {
    if (!enc) return false;
    if (shift != 0 && shift != 16 && shift != 32 && shift != 48) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 1U : 0U) << 31;
    encoding |= 0x2A5U << 20;  // 10100101 (MOVZ)
    encoding |= ((uint32_t)(shift / 16)) << 21;
    encoding |= (uint32_t)imm << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// MOVK: movk rd, #imm, lsl #shift
bool arm64_encode_movk(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                       uint16_t imm, uint8_t shift) {
    if (!enc) return false;
    if (shift != 0 && shift != 16 && shift != 32 && shift != 48) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 1U : 0U) << 31;
    encoding |= 0x3A5U << 20;  // 11100101 (MOVK)
    encoding |= ((uint32_t)(shift / 16)) << 21;
    encoding |= (uint32_t)imm << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// LDR: ldr rt, [rn, #offset]
bool arm64_encode_ldr(ARM64Encoder *enc, uint8_t sf, uint8_t rt,
                      uint8_t rn, int32_t offset) {
    if (!enc) return false;
    
    int32_t scaled_offset = sf ? (offset >> 3) : (offset >> 2);
    if (scaled_offset < 0 || scaled_offset > 4095) return false;
    
    uint32_t encoding = 0;
    // size[1:0]: sf=1 -> 0b11, sf=0 -> 0b10
    encoding |= (uint32_t)(sf ? 3 : 2) << 30;
    // opc[1:0]=01 for LDR
    encoding |= 1U << 22;
    // imm12
    encoding |= (uint32_t)(scaled_offset & 0xFFF) << 10;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rt & 0x1F);
    
    return arm64_emit_word(enc, encoding); 
}

// STR: str rt, [rn, #offset]
bool arm64_encode_str(ARM64Encoder *enc, uint8_t sf, uint8_t rt,
                      uint8_t rn, int32_t offset) {
    if (!enc) return false;
    
    int32_t scaled_offset = sf ? (offset >> 3) : (offset >> 2);
    if (scaled_offset < 0 || scaled_offset > 4095) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)(sf ? 3 : 2) << 30;
    // opc[1:0]=00 for STR
    encoding |= 0U << 22;
    encoding |= (uint32_t)(scaled_offset & 0xFFF) << 10;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rt & 0x1F);
    
    return arm64_emit_word(enc, encoding); 
}

// LDRB: ldrb rt, [rn, #offset]
bool arm64_encode_ldrb(ARM64Encoder *enc, uint8_t rt, uint8_t rn, int32_t offset) {
    if (!enc) return false;
    if (offset < 0 || offset > 4095) return false;
    
    uint32_t encoding = 0;
    encoding |= 0x39U << 24;  // 00111001
    encoding |= (uint32_t)offset << 10;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rt & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// STRB: strb rt, [rn, #offset]
bool arm64_encode_strb(ARM64Encoder *enc, uint8_t rt, uint8_t rn, int32_t offset) {
    if (!enc) return false;
    if (offset < 0 || offset > 4095) return false;
    
    uint32_t encoding = 0;
    encoding |= 0x39U << 24;  // 00111001
    encoding |= 0U << 22;      // STR
    encoding |= (uint32_t)offset << 10;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rt & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// LDP: ldp rt, rt2, [rn, #offset]
bool arm64_encode_ldp(ARM64Encoder *enc, uint8_t sf, uint8_t rt, uint8_t rt2,
                      uint8_t rn, int32_t offset) {
    if (!enc) return false;
    
    int32_t scaled_offset = offset >> (sf ? 3 : 2);
    if (scaled_offset < -64 || scaled_offset > 63) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 2U : 0U) << 30;  // size
    encoding |= 0xA9U << 23;  // 10101001
    encoding |= ((uint32_t)scaled_offset & 0x7F) << 15;
    encoding |= (uint32_t)(rt2 & 0x1F) << 10;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rt & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// STP: stp rt, rt2, [rn, #offset]
bool arm64_encode_stp(ARM64Encoder *enc, uint8_t sf, uint8_t rt, uint8_t rt2,
                      uint8_t rn, int32_t offset) {
    if (!enc) return false;
    
    int32_t scaled_offset = offset >> (sf ? 3 : 2);
    if (scaled_offset < -64 || scaled_offset > 63) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 2U : 0U) << 30;
    encoding |= 0xA9U << 23;
    encoding |= 0U << 22;      // STP
    encoding |= ((uint32_t)scaled_offset & 0x7F) << 15;
    encoding |= (uint32_t)(rt2 & 0x1F) << 10;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rt & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// B: b offset
bool arm64_encode_b(ARM64Encoder *enc, int32_t offset) {
    if (!enc) return false;
    
    // PC相对偏移，26位有符号，按4字节对齐
    int32_t imm26 = offset >> 2;
    if (imm26 < -0x2000000 || imm26 > 0x1FFFFFF) return false;
    
    uint32_t encoding = 0;
    encoding |= 0x05U << 26;  // 000101
    encoding |= (uint32_t)(imm26 & 0x3FFFFFF);
    
    return arm64_emit_word(enc, encoding);
}

// BL: bl offset
bool arm64_encode_bl(ARM64Encoder *enc, int32_t offset) {
    if (!enc) return false;
    
    int32_t imm26 = offset >> 2;
    if (imm26 < -0x2000000 || imm26 > 0x1FFFFFF) return false;
    
    uint32_t encoding = 0;
    encoding |= 0x25U << 26;  // 100101
    encoding |= (uint32_t)(imm26 & 0x3FFFFFF);
    
    return arm64_emit_word(enc, encoding);
}

// BR: br rn
bool arm64_encode_br(ARM64Encoder *enc, uint8_t rn) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= 0xD61F0000;  // BR base
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    
    return arm64_emit_word(enc, encoding);
}

// BLR: blr rn
bool arm64_encode_blr(ARM64Encoder *enc, uint8_t rn) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= 0xD63F0000;  // BLR base
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    
    return arm64_emit_word(enc, encoding);
}

// RET: ret {rn} (default X30/LR)
bool arm64_encode_ret(ARM64Encoder *enc, uint8_t rn) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= 0xD65F0000;  // RET base
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    
    return arm64_emit_word(enc, encoding);
}

// B.cond: b.cond offset
bool arm64_encode_b_cond(ARM64Encoder *enc, ARM64Condition cond, int32_t offset) {
    if (!enc) return false;
    
    int32_t imm19 = offset >> 2;
    if (imm19 < -0x40000 || imm19 > 0x3FFFF) return false;
    
    uint32_t encoding = 0;
    encoding |= 0x54U << 24;  // 01010100
    encoding |= (uint32_t)(imm19 & 0x7FFFF) << 5;
    encoding |= (uint32_t)(cond & 0xF);
    
    return arm64_emit_word(enc, encoding);
}

// CBZ: cbz rt, offset
bool arm64_encode_cbz(ARM64Encoder *enc, uint8_t sf, uint8_t rt, int32_t offset) {
    if (!enc) return false;
    
    int32_t imm19 = offset >> 2;
    if (imm19 < -0x40000 || imm19 > 0x3FFFF) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 0xB4U : 0x34U) << 24;  // CBZ
    encoding |= (uint32_t)(imm19 & 0x7FFFF) << 5;
    encoding |= (uint32_t)(rt & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// CBNZ: cbnz rt, offset
bool arm64_encode_cbnz(ARM64Encoder *enc, uint8_t sf, uint8_t rt, int32_t offset) {
    if (!enc) return false;
    
    int32_t imm19 = offset >> 2;
    if (imm19 < -0x40000 || imm19 > 0x3FFFF) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 0xB5U : 0x35U) << 24;  // CBNZ
    encoding |= (uint32_t)(imm19 & 0x7FFFF) << 5;
    encoding |= (uint32_t)(rt & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// MADD: madd rd, rn, rm, ra  (rd = rn * rm + ra)
bool arm64_encode_madd(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                       uint8_t rn, uint8_t rm, uint8_t ra) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 0x9B : 0x1B) << 24;  // MADD
    encoding |= 0U << 15;  // MADD=0, MSUB=1
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= (uint32_t)(ra & 0x1F) << 10;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// SDIV: sdiv rd, rn, rm
bool arm64_encode_sdiv(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                       uint8_t rn, uint8_t rm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 0x9B : 0x1B) << 24;  // SDIV
    encoding |= 0x0C3U << 10;  // SDIV op
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// UDIV: udiv rd, rn, rm
bool arm64_encode_udiv(ARM64Encoder *enc, uint8_t sf, uint8_t rd,
                       uint8_t rn, uint8_t rm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sf ? 0x9B : 0x1B) << 24;  // UDIV
    encoding |= 0x083U << 10;  // UDIV op
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// NOP: nop
bool arm64_encode_nop(ARM64Encoder *enc) {
    if (!enc) return false;
    return arm64_emit_word(enc, 0xD503201F);  // NOP
}

// SVC: svc #imm (system call)
bool arm64_encode_svc(ARM64Encoder *enc, uint16_t imm) {
    if (!enc) return false;
    if (imm > 0xFFFF) return false;
    
    uint32_t encoding = 0;
    encoding |= 0xD4000001;  // SVC base
    encoding |= (uint32_t)imm << 5;
    
    return arm64_emit_word(enc, encoding);
}

// ARM64浮点指令
bool arm64_encode_fadd(ARM64Encoder *enc, uint8_t sz, uint8_t rd, 
                       uint8_t rn, uint8_t rm) {
    if (!enc) return false;
    
    // FADD Dd, Dn, Dm  (双精度)
    // FADD Sd, Sn, Sm  (单精度)
    uint32_t encoding = 0;
    encoding |= (sz ? 0x1EU : 0x1CU) << 24;  // 单精度或双精度
    encoding |= 0x1U << 22;   // FADD
    encoding |= 0x1U << 21;   // type
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

bool arm64_encode_fsub(ARM64Encoder *enc, uint8_t sz, uint8_t rd,
                       uint8_t rn, uint8_t rm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sz ? 0x1EU : 0x1CU) << 24;
    encoding |= 0x1U << 22;
    encoding |= 0x3U << 21;   // FSUB type
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

bool arm64_encode_fmul(ARM64Encoder *enc, uint8_t sz, uint8_t rd,
                       uint8_t rn, uint8_t rm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sz ? 0x1EU : 0x1CU) << 24;
    encoding |= 0x1U << 23;   // FMUL
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

bool arm64_encode_fdiv(ARM64Encoder *enc, uint8_t sz, uint8_t rd,
                       uint8_t rn, uint8_t rm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sz ? 0x1EU : 0x1CU) << 24;
    encoding |= 0x1U << 23;
    encoding |= 0x1U << 22;   // FDIV
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// ARM64 浮点-整数转换
bool arm64_encode_scvtf(ARM64Encoder *enc, uint8_t sz, uint8_t rd, uint8_t rn) {
    // 有符号整数转浮点
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sz ? 0x9EU : 0x1CU) << 24;
    encoding |= 0x2U << 16;   // SCVTF
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

bool arm64_encode_fcvtzs(ARM64Encoder *enc, uint8_t sz, uint8_t rd, uint8_t rn) {
    // 浮点转有符号整数，向零舍入
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sz ? 0x9EU : 0x1CU) << 24;
    encoding |= 0x3U << 16;   // FCVTZS
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rd & 0x1F);
    
    return arm64_emit_word(enc, encoding);
}

// ARM64 浮点比较
bool arm64_encode_fcmp(ARM64Encoder *enc, uint8_t sz, uint8_t rn, uint8_t rm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (sz ? 0x1EU : 0x1CU) << 24;
    encoding |= 0x2U << 22;   // FCMP
    encoding |= (uint32_t)(rm & 0x1F) << 16;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    
    return arm64_emit_word(enc, encoding);
}

// ARM64 浮点加载/存储
bool arm64_encode_ldr_float(ARM64Encoder *enc, uint8_t sz, uint8_t rt,
                             uint8_t rn, int32_t offset) {
    if (!enc) return false;
    
    int32_t scaled_offset = sz ? (offset >> 3) : (offset >> 2);
    if (scaled_offset < 0 || scaled_offset > 4095) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)(sz ? 3 : 2) << 30;
    encoding |= 1U << 26;  // V=1
    encoding |= 1U << 22;  // LDR
    encoding |= (uint32_t)(scaled_offset & 0xFFF) << 10;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rt & 0x1F);
    
    return arm64_emit_word(enc, encoding); 
}

bool arm64_encode_str_float(ARM64Encoder *enc, uint8_t sz, uint8_t rt,
                             uint8_t rn, int32_t offset) {
    if (!enc) return false;
    
    int32_t scaled_offset = sz ? (offset >> 3) : (offset >> 2);
    if (scaled_offset < 0 || scaled_offset > 4095) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)(sz ? 3 : 2) << 30;
    encoding |= 1U << 26;  // V=1
    encoding |= 0U << 22;  // STR
    encoding |= (uint32_t)(scaled_offset & 0xFFF) << 10;
    encoding |= (uint32_t)(rn & 0x1F) << 5;
    encoding |= (uint32_t)(rt & 0x1F);
    
    return arm64_emit_word(enc, encoding); 
}