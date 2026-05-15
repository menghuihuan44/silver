#include "silver/target/x86/x86_encode.h"
#include <string.h>
#include <stdlib.h>

// REX前缀标志
#define REX_W(r, x, b) (0x48 | ((r) ? 0x04 : 0) | ((x) ? 0x02 : 0) | ((b) ? 0x01 : 0))
#define REX_R(r, x, b) (0x44 | ((r) ? 0x04 : 0) | ((x) ? 0x02 : 0) | ((b) ? 0x01 : 0))
#define REX_X(r, x, b) (0x42 | ((r) ? 0x04 : 0) | ((x) ? 0x02 : 0) | ((b) ? 0x01 : 0))
#define REX_B(r, x, b) (0x41 | ((r) ? 0x04 : 0) | ((x) ? 0x02 : 0) | ((b) ? 0x01 : 0))

typedef enum {
    ADDR_MODE_DIRECT,      // [reg]
    ADDR_MODE_DISP8,       // [reg + disp8]
    ADDR_MODE_DISP32,      // [reg + disp32]
    ADDR_MODE_SIB,         // [base + index*scale]
    ADDR_MODE_SIB_DISP8,   // [base + index*scale + disp8]
    ADDR_MODE_SIB_DISP32,  // [base + index*scale + disp32]
    ADDR_MODE_RIP_REL,     // [rip + disp32]
} AddressMode;

// 分析内存地址并确定寻址模式
static AddressMode analyze_address(MachineRegister base, 
                                    MachineRegister index,
                                    uint8_t scale, int32_t disp) {
    if (base == REG_RIP) {
        return ADDR_MODE_RIP_REL;
    }
    
    if (index != REG_NONE && scale > 0) {
        if (disp == 0) return ADDR_MODE_SIB;
        if (disp >= -128 && disp <= 127) return ADDR_MODE_SIB_DISP8;
        return ADDR_MODE_SIB_DISP32;
    }
    
    // 检查是否必须使用SIB（RSP/R12作为基址）
    if (base == REG_RSP || base == REG_R12) {
        if (disp == 0) return ADDR_MODE_SIB;
        if (disp >= -128 && disp <= 127) return ADDR_MODE_SIB_DISP8;
        return ADDR_MODE_SIB_DISP32;
    }
    
    // 检查RBP/R13特殊情况（无偏移时必须有disp8=0）
    if (base == REG_RBP || base == REG_R13) {
        if (disp >= -128 && disp <= 127) return ADDR_MODE_DISP8;
        return ADDR_MODE_DISP32;
    }
    
    if (disp == 0) return ADDR_MODE_DIRECT;
    if (disp >= -128 && disp <= 127) return ADDR_MODE_DISP8;
    return ADDR_MODE_DISP32;
}

// 编码ModRM和可选的SIB以及偏移
static bool encode_modrm_sib_disp(X86Encoder *enc, AddressMode mode,
                                   uint8_t reg_opcode, MachineRegister base,
                                   MachineRegister index, uint8_t scale,
                                   int32_t disp) {
    uint8_t base_enc = x86_reg_encoding(base);
    uint8_t index_enc = (index != REG_NONE) ? x86_reg_encoding(index) : 4;
    
    switch (mode) {
        case ADDR_MODE_DIRECT:
            // ModRM: mod=0, reg, r/m=base
            x86_emit_modrm(enc, 0, reg_opcode, base_enc);
            // RBP不能使用mod=0，需要特殊处理
            if (base == REG_RBP || base == REG_R13) {
                // 实际上RBP在mod=0时表示[RIP+disp32]
                // 所以需要mod=1 + disp8=0
                x86_emit_imm8(enc, 0);
            }
            break;
        
        case ADDR_MODE_DISP8:
            x86_emit_modrm(enc, 1, reg_opcode, base_enc);
            x86_emit_imm8(enc, (uint8_t)disp);
            break;
        
        case ADDR_MODE_DISP32:
            x86_emit_modrm(enc, 2, reg_opcode, base_enc);
            x86_emit_imm32(enc, (uint32_t)disp);
            break;
        
        case ADDR_MODE_SIB:
            x86_emit_modrm(enc, 0, reg_opcode, 4);  // r/m=4 表示SIB
            x86_emit_sib(enc, scale, index_enc, base_enc);
            // RBP作为基址时的特殊处理
            if (base == REG_RBP || base == REG_R13) {
                x86_emit_imm8(enc, 0);
            }
            break;
        
        case ADDR_MODE_SIB_DISP8:
            x86_emit_modrm(enc, 1, reg_opcode, 4);
            x86_emit_sib(enc, scale, index_enc, base_enc);
            x86_emit_imm8(enc, (uint8_t)disp);
            break;
        
        case ADDR_MODE_SIB_DISP32:
            x86_emit_modrm(enc, 2, reg_opcode, 4);
            x86_emit_sib(enc, scale, index_enc, base_enc);
            x86_emit_imm32(enc, (uint32_t)disp);
            break;
        
        case ADDR_MODE_RIP_REL:
            // RIP相对寻址：mod=0, r/m=5
            x86_emit_modrm(enc, 0, reg_opcode, 5);
            x86_emit_imm32(enc, (uint32_t)disp);
            break;
    }
    
    return true;
}

void x86_encoder_init(X86Encoder *enc, uint8_t *buffer, uint32_t capacity) {
    if (!enc) return;
    enc->buffer = buffer;
    enc->capacity = capacity;
    enc->length = 0;
    enc->has_rex = false;
    enc->rex_byte = 0;
    enc->has_vex = false;
    enc->has_evex = false;
}

bool x86_emit_byte(X86Encoder *enc, uint8_t byte) {
    if (!enc || enc->length >= enc->capacity) return false;
    enc->buffer[enc->length++] = byte;
    return true;
}

bool x86_emit_bytes(X86Encoder *enc, const uint8_t *bytes, uint32_t len) {
    if (!enc || enc->length + len > enc->capacity) return false;
    memcpy(enc->buffer + enc->length, bytes, len);
    enc->length += len;
    return true;
}

// 获取寄存器的硬件编码
uint8_t x86_reg_encoding(MachineRegister reg) {
    switch (reg) {
        case REG_RAX: return 0;
        case REG_RCX: return 1;
        case REG_RDX: return 2;
        case REG_RBX: return 3;
        case REG_RSP: return 4;
        case REG_RBP: return 5;
        case REG_RSI: return 6;
        case REG_RDI: return 7;
        case REG_R8:  return 0;  // REX.R=1
        case REG_R9:  return 1;
        case REG_R10: return 2;
        case REG_R11: return 3;
        case REG_R12: return 4;
        case REG_R13: return 5;
        case REG_R14: return 6;
        case REG_R15: return 7;
        default: return 0;
    }
}

bool x86_is_extended_reg(MachineRegister reg) {
    return reg >= REG_R8 && reg <= REG_R15;
}

uint8_t x86_rex_b_for_reg(MachineRegister reg) {
    return x86_is_extended_reg(reg) ? 1 : 0;
}

uint8_t x86_rex_r_for_reg(MachineRegister reg) {
    return x86_is_extended_reg(reg) ? 1 : 0;
}

bool x86_emit_rex(X86Encoder *enc, bool w, uint8_t r, uint8_t x, uint8_t b) {
    if (!enc) return false;
    
    // REX前缀只有在以下情况才需要发射：
    // 1. W=1（64位操作数）
    // 2. 任何扩展寄存器位为1
    if (!w && r == 0 && x == 0 && b == 0) {
        return true;  // 不需要REX
    }
    
    uint8_t rex = 0x40 | (w ? 0x08 : 0) | (r ? 0x04 : 0) | (x ? 0x02 : 0) | (b ? 0x01 : 0);
    return x86_emit_byte(enc, rex);
}

bool x86_emit_modrm(X86Encoder *enc, uint8_t mod, uint8_t reg, uint8_t rm) {
    if (!enc) return false;
    uint8_t modrm = ((mod & 0x03) << 6) | ((reg & 0x07) << 3) | (rm & 0x07);
    return x86_emit_byte(enc, modrm);
}

bool x86_emit_sib(X86Encoder *enc, uint8_t scale, uint8_t index, uint8_t base) {
    if (!enc) return false;
    uint8_t sib = ((scale & 0x03) << 6) | ((index & 0x07) << 3) | (base & 0x07);
    return x86_emit_byte(enc, sib);
}

bool x86_emit_imm8(X86Encoder *enc, uint8_t imm) {
    return x86_emit_byte(enc, imm);
}

bool x86_emit_imm16(X86Encoder *enc, uint16_t imm) {
    return x86_emit_bytes(enc, (uint8_t *)&imm, 2);
}

bool x86_emit_imm32(X86Encoder *enc, uint32_t imm) {
    return x86_emit_bytes(enc, (uint8_t *)&imm, 4);
}

bool x86_emit_imm64(X86Encoder *enc, uint64_t imm) {
    return x86_emit_bytes(enc, (uint8_t *)&imm, 8);
}

// MOV r64, r64
bool x86_encode_mov_rr(X86Encoder *enc, MachineRegister dst, MachineRegister src) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(dst);
    uint8_t rex_r = x86_rex_r_for_reg(src);
    uint8_t dst_enc = x86_reg_encoding(dst) & 0x07;
    uint8_t src_enc = x86_reg_encoding(src) & 0x07;
    
    // REX.W + REX.R + REX.B
    x86_emit_rex(enc, true, rex_r, 0, rex_b);
    x86_emit_byte(enc, X86_MOV_RR);  // 0x89
    x86_emit_modrm(enc, X86_MOD_REG, src_enc, dst_enc);
    
    return true;
}

// MOV r64, imm64
bool x86_encode_mov_ri(X86Encoder *enc, MachineRegister dst, uint64_t imm, bool is_64bit) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(dst);
    uint8_t dst_enc = x86_reg_encoding(dst);
    
    if (is_64bit) {
        // MOV r64, imm64
        x86_emit_rex(enc, true, 0, 0, rex_b);
        x86_emit_byte(enc, X86_MOV_RI + dst_enc);  // 0xB8 + reg
        x86_emit_imm64(enc, imm);
    } else {
        // MOV r32, imm32 (零扩展)
        x86_emit_rex(enc, false, 0, 0, rex_b);
        x86_emit_byte(enc, X86_MOV_RI + dst_enc);
        x86_emit_imm32(enc, (uint32_t)imm);
    }
    
    return true;
}

// ADD r64, r64
bool x86_encode_add_rr(X86Encoder *enc, MachineRegister dst, MachineRegister src) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(dst);
    uint8_t rex_r = x86_rex_r_for_reg(src);
    uint8_t dst_enc = x86_reg_encoding(dst);
    uint8_t src_enc = x86_reg_encoding(src);
    
    x86_emit_rex(enc, true, rex_r, 0, rex_b);
    x86_emit_byte(enc, X86_ADD_RM);  // 0x03
    x86_emit_modrm(enc, X86_MOD_REG, src_enc, dst_enc);
    
    return true;
}

// ADD r64, imm32
bool x86_encode_add_ri(X86Encoder *enc, MachineRegister dst, int32_t imm) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(dst);
    uint8_t dst_enc = x86_reg_encoding(dst);
    
    // 如果立即数适合8位，使用专门的操作码
    if (imm >= -128 && imm <= 127) {
        x86_emit_rex(enc, true, 0, 0, rex_b);
        x86_emit_byte(enc, 0x83);  // ADD r/m64, imm8
        x86_emit_modrm(enc, X86_MOD_REG, 0, dst_enc);  // /0
        x86_emit_imm8(enc, (uint8_t)imm);
    } else {
        x86_emit_rex(enc, true, 0, 0, rex_b);
        x86_emit_byte(enc, X86_ADD_RI);  // 0x81
        x86_emit_modrm(enc, X86_MOD_REG, 0, dst_enc);  // /0
        x86_emit_imm32(enc, (uint32_t)imm);
    }
    
    return true;
}

// SUB r64, r64
bool x86_encode_sub_rr(X86Encoder *enc, MachineRegister dst, MachineRegister src) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(dst);
    uint8_t rex_r = x86_rex_r_for_reg(src);
    uint8_t dst_enc = x86_reg_encoding(dst);
    uint8_t src_enc = x86_reg_encoding(src);
    
    x86_emit_rex(enc, true, rex_r, 0, rex_b);
    x86_emit_byte(enc, X86_SUB_RM);  // 0x2B
    x86_emit_modrm(enc, X86_MOD_REG, src_enc, dst_enc);
    
    return true;
}

// SUB r64, imm32
bool x86_encode_sub_ri(X86Encoder *enc, MachineRegister dst, int32_t imm) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(dst);
    uint8_t dst_enc = x86_reg_encoding(dst);
    
    if (imm >= -128 && imm <= 127) {
        x86_emit_rex(enc, true, 0, 0, rex_b);
        x86_emit_byte(enc, 0x83);  // SUB r/m64, imm8
        x86_emit_modrm(enc, X86_MOD_REG, 5, dst_enc);  // /5
        x86_emit_imm8(enc, (uint8_t)imm);
    } else {
        x86_emit_rex(enc, true, 0, 0, rex_b);
        x86_emit_byte(enc, X86_SUB_RI);  // 0x81
        x86_emit_modrm(enc, X86_MOD_REG, 5, dst_enc);  // /5
        x86_emit_imm32(enc, (uint32_t)imm);
    }
    
    return true;
}

// IMUL r64, r64
bool x86_encode_imul_rr(X86Encoder *enc, MachineRegister dst, MachineRegister src) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(src);
    uint8_t rex_r = x86_rex_r_for_reg(dst);
    uint8_t dst_enc = x86_reg_encoding(dst);
    uint8_t src_enc = x86_reg_encoding(src);
    
    x86_emit_rex(enc, true, rex_r, 0, rex_b);
    x86_emit_byte(enc, X86_ESCAPE);        // 0x0F
    x86_emit_byte(enc, 0xAF);              // IMUL r, r/m
    x86_emit_modrm(enc, X86_MOD_REG, dst_enc, src_enc);
    
    return true;
}

// IDIV r64 (divides RDX:RAX by divisor)
bool x86_encode_idiv_r(X86Encoder *enc, MachineRegister divisor, bool is_64bit) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(divisor);
    uint8_t div_enc = x86_reg_encoding(divisor);
    
    x86_emit_rex(enc, is_64bit, 0, 0, rex_b);
    x86_emit_byte(enc, X86_IDIV_R);  // 0xF7
    x86_emit_modrm(enc, X86_MOD_REG, 7, div_enc);  // /7
    
    return true;
}

// AND r64, r64
bool x86_encode_and_rr(X86Encoder *enc, MachineRegister dst, MachineRegister src) {
    if (!enc) return false;
    
    x86_emit_rex(enc, true, x86_rex_r_for_reg(src), 0, x86_rex_b_for_reg(dst));
    x86_emit_byte(enc, X86_AND_RM);  // 0x23
    x86_emit_modrm(enc, X86_MOD_REG, x86_reg_encoding(src), x86_reg_encoding(dst));
    
    return true;
}

// OR r64, r64
bool x86_encode_or_rr(X86Encoder *enc, MachineRegister dst, MachineRegister src) {
    if (!enc) return false;
    
    x86_emit_rex(enc, true, x86_rex_r_for_reg(src), 0, x86_rex_b_for_reg(dst));
    x86_emit_byte(enc, X86_OR_RM);  // 0x0B
    x86_emit_modrm(enc, X86_MOD_REG, x86_reg_encoding(src), x86_reg_encoding(dst));
    
    return true;
}

// XOR r64, r64
bool x86_encode_xor_rr(X86Encoder *enc, MachineRegister dst, MachineRegister src) {
    if (!enc) return false;
    
    x86_emit_rex(enc, true, x86_rex_r_for_reg(src), 0, x86_rex_b_for_reg(dst));
    x86_emit_byte(enc, X86_XOR_RM);  // 0x33
    x86_emit_modrm(enc, X86_MOD_REG, x86_reg_encoding(src), x86_reg_encoding(dst));
    
    return true;
}

// SHL r64, imm8
bool x86_encode_shl_ri(X86Encoder *enc, MachineRegister dst, uint8_t imm) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(dst);
    uint8_t dst_enc = x86_reg_encoding(dst);
    
    if (imm == 1) {
        x86_emit_rex(enc, true, 0, 0, rex_b);
        x86_emit_byte(enc, 0xD1);  // SHL r/m64, 1
        x86_emit_modrm(enc, X86_MOD_REG, 4, dst_enc);  // /4
    } else {
        x86_emit_rex(enc, true, 0, 0, rex_b);
        x86_emit_byte(enc, X86_SHL_RI);  // 0xC1
        x86_emit_modrm(enc, X86_MOD_REG, 4, dst_enc);  // /4
        x86_emit_imm8(enc, imm);
    }
    
    return true;
}

// SHR r64, imm8
bool x86_encode_shr_ri(X86Encoder *enc, MachineRegister dst, uint8_t imm) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(dst);
    uint8_t dst_enc = x86_reg_encoding(dst);
    
    x86_emit_rex(enc, true, 0, 0, rex_b);
    x86_emit_byte(enc, X86_SHR_RI);  // 0xC1
    x86_emit_modrm(enc, X86_MOD_REG, 5, dst_enc);  // /5
    x86_emit_imm8(enc, imm);
    
    return true;
}

// SAR r64, imm8
bool x86_encode_sar_ri(X86Encoder *enc, MachineRegister dst, uint8_t imm) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(dst);
    uint8_t dst_enc = x86_reg_encoding(dst);
    
    x86_emit_rex(enc, true, 0, 0, rex_b);
    x86_emit_byte(enc, X86_SAR_RI);  // 0xC1
    x86_emit_modrm(enc, X86_MOD_REG, 7, dst_enc);  // /7
    x86_emit_imm8(enc, imm);
    
    return true;
}

// CMP r64, r64
bool x86_encode_cmp_rr(X86Encoder *enc, MachineRegister lhs, MachineRegister rhs) {
    if (!enc) return false;
    
    x86_emit_rex(enc, true, x86_rex_r_for_reg(rhs), 0, x86_rex_b_for_reg(lhs));
    x86_emit_byte(enc, X86_CMP_RM);  // 0x3B
    x86_emit_modrm(enc, X86_MOD_REG, x86_reg_encoding(rhs), x86_reg_encoding(lhs));
    
    return true;
}

// CMP r64, imm32
bool x86_encode_cmp_ri(X86Encoder *enc, MachineRegister lhs, int32_t imm) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(lhs);
    uint8_t lhs_enc = x86_reg_encoding(lhs);
    
    if (imm >= -128 && imm <= 127) {
        x86_emit_rex(enc, true, 0, 0, rex_b);
        x86_emit_byte(enc, 0x83);  // CMP r/m64, imm8
        x86_emit_modrm(enc, X86_MOD_REG, 7, lhs_enc);  // /7
        x86_emit_imm8(enc, (uint8_t)imm);
    } else {
        x86_emit_rex(enc, true, 0, 0, rex_b);
        x86_emit_byte(enc, X86_CMP_RI);  // 0x81
        x86_emit_modrm(enc, X86_MOD_REG, 7, lhs_enc);  // /7
        x86_emit_imm32(enc, (uint32_t)imm);
    }
    
    return true;
}

// TEST r64, r64
bool x86_encode_test_rr(X86Encoder *enc, MachineRegister lhs, MachineRegister rhs) {
    if (!enc) return false;
    
    x86_emit_rex(enc, true, x86_rex_r_for_reg(rhs), 0, x86_rex_b_for_reg(lhs));
    x86_emit_byte(enc, X86_TEST_RM);  // 0x85
    x86_emit_modrm(enc, X86_MOD_REG, x86_reg_encoding(rhs), x86_reg_encoding(lhs));
    
    return true;
}

// SETcc r8
bool x86_encode_setcc(X86Encoder *enc, MachineCondition cond, MachineRegister dst) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(dst);
    uint8_t dst_enc = x86_reg_encoding(dst);
    
    // SETcc opcodes: 0x90 - 0x9F
    static const uint8_t setcc_codes[] = {
        [COND_O]  = 0x90, [COND_NO] = 0x91,
        [COND_B]  = 0x92, [COND_AE] = 0x93,
        [COND_E]  = 0x94, [COND_NE] = 0x95,
        [COND_BE] = 0x96, [COND_A]  = 0x97,
        [COND_S]  = 0x98, [COND_NS] = 0x99,
        [COND_P]  = 0x9A, [COND_NP] = 0x9B,
        [COND_L]  = 0x9C, [COND_GE] = 0x9D,
        [COND_LE] = 0x9E, [COND_G]  = 0x9F,
    };
    
    x86_emit_rex(enc, false, 0, 0, rex_b);
    x86_emit_byte(enc, X86_SETCC);  // 0x0F
    x86_emit_byte(enc, setcc_codes[cond]);
    x86_emit_modrm(enc, X86_MOD_REG, 0, dst_enc);
    
    return true;
}

// JMP rel32
bool x86_encode_jmp_rel32(X86Encoder *enc, int32_t offset) {
    if (!enc) return false;
    
    // 检查是否可以用短跳转
    if (offset >= -128 && offset <= 127) {
        x86_emit_byte(enc, X86_JMP_REL8);  // 0xEB
        x86_emit_imm8(enc, (uint8_t)offset);
    } else {
        x86_emit_byte(enc, X86_JMP_REL32);  // 0xE9
        x86_emit_imm32(enc, (uint32_t)(offset - 5));  // 减去指令长度
    }
    
    return true;
}

// Jcc rel32
bool x86_encode_jcc_rel32(X86Encoder *enc, MachineCondition cond, int32_t offset) {
    if (!enc) return false;
    
    static const uint8_t jcc_short[] = {
        [COND_O]  = 0x70, [COND_NO] = 0x71,
        [COND_B]  = 0x72, [COND_AE] = 0x73,
        [COND_E]  = 0x74, [COND_NE] = 0x75,
        [COND_BE] = 0x76, [COND_A]  = 0x77,
        [COND_S]  = 0x78, [COND_NS] = 0x79,
        [COND_P]  = 0x7A, [COND_NP] = 0x7B,
        [COND_L]  = 0x7C, [COND_GE] = 0x7D,
        [COND_LE] = 0x7E, [COND_G]  = 0x7F,
    };
    
    static const uint8_t jcc_near[] = {
        [COND_O]  = 0x80, [COND_NO] = 0x81,
        [COND_B]  = 0x82, [COND_AE] = 0x83,
        [COND_E]  = 0x84, [COND_NE] = 0x85,
        [COND_BE] = 0x86, [COND_A]  = 0x87,
        [COND_S]  = 0x88, [COND_NS] = 0x89,
        [COND_P]  = 0x8A, [COND_NP] = 0x8B,
        [COND_L]  = 0x8C, [COND_GE] = 0x8D,
        [COND_LE] = 0x8E, [COND_G]  = 0x8F,
    };
    
    if (offset >= -128 && offset <= 127) {
        x86_emit_byte(enc, jcc_short[cond]);
        x86_emit_imm8(enc, (uint8_t)offset);
    } else {
        x86_emit_byte(enc, X86_JCC_REL32);  // 0x0F
        x86_emit_byte(enc, jcc_near[cond]);
        x86_emit_imm32(enc, (uint32_t)(offset - 6));  // 减去指令长度
    }
    
    return true;
}

// JMP r64
bool x86_encode_jmp_r(X86Encoder *enc, MachineRegister reg) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(reg);
    x86_emit_rex(enc, false, 0, 0, rex_b);
    x86_emit_byte(enc, X86_JMP_RM);  // 0xFF
    x86_emit_modrm(enc, X86_MOD_REG, 4, x86_reg_encoding(reg));  // /4
    
    return true;
}

// CALL rel32
bool x86_encode_call_rel32(X86Encoder *enc, int32_t offset) {
    if (!enc) return false;
    
    x86_emit_byte(enc, X86_CALL_REL32);  // 0xE8
    x86_emit_imm32(enc, (uint32_t)(offset - 5));  // 减去指令长度
    
    return true;
}

// CALL r64
bool x86_encode_call_r(X86Encoder *enc, MachineRegister reg) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(reg);
    x86_emit_rex(enc, false, 0, 0, rex_b);
    x86_emit_byte(enc, X86_CALL_RM);  // 0xFF
    x86_emit_modrm(enc, X86_MOD_REG, 2, x86_reg_encoding(reg));  // /2
    
    return true;
}

// RET
bool x86_encode_ret(X86Encoder *enc) {
    if (!enc) return false;
    return x86_emit_byte(enc, X86_RET);
}

// PUSH r64
bool x86_encode_push_r(X86Encoder *enc, MachineRegister reg) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(reg);
    x86_emit_rex(enc, false, 0, 0, rex_b);
    x86_emit_byte(enc, X86_PUSH_R + x86_reg_encoding(reg));
    
    return true;
}

// POP r64
bool x86_encode_pop_r(X86Encoder *enc, MachineRegister reg) {
    if (!enc) return false;
    
    uint8_t rex_b = x86_rex_b_for_reg(reg);
    x86_emit_rex(enc, false, 0, 0, rex_b);
    x86_emit_byte(enc, X86_POP_R + x86_reg_encoding(reg));
    
    return true;
}

// LEA r64, [base + index*scale + disp]
bool x86_encode_lea(X86Encoder *enc, MachineRegister dst, MachineRegister base,
                     MachineRegister index, uint8_t scale, int32_t disp) {
    if (!enc) return false;
    
    uint8_t rex_r = x86_rex_r_for_reg(dst);
    uint8_t rex_x = x86_rex_r_for_reg(index);
    uint8_t rex_b = x86_rex_b_for_reg(base);
    uint8_t dst_enc = x86_reg_encoding(dst);
    uint8_t base_enc = x86_reg_encoding(base);
    uint8_t index_enc = x86_reg_encoding(index);
    
    x86_emit_rex(enc, true, rex_r, rex_x, rex_b);
    x86_emit_byte(enc, X86_LEA);  // 0x8D
    
    if (disp == 0 && base != REG_RBP) {
        x86_emit_modrm(enc, X86_MOD_DISP0, dst_enc, 4);  // 需要SIB
        x86_emit_sib(enc, scale, index_enc, base_enc);
    } else if (disp >= -128 && disp <= 127) {
        x86_emit_modrm(enc, X86_MOD_DISP8, dst_enc, 4);  // 需要SIB
        x86_emit_sib(enc, scale, index_enc, base_enc);
        x86_emit_imm8(enc, (uint8_t)disp);
    } else {
        x86_emit_modrm(enc, X86_MOD_DISP32, dst_enc, 4);  // 需要SIB
        x86_emit_sib(enc, scale, index_enc, base_enc);
        x86_emit_imm32(enc, (uint32_t)disp);
    }
    
    return true;
}

// MOV r64, [base + offset]
bool x86_encode_mov_load(X86Encoder *enc, MachineRegister dst, MachineRegister base,
                          int32_t offset) {
    if (!enc) return false;
    
    uint8_t rex_r = x86_rex_r_for_reg(dst);
    uint8_t rex_b = x86_rex_b_for_reg(base);
    uint8_t dst_enc = x86_reg_encoding(dst);
    uint8_t base_enc = x86_reg_encoding(base);
    
    x86_emit_rex(enc, true, rex_r, 0, rex_b);
    x86_emit_byte(enc, X86_MOV_MR);  // 0x8B
    
    if (base == REG_RSP || base == REG_R12) {
        // RSP/R12需要SIB字节
        if (offset == 0) {
            x86_emit_modrm(enc, X86_MOD_DISP0, dst_enc, 4);
            x86_emit_sib(enc, 0, 4, 4);  // [rsp]
        } else if (offset >= -128 && offset <= 127) {
            x86_emit_modrm(enc, X86_MOD_DISP8, dst_enc, 4);
            x86_emit_sib(enc, 0, 4, 4);
            x86_emit_imm8(enc, (uint8_t)offset);
        } else {
            x86_emit_modrm(enc, X86_MOD_DISP32, dst_enc, 4);
            x86_emit_sib(enc, 0, 4, 4);
            x86_emit_imm32(enc, (uint32_t)offset);
        }
    } else if (offset == 0 && base != REG_RBP && base != REG_R13) {
        x86_emit_modrm(enc, X86_MOD_DISP0, dst_enc, base_enc);
    } else if (offset >= -128 && offset <= 127) {
        x86_emit_modrm(enc, X86_MOD_DISP8, dst_enc, base_enc);
        x86_emit_imm8(enc, (uint8_t)offset);
    } else {
        x86_emit_modrm(enc, X86_MOD_DISP32, dst_enc, base_enc);
        x86_emit_imm32(enc, (uint32_t)offset);
    }
    
    return true;
}

// MOV [base + offset], src
bool x86_encode_mov_store(X86Encoder *enc, MachineRegister src, MachineRegister base,
                           int32_t offset) {
    if (!enc) return false;
    
    uint8_t rex_r = x86_rex_r_for_reg(src);
    uint8_t rex_b = x86_rex_b_for_reg(base);
    uint8_t src_enc = x86_reg_encoding(src);
    uint8_t base_enc = x86_reg_encoding(base);
    
    x86_emit_rex(enc, true, rex_r, 0, rex_b);
    x86_emit_byte(enc, X86_MOV_RR);  // 0x89
    
    if (base == REG_RSP || base == REG_R12) {
        if (offset == 0) {
            x86_emit_modrm(enc, X86_MOD_DISP0, src_enc, 4);
            x86_emit_sib(enc, 0, 4, 4);
        } else if (offset >= -128 && offset <= 127) {
            x86_emit_modrm(enc, X86_MOD_DISP8, src_enc, 4);
            x86_emit_sib(enc, 0, 4, 4);
            x86_emit_imm8(enc, (uint8_t)offset);
        } else {
            x86_emit_modrm(enc, X86_MOD_DISP32, src_enc, 4);
            x86_emit_sib(enc, 0, 4, 4);
            x86_emit_imm32(enc, (uint32_t)offset);
        }
    } else if (offset == 0 && base != REG_RBP && base != REG_R13) {
        x86_emit_modrm(enc, X86_MOD_DISP0, src_enc, base_enc);
    } else if (offset >= -128 && offset <= 127) {
        x86_emit_modrm(enc, X86_MOD_DISP8, src_enc, base_enc);
        x86_emit_imm8(enc, (uint8_t)offset);
    } else {
        x86_emit_modrm(enc, X86_MOD_DISP32, src_enc, base_enc);
        x86_emit_imm32(enc, (uint32_t)offset);
    }
    
    return true;
}

bool x86_encode_mov_load_v2(X86Encoder *enc, MachineRegister dst,
                              MachineRegister base, MachineRegister index,
                              uint8_t scale, int32_t offset) {
    if (!enc) return false;
    
    uint8_t rex_r = x86_rex_r_for_reg(dst);
    uint8_t rex_b = x86_rex_b_for_reg(base);
    uint8_t rex_x = (index != REG_NONE) ? x86_rex_r_for_reg(index) : 0;
    uint8_t dst_enc = x86_reg_encoding(dst) & 0x07;
    
    AddressMode mode = analyze_address(base, index, scale, offset);
    
    x86_emit_rex(enc, true, rex_r, rex_x, rex_b);
    x86_emit_byte(enc, X86_MOV_MR);  // 0x8B
    encode_modrm_sib_disp(enc, mode, dst_enc, base, index, scale, offset);
    
    return true;
}

bool x86_encode_mov_store_v2(X86Encoder *enc, MachineRegister src,
                               MachineRegister base, MachineRegister index,
                               uint8_t scale, int32_t offset) {
    if (!enc) return false;
    
    uint8_t rex_r = x86_rex_r_for_reg(src);
    uint8_t rex_b = x86_rex_b_for_reg(base);
    uint8_t rex_x = (index != REG_NONE) ? x86_rex_r_for_reg(index) : 0;
    uint8_t src_enc = x86_reg_encoding(src) & 0x07;
    
    AddressMode mode = analyze_address(base, index, scale, offset);
    
    x86_emit_rex(enc, true, rex_r, rex_x, rex_b);
    x86_emit_byte(enc, X86_MOV_RR);  // 0x89
    encode_modrm_sib_disp(enc, mode, src_enc, base, index, scale, offset);
    
    return true;
}

// CDQ / CQO (sign-extend RAX into RDX:RAX)
bool x86_encode_cdq(X86Encoder *enc, bool is_64bit) {
    if (!enc) return false;
    
    if (is_64bit) {
        x86_emit_rex(enc, true, 0, 0, 0);  // REX.W
    }
    x86_emit_byte(enc, 0x99);  // CDQ/CQO
    
    return true;
}

// MOVSXD r64, r32 (sign-extend 32-bit to 64-bit)
bool x86_encode_movsxd(X86Encoder *enc, MachineRegister dst, MachineRegister src) {
    if (!enc) return false;
    
    uint8_t rex_r = x86_rex_r_for_reg(dst);
    uint8_t rex_b = x86_rex_b_for_reg(src);
    uint8_t dst_enc = x86_reg_encoding(dst);
    uint8_t src_enc = x86_reg_encoding(src);
    
    x86_emit_rex(enc, true, rex_r, 0, rex_b);
    x86_emit_byte(enc, X86_MOVSXD_OP);  // 0x63
    x86_emit_modrm(enc, X86_MOD_REG, dst_enc, src_enc);
    
    return true;
}

// 编码SSE双精度浮点操作
bool x86_encode_addsd(X86Encoder *enc, MachineRegister dst, MachineRegister src) {
    if (!enc) return false;
    
    uint8_t rex_r = x86_rex_r_for_reg(dst);
    uint8_t rex_b = x86_rex_b_for_reg(src);
    uint8_t dst_enc = x86_reg_encoding(dst) & 0x07;
    uint8_t src_enc = x86_reg_encoding(src) & 0x07;
    
    // F2前缀（SSE3风格，ADDSD使用F2）
    x86_emit_byte(enc, X86_SSE3_PREFIX);  // 0xF2
    x86_emit_rex(enc, false, rex_r, 0, rex_b);
    x86_emit_byte(enc, X86_ESCAPE);       // 0x0F
    x86_emit_byte(enc, X86_ADDSD);        // 0x58
    x86_emit_modrm(enc, X86_MOD_REG, dst_enc, src_enc);
    
    return true;
}

bool x86_encode_subsd(X86Encoder *enc, MachineRegister dst, MachineRegister src) {
    if (!enc) return false;
    
    uint8_t rex_r = x86_rex_r_for_reg(dst);
    uint8_t rex_b = x86_rex_b_for_reg(src);
    uint8_t dst_enc = x86_reg_encoding(dst) & 0x07;
    uint8_t src_enc = x86_reg_encoding(src) & 0x07;
    
    x86_emit_byte(enc, X86_SSE3_PREFIX);  // 0xF2
    x86_emit_rex(enc, false, rex_r, 0, rex_b);
    x86_emit_byte(enc, X86_ESCAPE);       // 0x0F
    x86_emit_byte(enc, X86_SUBSD);        // 0x5C
    x86_emit_modrm(enc, X86_MOD_REG, dst_enc, src_enc);
    
    return true;
}

bool x86_encode_mulsd(X86Encoder *enc, MachineRegister dst, MachineRegister src) {
    if (!enc) return false;
    
    uint8_t rex_r = x86_rex_r_for_reg(dst);
    uint8_t rex_b = x86_rex_b_for_reg(src);
    uint8_t dst_enc = x86_reg_encoding(dst) & 0x07;
    uint8_t src_enc = x86_reg_encoding(src) & 0x07;
    
    x86_emit_byte(enc, X86_SSE3_PREFIX);  // 0xF2
    x86_emit_rex(enc, false, rex_r, 0, rex_b);
    x86_emit_byte(enc, X86_ESCAPE);       // 0x0F
    x86_emit_byte(enc, X86_MULSD);        // 0x59
    x86_emit_modrm(enc, X86_MOD_REG, dst_enc, src_enc);
    
    return true;
}

bool x86_encode_divsd(X86Encoder *enc, MachineRegister dst, MachineRegister src) {
    if (!enc) return false;
    
    uint8_t rex_r = x86_rex_r_for_reg(dst);
    uint8_t rex_b = x86_rex_b_for_reg(src);
    uint8_t dst_enc = x86_reg_encoding(dst) & 0x07;
    uint8_t src_enc = x86_reg_encoding(src) & 0x07;
    
    x86_emit_byte(enc, X86_SSE3_PREFIX);  // 0xF2
    x86_emit_rex(enc, false, rex_r, 0, rex_b);
    x86_emit_byte(enc, X86_ESCAPE);       // 0x0F
    x86_emit_byte(enc, X86_DIVSD);        // 0x5E
    x86_emit_modrm(enc, X86_MOD_REG, dst_enc, src_enc);
    
    return true;
}

// 浮点比较
bool x86_encode_ucomisd(X86Encoder *enc, MachineRegister lhs, MachineRegister rhs) {
    if (!enc) return false;
    
    uint8_t rex_r = x86_rex_r_for_reg(lhs);
    uint8_t rex_b = x86_rex_b_for_reg(rhs);
    uint8_t lhs_enc = x86_reg_encoding(lhs) & 0x07;
    uint8_t rhs_enc = x86_reg_encoding(rhs) & 0x07;
    
    x86_emit_byte(enc, 0x66);            // SSE2前缀
    x86_emit_rex(enc, false, rex_r, 0, rex_b);
    x86_emit_byte(enc, X86_ESCAPE);      // 0x0F
    x86_emit_byte(enc, X86_UCOMISD);     // 0x2E
    x86_emit_modrm(enc, X86_MOD_REG, lhs_enc, rhs_enc);
    
    return true;
}

// 浮点到整数转换
bool x86_encode_cvtsd2si(X86Encoder *enc, MachineRegister dst, MachineRegister src) {
    if (!enc) return false;
    
    uint8_t rex_r = x86_rex_r_for_reg(dst);
    uint8_t rex_b = x86_rex_b_for_reg(src);
    uint8_t dst_enc = x86_reg_encoding(dst) & 0x07;
    uint8_t src_enc = x86_reg_encoding(src) & 0x07;
    
    x86_emit_byte(enc, X86_SSE3_PREFIX);  // 0xF2
    x86_emit_rex(enc, true, rex_r, 0, rex_b);  // REX.W for 64-bit
    x86_emit_byte(enc, X86_ESCAPE);       // 0x0F
    x86_emit_byte(enc, X86_CVTSD2SI);     // 0x2D
    x86_emit_modrm(enc, X86_MOD_REG, dst_enc, src_enc);
    
    return true;
}

// 整数到浮点转换
bool x86_encode_cvtsi2sd(X86Encoder *enc, MachineRegister dst, MachineRegister src) {
    if (!enc) return false;
    
    uint8_t rex_r = x86_rex_r_for_reg(dst);
    uint8_t rex_b = x86_rex_b_for_reg(src);
    uint8_t dst_enc = x86_reg_encoding(dst) & 0x07;
    uint8_t src_enc = x86_reg_encoding(src) & 0x07;
    
    x86_emit_byte(enc, X86_SSE3_PREFIX);  // 0xF2
    x86_emit_rex(enc, true, rex_r, 0, rex_b);  // REX.W
    x86_emit_byte(enc, X86_ESCAPE);       // 0x0F
    x86_emit_byte(enc, X86_CVTSI2SD);     // 0x2A
    x86_emit_modrm(enc, X86_MOD_REG, dst_enc, src_enc);
    
    return true;
}

// 浮点移动
bool x86_encode_movsd_load(X86Encoder *enc, MachineRegister dst, 
                            MachineRegister base, int32_t offset) {
    if (!enc) return false;
    
    uint8_t rex_r = x86_rex_r_for_reg(dst);
    uint8_t rex_b = x86_rex_b_for_reg(base);
    uint8_t dst_enc = x86_reg_encoding(dst) & 0x07;
    uint8_t base_enc = x86_reg_encoding(base) & 0x07;
    
    x86_emit_byte(enc, X86_SSE3_PREFIX);  // 0xF2
    x86_emit_rex(enc, false, rex_r, 0, rex_b);
    x86_emit_byte(enc, X86_ESCAPE);       // 0x0F
    x86_emit_byte(enc, 0x10);             // MOVSD xmm, m64
    
    // 内存寻址模式
    if (base == REG_RSP || base == REG_R12) {
        x86_emit_modrm(enc, X86_MOD_DISP8, dst_enc, 4);  // SIB
        x86_emit_sib(enc, 0, 4, 4);  // [rsp]
        x86_emit_imm8(enc, (uint8_t)offset);
    } else {
        x86_emit_modrm(enc, X86_MOD_DISP8, dst_enc, base_enc);
        x86_emit_imm8(enc, (uint8_t)offset);
    }
    
    return true;
}

bool x86_encode_movsd_store(X86Encoder *enc, MachineRegister src,
                             MachineRegister base, int32_t offset) {
    if (!enc) return false;
    
    uint8_t rex_r = x86_rex_r_for_reg(src);
    uint8_t rex_b = x86_rex_b_for_reg(base);
    uint8_t src_enc = x86_reg_encoding(src) & 0x07;
    uint8_t base_enc = x86_reg_encoding(base) & 0x07;
    
    x86_emit_byte(enc, X86_SSE3_PREFIX);  // 0xF2
    x86_emit_rex(enc, false, rex_r, 0, rex_b);
    x86_emit_byte(enc, X86_ESCAPE);       // 0x0F
    x86_emit_byte(enc, 0x11);             // MOVSD m64, xmm
    
    if (base == REG_RSP || base == REG_R12) {
        x86_emit_modrm(enc, X86_MOD_DISP8, src_enc, 4);
        x86_emit_sib(enc, 0, 4, 4);
        x86_emit_imm8(enc, (uint8_t)offset);
    } else {
        x86_emit_modrm(enc, X86_MOD_DISP8, src_enc, base_enc);
        x86_emit_imm8(enc, (uint8_t)offset);
    }
    
    return true;
}