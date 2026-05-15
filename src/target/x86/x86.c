#include "silver/target/x86/x86.h"
#include "silver/target/x86/x86_encode.h"
#include <stdlib.h>
#include <string.h>

// ============================================================
// REX/ModRM/SIB 快速编码辅助
// ============================================================
static inline uint8_t x86_make_rex(bool w, uint8_t r, uint8_t x, uint8_t b) {
    return 0x40 | (w ? 0x08 : 0) | (r ? 0x04 : 0) | (x ? 0x02 : 0) | (b ? 0x01 : 0);
}
static inline bool x86_need_rex(bool w, uint8_t r, uint8_t x, uint8_t b) {
    return w || r || x || b;
}
static inline void x86_emit_rex_fast(uint8_t **ptr, bool w, uint8_t r, uint8_t x, uint8_t b) {
    if (x86_need_rex(w, r, x, b)) *(*ptr)++ = x86_make_rex(w, r, x, b);
}
static inline void x86_emit_imm8_fast(uint8_t **ptr, uint8_t imm) { *(*ptr)++ = imm; }
static inline void x86_emit_imm32_fast(uint8_t **ptr, uint32_t imm) {
    (*ptr)[0] = (uint8_t)(imm & 0xFF); (*ptr)[1] = (uint8_t)((imm >> 8) & 0xFF);
    (*ptr)[2] = (uint8_t)((imm >> 16) & 0xFF); (*ptr)[3] = (uint8_t)((imm >> 24) & 0xFF);
    *ptr += 4;
}
static inline void x86_emit_imm64_fast(uint8_t **ptr, uint64_t imm) {
    x86_emit_imm32_fast(ptr, (uint32_t)(imm & 0xFFFFFFFF));
    x86_emit_imm32_fast(ptr, (uint32_t)(imm >> 32));
}

// ============================================================
// ModRM 和 SIB 预计算查找表
// ============================================================
static const uint8_t x86_modrm_lut[4][8][8] = {
    [0] = { // mod=0 (disp0 / [reg] / SIB)
        {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07},{0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F},
        {0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17},{0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F},
        {0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27},{0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F},
        {0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37},{0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F}},
    [1] = { // mod=1 (disp8)
        {0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47},{0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F},
        {0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57},{0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F},
        {0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67},{0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F},
        {0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77},{0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F}},
    [2] = { // mod=2 (disp32)
        {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87},{0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F},
        {0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97},{0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F},
        {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7},{0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF},
        {0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7},{0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF}},
    [3] = { // mod=3 (寄存器直接寻址)
        {0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7},{0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF},
        {0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7},{0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF},
        {0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7},{0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF},
        {0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7},{0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}},
};

static const uint8_t x86_sib_lut[4][8][8] = {
    [0] = { // scale=1
        {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07},{0x08,0x09,0x0A,0x0B,0x0C,0x0D,0x0E,0x0F},
        {0x10,0x11,0x12,0x13,0x14,0x15,0x16,0x17},{0x18,0x19,0x1A,0x1B,0x1C,0x1D,0x1E,0x1F},
        {0x20,0x21,0x22,0x23,0x24,0x25,0x26,0x27},{0x28,0x29,0x2A,0x2B,0x2C,0x2D,0x2E,0x2F},
        {0x30,0x31,0x32,0x33,0x34,0x35,0x36,0x37},{0x38,0x39,0x3A,0x3B,0x3C,0x3D,0x3E,0x3F}},
    [1] = { // scale=2
        {0x40,0x41,0x42,0x43,0x44,0x45,0x46,0x47},{0x48,0x49,0x4A,0x4B,0x4C,0x4D,0x4E,0x4F},
        {0x50,0x51,0x52,0x53,0x54,0x55,0x56,0x57},{0x58,0x59,0x5A,0x5B,0x5C,0x5D,0x5E,0x5F},
        {0x60,0x61,0x62,0x63,0x64,0x65,0x66,0x67},{0x68,0x69,0x6A,0x6B,0x6C,0x6D,0x6E,0x6F},
        {0x70,0x71,0x72,0x73,0x74,0x75,0x76,0x77},{0x78,0x79,0x7A,0x7B,0x7C,0x7D,0x7E,0x7F}},
    [2] = { // scale=4
        {0x80,0x81,0x82,0x83,0x84,0x85,0x86,0x87},{0x88,0x89,0x8A,0x8B,0x8C,0x8D,0x8E,0x8F},
        {0x90,0x91,0x92,0x93,0x94,0x95,0x96,0x97},{0x98,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F},
        {0xA0,0xA1,0xA2,0xA3,0xA4,0xA5,0xA6,0xA7},{0xA8,0xA9,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF},
        {0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7},{0xB8,0xB9,0xBA,0xBB,0xBC,0xBD,0xBE,0xBF}},
    [3] = { // scale=8
        {0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC7},{0xC8,0xC9,0xCA,0xCB,0xCC,0xCD,0xCE,0xCF},
        {0xD0,0xD1,0xD2,0xD3,0xD4,0xD5,0xD6,0xD7},{0xD8,0xD9,0xDA,0xDB,0xDC,0xDD,0xDE,0xDF},
        {0xE0,0xE1,0xE2,0xE3,0xE4,0xE5,0xE6,0xE7},{0xE8,0xE9,0xEA,0xEB,0xEC,0xED,0xEE,0xEF},
        {0xF0,0xF1,0xF2,0xF3,0xF4,0xF5,0xF6,0xF7},{0xF8,0xF9,0xFA,0xFB,0xFC,0xFD,0xFE,0xFF}},
};

// ============================================================
// 查表版内存编码
// ============================================================
static bool x86_encode_mem_lut(uint8_t **ptr, MachineRegister base,
                                MachineRegister index, uint8_t scale,
                                int32_t disp, uint8_t reg_opcode) {
    uint8_t base_enc = x86_reg_encoding(base) & 0x07;
    
    bool need_sib = (index != REG_NONE && scale > 0) ||
                    (base == REG_RSP) || (base == REG_R12);
    
    if (need_sib) {
        uint8_t idx_enc = (index != REG_NONE) ? (x86_reg_encoding(index) & 0x07) : 4;
        uint8_t sc = (scale >= 8) ? 3 : (scale >= 4) ? 2 : (scale >= 2) ? 1 : 0;
        
        if (disp == 0 && base != REG_RBP && base != REG_R13) {
            *(*ptr)++ = x86_modrm_lut[0][reg_opcode][4];
            *(*ptr)++ = x86_sib_lut[sc][idx_enc][base_enc];
        } else if (disp >= -128 && disp <= 127) {
            *(*ptr)++ = x86_modrm_lut[1][reg_opcode][4];
            *(*ptr)++ = x86_sib_lut[sc][idx_enc][base_enc];
            *(*ptr)++ = (uint8_t)disp;
        } else {
            *(*ptr)++ = x86_modrm_lut[2][reg_opcode][4];
            *(*ptr)++ = x86_sib_lut[sc][idx_enc][base_enc];
            x86_emit_imm32_fast(ptr, (uint32_t)disp);
        }
        return true;
    }
    
    if (base == REG_RIP) {
        *(*ptr)++ = x86_modrm_lut[0][reg_opcode][5];
        x86_emit_imm32_fast(ptr, (uint32_t)disp);
        return true;
    }
    
    if (disp == 0 && base != REG_RBP && base != REG_R13) {
        *(*ptr)++ = x86_modrm_lut[0][reg_opcode][base_enc];
    } else if (disp >= -128 && disp <= 127) {
        *(*ptr)++ = x86_modrm_lut[1][reg_opcode][base_enc];
        *(*ptr)++ = (uint8_t)disp;
    } else {
        *(*ptr)++ = x86_modrm_lut[2][reg_opcode][base_enc];
        x86_emit_imm32_fast(ptr, (uint32_t)disp);
    }
    return true;
}

// ============================================================
// 序言/尾声
// ============================================================
uint32_t x86_emit_prologue(const SilverTarget *target, IRFunction *func, uint8_t *buffer) {
    if (!target || !func || !buffer) return 0;
    uint8_t *ptr = buffer;
    // PUSH RBP
    uint8_t rbp_e = x86_reg_encoding(REG_RBP) & 0x07;
    if (x86_is_extended_reg(REG_RBP)) *ptr++ = 0x41;
    *ptr++ = 0x50 + rbp_e;
    // MOV RBP, RSP
    x86_emit_rex_fast(&ptr, true, 0, 0, x86_rex_b_for_reg(REG_RBP));
    *ptr++ = 0x89;
    *ptr++ = x86_modrm_lut[3][(x86_reg_encoding(REG_RSP) & 0x07)][rbp_e];
    // SUB RSP, stack_size
    if (func->stack_size > 0) {
        uint8_t rsp_e = x86_reg_encoding(REG_RSP) & 0x07;
        if (func->stack_size < 128) {
            x86_emit_rex_fast(&ptr, true, 0, 0, x86_rex_b_for_reg(REG_RSP));
            *ptr++ = 0x83; *ptr++ = x86_modrm_lut[3][5][rsp_e]; *ptr++ = (uint8_t)func->stack_size;
        } else {
            x86_emit_rex_fast(&ptr, true, 0, 0, x86_rex_b_for_reg(REG_RSP));
            *ptr++ = 0x81; *ptr++ = x86_modrm_lut[3][5][rsp_e]; x86_emit_imm32_fast(&ptr, func->stack_size);
        }
    }
    return (uint32_t)(ptr - buffer);
}

uint32_t x86_emit_epilogue(const SilverTarget *target, IRFunction *func, uint8_t *buffer) {
    if (!target || !func || !buffer) return 0;
    uint8_t *ptr = buffer;
    // ADD RSP, stack_size
    if (func->stack_size > 0) {
        uint8_t rsp_e = x86_reg_encoding(REG_RSP) & 0x07;
        if (func->stack_size < 128) {
            x86_emit_rex_fast(&ptr, true, 0, 0, x86_rex_b_for_reg(REG_RSP));
            *ptr++ = 0x83; *ptr++ = x86_modrm_lut[3][0][rsp_e]; *ptr++ = (uint8_t)func->stack_size;
        } else {
            x86_emit_rex_fast(&ptr, true, 0, 0, x86_rex_b_for_reg(REG_RSP));
            *ptr++ = 0x81; *ptr++ = x86_modrm_lut[3][0][rsp_e]; x86_emit_imm32_fast(&ptr, func->stack_size);
        }
    }
    // POP RBP
    uint8_t rbp_e = x86_reg_encoding(REG_RBP) & 0x07;
    if (x86_is_extended_reg(REG_RBP)) *ptr++ = 0x41;
    *ptr++ = 0x58 + rbp_e;
    // RET
    *ptr++ = 0xC3;
    return (uint32_t)(ptr - buffer);
}

// ============================================================
// 完整编码函数
// ============================================================
bool x86_encode_instruction(const SilverTarget *target,
                             const MachineInstExt *inst,
                             uint8_t *buffer, uint32_t *length) {
    if (!target || !inst || !buffer || !length) return false;
    uint8_t *ptr = buffer;
    bool success = true;
    bool is64 = true;
    MachineRegister r_d = (MachineRegister)inst->base.rd;
    MachineRegister r_n = (MachineRegister)inst->base.rn;
    MachineRegister r_m = (MachineRegister)inst->base.rm;
    uint8_t d_e = x86_reg_encoding(r_d) & 0x07;
    uint8_t n_e = x86_reg_encoding(r_n) & 0x07;
    uint8_t m_e = x86_reg_encoding(r_m) & 0x07;
    uint8_t n_r = x86_rex_r_for_reg(r_n);
    uint8_t d_b = x86_rex_b_for_reg(r_d);

    switch (inst->base.opcode) {
    // ============================================================
    // 控制流
    // ============================================================
    case MACH_NOP: *ptr++ = 0x90; break;
    case MACH_INT3: *ptr++ = 0xCC; break;
    case MACH_RET: *ptr++ = 0xC3; break;
    
    case MACH_JMP:
        if (inst->extended_imm != 0) {
            int32_t off = (int32_t)inst->extended_imm;
            if (off >= -128 && off <= 127) { *ptr++ = 0xEB; *ptr++ = (uint8_t)off; }
            else { *ptr++ = 0xE9; x86_emit_imm32_fast(&ptr, (uint32_t)(off - 5)); }
        } else {
            d_b = x86_rex_b_for_reg(r_n);
            x86_emit_rex_fast(&ptr, false, 0, 0, d_b);
            *ptr++ = 0xFF; *ptr++ = x86_modrm_lut[3][4][n_e];
        }
        break;
    
    case MACH_JCC: {
        int32_t off = (int32_t)inst->extended_imm;
        MachineCondition cond = (MachineCondition)inst->base.imm;
        static const uint8_t jcc_s[]={[COND_O]=0x70,[COND_NO]=0x71,[COND_B]=0x72,[COND_AE]=0x73,
            [COND_E]=0x74,[COND_NE]=0x75,[COND_BE]=0x76,[COND_A]=0x77,
            [COND_S]=0x78,[COND_NS]=0x79,[COND_P]=0x7A,[COND_NP]=0x7B,
            [COND_L]=0x7C,[COND_GE]=0x7D,[COND_LE]=0x7E,[COND_G]=0x7F};
        static const uint8_t jcc_n[]={[COND_O]=0x80,[COND_NO]=0x81,[COND_B]=0x82,[COND_AE]=0x83,
            [COND_E]=0x84,[COND_NE]=0x85,[COND_BE]=0x86,[COND_A]=0x87,
            [COND_S]=0x88,[COND_NS]=0x89,[COND_P]=0x8A,[COND_NP]=0x8B,
            [COND_L]=0x8C,[COND_GE]=0x8D,[COND_LE]=0x8E,[COND_G]=0x8F};
        if (off >= -128 && off <= 127) { *ptr++ = jcc_s[cond]; *ptr++ = (uint8_t)off; }
        else { *ptr++ = 0x0F; *ptr++ = jcc_n[cond]; x86_emit_imm32_fast(&ptr, (uint32_t)(off - 6)); }
        break;
    }
    
    case MACH_CALL:
        { int32_t off = (int32_t)inst->extended_imm; *ptr++ = 0xE8; x86_emit_imm32_fast(&ptr, (uint32_t)(off - 5)); }
        break;
    
    case MACH_INDIRECT_CALL:
        d_b = x86_rex_b_for_reg(r_n);
        x86_emit_rex_fast(&ptr, false, 0, 0, d_b);
        *ptr++ = 0xFF; *ptr++ = x86_modrm_lut[3][2][n_e];
        break;
    
    case MACH_PUSH:
        d_b = x86_rex_b_for_reg(r_n);
        if (d_b) *ptr++ = 0x41;
        *ptr++ = 0x50 + n_e;
        break;
    
    case MACH_POP:
        d_b = x86_rex_b_for_reg(r_d);
        if (d_b) *ptr++ = 0x41;
        *ptr++ = 0x58 + d_e;
        break;
    
    // ============================================================
    // 数据传输
    // ============================================================
    case MACH_MOV:
        if (r_n != REG_NONE) {
            n_r = x86_rex_r_for_reg(r_n); d_b = x86_rex_b_for_reg(r_d);
            x86_emit_rex_fast(&ptr, true, n_r, 0, d_b);
            *ptr++ = 0x89; *ptr++ = x86_modrm_lut[3][n_e][d_e];
        } else if (inst->extended_imm != 0 || inst->base.imm != 0) {
            d_b = x86_rex_b_for_reg(r_d);
            x86_emit_rex_fast(&ptr, true, 0, 0, d_b);
            *ptr++ = 0xB8 + d_e;
            x86_emit_imm64_fast(&ptr, inst->extended_imm);
        }
        break;
    
    case MACH_MOV_IMM:
        d_b = x86_rex_b_for_reg(r_d);
        x86_emit_rex_fast(&ptr, true, 0, 0, d_b);
        *ptr++ = 0xB8 + d_e;
        x86_emit_imm64_fast(&ptr, inst->extended_imm);
        break;
    
    case MACH_LOAD:
        n_r = x86_rex_r_for_reg(r_d); d_b = x86_rex_b_for_reg(r_n);
        x86_emit_rex_fast(&ptr, true, n_r, 0, d_b);
        *ptr++ = 0x8B;
        success = x86_encode_mem_lut(&ptr, r_n, REG_NONE, 0, inst->displacement, d_e);
        break;
    
    case MACH_STORE:
        n_r = x86_rex_r_for_reg(r_n); d_b = x86_rex_b_for_reg(r_m);
        x86_emit_rex_fast(&ptr, true, n_r, 0, d_b);
        *ptr++ = 0x89;
        success = x86_encode_mem_lut(&ptr, r_m, REG_NONE, 0, inst->displacement, n_e);
        break;
    
    case MACH_LEA: {
        uint8_t rr = x86_rex_r_for_reg(r_d), rx = x86_rex_r_for_reg(r_n), rb = x86_rex_b_for_reg(r_m);
        x86_emit_rex_fast(&ptr, true, rr, rx, rb);
        *ptr++ = 0x8D;
        success = x86_encode_mem_lut(&ptr, r_m, r_n, inst->base.imm, inst->displacement, d_e);
        break;
    }
    
    case MACH_MOVSX:
        n_r = x86_rex_r_for_reg(r_d); d_b = x86_rex_b_for_reg(r_n);
        x86_emit_rex_fast(&ptr, true, n_r, 0, d_b);
        *ptr++ = 0x63; *ptr++ = x86_modrm_lut[3][d_e][n_e];
        break;
    
    // ============================================================
    // 整数算术
    // ============================================================
    case MACH_ADD:
        n_r = x86_rex_r_for_reg(r_n); d_b = x86_rex_b_for_reg(r_d);
        x86_emit_rex_fast(&ptr, true, n_r, 0, d_b);
        *ptr++ = 0x03; *ptr++ = x86_modrm_lut[3][n_e][d_e];
        break;
    
    case MACH_ADD_IMM: {
        int32_t imm = (int32_t)inst->extended_imm;
        d_b = x86_rex_b_for_reg(r_d);
        x86_emit_rex_fast(&ptr, true, 0, 0, d_b);
        if (imm >= -128 && imm <= 127) {
            *ptr++ = 0x83; *ptr++ = x86_modrm_lut[3][0][d_e]; *ptr++ = (uint8_t)imm;
        } else {
            *ptr++ = 0x81; *ptr++ = x86_modrm_lut[3][0][d_e]; x86_emit_imm32_fast(&ptr, (uint32_t)imm);
        }
        break;
    }
    
    case MACH_SUB:
        n_r = x86_rex_r_for_reg(r_n); d_b = x86_rex_b_for_reg(r_d);
        x86_emit_rex_fast(&ptr, true, n_r, 0, d_b);
        *ptr++ = 0x2B; *ptr++ = x86_modrm_lut[3][n_e][d_e];
        break;
    
    case MACH_SUB_IMM: {
        int32_t imm = (int32_t)inst->extended_imm;
        d_b = x86_rex_b_for_reg(r_d);
        x86_emit_rex_fast(&ptr, true, 0, 0, d_b);
        if (imm >= -128 && imm <= 127) {
            *ptr++ = 0x83; *ptr++ = x86_modrm_lut[3][5][d_e]; *ptr++ = (uint8_t)imm;
        } else {
            *ptr++ = 0x81; *ptr++ = x86_modrm_lut[3][5][d_e]; x86_emit_imm32_fast(&ptr, (uint32_t)imm);
        }
        break;
    }
    
    case MACH_MUL:
        n_r = x86_rex_r_for_reg(r_d); d_b = x86_rex_b_for_reg(r_n);
        x86_emit_rex_fast(&ptr, true, n_r, 0, d_b);
        *ptr++ = 0x0F; *ptr++ = 0xAF; *ptr++ = x86_modrm_lut[3][d_e][n_e];
        break;
    
    case MACH_DIV:
    case MACH_UDIV:
        d_b = x86_rex_b_for_reg(r_n);
        x86_emit_rex_fast(&ptr, true, 0, 0, d_b);
        *ptr++ = 0xF7; *ptr++ = x86_modrm_lut[3][7][n_e];
        break;
    
    case MACH_CDQ:
    case MACH_CQO:
        x86_emit_rex_fast(&ptr, true, 0, 0, 0); *ptr++ = 0x99;
        break;
    
    // ============================================================
    // 位操作
    // ============================================================
    case MACH_AND:
        n_r = x86_rex_r_for_reg(r_n); d_b = x86_rex_b_for_reg(r_d);
        x86_emit_rex_fast(&ptr, true, n_r, 0, d_b);
        *ptr++ = 0x23; *ptr++ = x86_modrm_lut[3][n_e][d_e];
        break;
    
    case MACH_OR:
        n_r = x86_rex_r_for_reg(r_n); d_b = x86_rex_b_for_reg(r_d);
        x86_emit_rex_fast(&ptr, true, n_r, 0, d_b);
        *ptr++ = 0x0B; *ptr++ = x86_modrm_lut[3][n_e][d_e];
        break;
    
    case MACH_XOR:
        n_r = x86_rex_r_for_reg(r_n); d_b = x86_rex_b_for_reg(r_d);
        x86_emit_rex_fast(&ptr, true, n_r, 0, d_b);
        *ptr++ = 0x33; *ptr++ = x86_modrm_lut[3][n_e][d_e];
        break;
    
    case MACH_SHL_IMM:
        d_b = x86_rex_b_for_reg(r_d);
        x86_emit_rex_fast(&ptr, true, 0, 0, d_b);
        if (inst->extended_imm == 1) { *ptr++ = 0xD1; *ptr++ = x86_modrm_lut[3][4][d_e]; }
        else { *ptr++ = 0xC1; *ptr++ = x86_modrm_lut[3][4][d_e]; *ptr++ = (uint8_t)inst->extended_imm; }
        break;
    
    case MACH_LSHR_IMM:
        d_b = x86_rex_b_for_reg(r_d);
        x86_emit_rex_fast(&ptr, true, 0, 0, d_b);
        *ptr++ = 0xC1; *ptr++ = x86_modrm_lut[3][5][d_e]; *ptr++ = (uint8_t)inst->extended_imm;
        break;
    
    case MACH_ASHR_IMM:
        d_b = x86_rex_b_for_reg(r_d);
        x86_emit_rex_fast(&ptr, true, 0, 0, d_b);
        *ptr++ = 0xC1; *ptr++ = x86_modrm_lut[3][7][d_e]; *ptr++ = (uint8_t)inst->extended_imm;
        break;
    
    // ============================================================
    // 比较
    // ============================================================
    case MACH_CMP:
        n_r = x86_rex_r_for_reg(r_m); d_b = x86_rex_b_for_reg(r_n);
        x86_emit_rex_fast(&ptr, true, n_r, 0, d_b);
        *ptr++ = 0x3B; *ptr++ = x86_modrm_lut[3][m_e][n_e];
        break;
    
    case MACH_CMP_IMM: {
        int32_t imm = (int32_t)inst->extended_imm;
        d_b = x86_rex_b_for_reg(r_n);
        x86_emit_rex_fast(&ptr, true, 0, 0, d_b);
        if (imm >= -128 && imm <= 127) {
            *ptr++ = 0x83; *ptr++ = x86_modrm_lut[3][7][n_e]; *ptr++ = (uint8_t)imm;
        } else {
            *ptr++ = 0x81; *ptr++ = x86_modrm_lut[3][7][n_e]; x86_emit_imm32_fast(&ptr, (uint32_t)imm);
        }
        break;
    }
    
    case MACH_SETCC: {
        static const uint8_t setcc[]={[COND_O]=0x90,[COND_NO]=0x91,[COND_B]=0x92,[COND_AE]=0x93,
            [COND_E]=0x94,[COND_NE]=0x95,[COND_BE]=0x96,[COND_A]=0x97,
            [COND_S]=0x98,[COND_NS]=0x99,[COND_P]=0x9A,[COND_NP]=0x9B,
            [COND_L]=0x9C,[COND_GE]=0x9D,[COND_LE]=0x9E,[COND_G]=0x9F};
        d_b = x86_rex_b_for_reg(r_d);
        x86_emit_rex_fast(&ptr, false, 0, 0, d_b);
        *ptr++ = 0x0F; *ptr++ = setcc[inst->base.imm];
        *ptr++ = x86_modrm_lut[3][0][d_e];
        break;
    }
    
    // ============================================================
    // 浮点 SSE/SSE2
    // ============================================================
    case MACH_FADD:
        n_r = x86_rex_r_for_reg(r_d); d_b = x86_rex_b_for_reg(r_n);
        *ptr++ = 0xF2; x86_emit_rex_fast(&ptr, false, n_r, 0, d_b);
        *ptr++ = 0x0F; *ptr++ = 0x58; *ptr++ = x86_modrm_lut[3][d_e][n_e];
        break;
    
    case MACH_FSUB:
        n_r = x86_rex_r_for_reg(r_d); d_b = x86_rex_b_for_reg(r_n);
        *ptr++ = 0xF2; x86_emit_rex_fast(&ptr, false, n_r, 0, d_b);
        *ptr++ = 0x0F; *ptr++ = 0x5C; *ptr++ = x86_modrm_lut[3][d_e][n_e];
        break;
    
    case MACH_FMUL:
        n_r = x86_rex_r_for_reg(r_d); d_b = x86_rex_b_for_reg(r_n);
        *ptr++ = 0xF2; x86_emit_rex_fast(&ptr, false, n_r, 0, d_b);
        *ptr++ = 0x0F; *ptr++ = 0x59; *ptr++ = x86_modrm_lut[3][d_e][n_e];
        break;
    
    case MACH_FDIV:
        n_r = x86_rex_r_for_reg(r_d); d_b = x86_rex_b_for_reg(r_n);
        *ptr++ = 0xF2; x86_emit_rex_fast(&ptr, false, n_r, 0, d_b);
        *ptr++ = 0x0F; *ptr++ = 0x5E; *ptr++ = x86_modrm_lut[3][d_e][n_e];
        break;
    
    case MACH_FCMP:
        n_r = x86_rex_r_for_reg(r_n); d_b = x86_rex_b_for_reg(r_m);
        *ptr++ = 0x66; x86_emit_rex_fast(&ptr, false, n_r, 0, d_b);
        *ptr++ = 0x0F; *ptr++ = 0x2E; *ptr++ = x86_modrm_lut[3][n_e][m_e];
        break;
    
    case MACH_CVTSI2SD:
        n_r = x86_rex_r_for_reg(r_d); d_b = x86_rex_b_for_reg(r_n);
        *ptr++ = 0xF2; x86_emit_rex_fast(&ptr, true, n_r, 0, d_b);
        *ptr++ = 0x0F; *ptr++ = 0x2A; *ptr++ = x86_modrm_lut[3][d_e][n_e];
        break;
    
    case MACH_CVTSD2SI:
        n_r = x86_rex_r_for_reg(r_d); d_b = x86_rex_b_for_reg(r_n);
        *ptr++ = 0xF2; x86_emit_rex_fast(&ptr, true, n_r, 0, d_b);
        *ptr++ = 0x0F; *ptr++ = 0x2D; *ptr++ = x86_modrm_lut[3][d_e][n_e];
        break;
    
    default:
        *ptr++ = 0xCC; success = false; break;
    }
    
    *length = (uint32_t)(ptr - buffer);
    return success;
}

// ============================================================
// System V AMD64 ABI 辅助
// ============================================================
typedef struct {
    MachineRegister int_regs[6]; uint32_t num_int_regs, int_reg_index;
    MachineRegister float_regs[8]; uint32_t num_float_regs, float_reg_index;
    uint32_t stack_arg_offset;
} CallingConventionState;

static void cc_init(CallingConventionState *cc) {
    memset(cc, 0, sizeof(*cc));
    cc->int_regs[0]=REG_RDI; cc->int_regs[1]=REG_RSI; cc->int_regs[2]=REG_RDX;
    cc->int_regs[3]=REG_RCX; cc->int_regs[4]=REG_R8;  cc->int_regs[5]=REG_R9;
    cc->num_int_regs=6;
    cc->float_regs[0]=REG_XMM0; cc->float_regs[1]=REG_XMM1; cc->float_regs[2]=REG_XMM2;
    cc->float_regs[3]=REG_XMM3; cc->float_regs[4]=REG_XMM4; cc->float_regs[5]=REG_XMM5;
    cc->float_regs[6]=REG_XMM6; cc->float_regs[7]=REG_XMM7;
    cc->num_float_regs=8;
    cc->stack_arg_offset=16;
}

typedef enum { ARG_LOC_REG, ARG_LOC_STACK } ArgLocType;
typedef struct { ArgLocType type; MachineRegister reg; int32_t stack_off; } ArgLocation;

static ArgLocation classify_argument(CallingConventionState *cc, IRTypeId tid, IRTypeTable *tt) {
    ArgLocation loc; memset(&loc, 0, sizeof(loc));
    IRType *t = ir_type_get(tt, tid);
    if (!t) { loc.type=ARG_LOC_STACK; loc.stack_off=cc->stack_arg_offset; cc->stack_arg_offset+=8; return loc; }
    if (t->kind==IR_TYPE_INT || t->kind==IR_TYPE_PTR) {
        if (cc->int_reg_index<cc->num_int_regs) { loc.type=ARG_LOC_REG; loc.reg=cc->int_regs[cc->int_reg_index++]; }
        else { loc.type=ARG_LOC_STACK; loc.stack_off=cc->stack_arg_offset; cc->stack_arg_offset+=8; }
    } else if (t->kind==IR_TYPE_FLOAT) {
        if (cc->float_reg_index<cc->num_float_regs) { loc.type=ARG_LOC_REG; loc.reg=cc->float_regs[cc->float_reg_index++]; }
        else { loc.type=ARG_LOC_STACK; loc.stack_off=cc->stack_arg_offset; cc->stack_arg_offset+=8; }
    } else {
        loc.type=ARG_LOC_STACK; loc.stack_off=cc->stack_arg_offset;
        cc->stack_arg_offset+=(t->size+7)&~7;
    }
    return loc;
}

// ============================================================
// 寄存器描述表 & 目标创建/销毁
// ============================================================
static const RegisterDesc x86_64_reg_descs[] = {
    [REG_NONE]={REG_NONE,REG_CLASS_NONE,"none",0,false,false,false,0},
    [REG_RAX]={REG_RAX,REG_CLASS_GPR,"rax",0,true,false,false,0},
    [REG_RCX]={REG_RCX,REG_CLASS_GPR,"rcx",1,true,false,false,0},
    [REG_RDX]={REG_RDX,REG_CLASS_GPR,"rdx",2,true,false,false,0},
    [REG_RSI]={REG_RSI,REG_CLASS_GPR,"rsi",6,true,false,false,0},
    [REG_RDI]={REG_RDI,REG_CLASS_GPR,"rdi",7,true,false,false,0},
    [REG_R8] ={REG_R8, REG_CLASS_GPR,"r8", 8,true,false,false,0},
    [REG_R9] ={REG_R9, REG_CLASS_GPR,"r9", 9,true,false,false,0},
    [REG_R10]={REG_R10,REG_CLASS_GPR,"r10",10,true,false,false,0},
    [REG_R11]={REG_R11,REG_CLASS_GPR,"r11",11,true,false,false,0},
    [REG_RBX]={REG_RBX,REG_CLASS_GPR,"rbx",3,false,true,false,0},
    [REG_R12]={REG_R12,REG_CLASS_GPR,"r12",12,false,true,false,0},
    [REG_R13]={REG_R13,REG_CLASS_GPR,"r13",13,false,true,false,0},
    [REG_R14]={REG_R14,REG_CLASS_GPR,"r14",14,false,true,false,0},
    [REG_R15]={REG_R15,REG_CLASS_GPR,"r15",15,false,true,false,0},
    [REG_RSP]={REG_RSP,REG_CLASS_SPECIAL,"rsp",4,false,false,true,0},
    [REG_RBP]={REG_RBP,REG_CLASS_SPECIAL,"rbp",5,false,false,true,0},
    [REG_RIP]={REG_RIP,REG_CLASS_SPECIAL,"rip",0,false,false,true,0},
    [REG_XMM0]={REG_XMM0,REG_CLASS_FPR,"xmm0",0,true,false,false,0},
    [REG_XMM1]={REG_XMM1,REG_CLASS_FPR,"xmm1",1,true,false,false,0},
    [REG_XMM2]={REG_XMM2,REG_CLASS_FPR,"xmm2",2,true,false,false,0},
    [REG_XMM3]={REG_XMM3,REG_CLASS_FPR,"xmm3",3,true,false,false,0},
    [REG_XMM4]={REG_XMM4,REG_CLASS_FPR,"xmm4",4,true,false,false,0},
    [REG_XMM5]={REG_XMM5,REG_CLASS_FPR,"xmm5",5,true,false,false,0},
    [REG_XMM6]={REG_XMM6,REG_CLASS_FPR,"xmm6",6,true,false,false,0},
    [REG_XMM7]={REG_XMM7,REG_CLASS_FPR,"xmm7",7,true,false,false,0},
    [REG_XMM8]={REG_XMM8,REG_CLASS_FPR,"xmm8",8,false,true,false,0},
    [REG_XMM9]={REG_XMM9,REG_CLASS_FPR,"xmm9",9,false,true,false,0},
    [REG_XMM10]={REG_XMM10,REG_CLASS_FPR,"xmm10",10,false,true,false,0},
    [REG_XMM11]={REG_XMM11,REG_CLASS_FPR,"xmm11",11,false,true,false,0},
    [REG_XMM12]={REG_XMM12,REG_CLASS_FPR,"xmm12",12,false,true,false,0},
    [REG_XMM13]={REG_XMM13,REG_CLASS_FPR,"xmm13",13,false,true,false,0},
    [REG_XMM14]={REG_XMM14,REG_CLASS_FPR,"xmm14",14,false,true,false,0},
    [REG_XMM15]={REG_XMM15,REG_CLASS_FPR,"xmm15",15,false,true,false,0},
};

SilverTarget *x86_target_create(void) {
    SilverTarget *t = (SilverTarget *)calloc(1, sizeof(SilverTarget));
    if (!t) return NULL;
    t->arch = SILVER_TARGET_X86_64; t->name = "x86-64"; t->triple = "x86_64-unknown-none";
    t->is_64_bit = true; t->pointer_size = 8; t->max_alignment = 16;
    t->registers = x86_64_reg_descs;
    t->num_registers = sizeof(x86_64_reg_descs) / sizeof(x86_64_reg_descs[0]);
    t->cc.caller_saved = machine_reg_mask(REG_RAX)|machine_reg_mask(REG_RCX)|
        machine_reg_mask(REG_RDX)|machine_reg_mask(REG_RSI)|machine_reg_mask(REG_RDI)|
        machine_reg_mask(REG_R8)|machine_reg_mask(REG_R9)|machine_reg_mask(REG_R10)|machine_reg_mask(REG_R11);
    t->cc.callee_saved = machine_reg_mask(REG_RBX)|machine_reg_mask(REG_R12)|
        machine_reg_mask(REG_R13)|machine_reg_mask(REG_R14)|machine_reg_mask(REG_R15);
    t->cc.argument_regs[0]=machine_reg_mask(REG_RDI); t->cc.argument_regs[1]=machine_reg_mask(REG_RSI);
    t->cc.argument_regs[2]=machine_reg_mask(REG_RDX); t->cc.argument_regs[3]=machine_reg_mask(REG_RCX);
    t->cc.argument_regs[4]=machine_reg_mask(REG_R8);  t->cc.argument_regs[5]=machine_reg_mask(REG_R9);
    t->cc.return_reg = machine_reg_mask(REG_RAX); t->cc.return_reg_hi = machine_reg_mask(REG_RDX);
    t->cc.stack_alignment = 16; t->cc.red_zone_size = 128;
    t->available_gpr = machine_reg_mask(REG_RAX)|machine_reg_mask(REG_RCX)|machine_reg_mask(REG_RDX)|
        machine_reg_mask(REG_RBX)|machine_reg_mask(REG_RSI)|machine_reg_mask(REG_RDI)|
        machine_reg_mask(REG_R8)|machine_reg_mask(REG_R9)|machine_reg_mask(REG_R10)|machine_reg_mask(REG_R11)|
        machine_reg_mask(REG_R12)|machine_reg_mask(REG_R13)|machine_reg_mask(REG_R14)|machine_reg_mask(REG_R15);
    t->available_fpr = machine_reg_mask_range(REG_XMM0, REG_XMM15);
    t->sp_reg = REG_RSP; t->bp_reg = REG_RBP; t->ip_reg = REG_RIP; t->flag_reg = REG_NONE;
    t->encode = x86_encode_instruction;
    t->emit_prologue = x86_emit_prologue; t->emit_epilogue = x86_emit_epilogue;
    return t;
}

void x86_target_destroy(SilverTarget *target) { free(target); }