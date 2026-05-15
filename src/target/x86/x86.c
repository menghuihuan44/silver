#include "silver/target/x86/x86.h"
#include "silver/target/x86/x86_encode.h"
#include <stdlib.h>
#include <string.h>

// ============================================================
// 快速编码辅助内联函数
// ============================================================
static inline uint8_t x86_make_rex(bool w, uint8_t r, uint8_t x, uint8_t b) {
    return 0x40 | (w ? 0x08 : 0) | (r ? 0x04 : 0) | (x ? 0x02 : 0) | (b ? 0x01 : 0);
}
static inline bool x86_need_rex(bool w, uint8_t r, uint8_t x, uint8_t b) {
    return w || r || x || b;
}
static inline uint8_t x86_make_modrm(uint8_t mod, uint8_t reg, uint8_t rm) {
    return ((mod & 0x03) << 6) | ((reg & 0x07) << 3) | (rm & 0x07);
}
static inline uint8_t x86_make_sib(uint8_t scale, uint8_t index, uint8_t base) {
    return ((scale & 0x03) << 6) | ((index & 0x07) << 3) | (base & 0x07);
}
static inline void x86_emit_rex_fast(uint8_t **ptr, bool w, uint8_t r, uint8_t x, uint8_t b) {
    if (x86_need_rex(w, r, x, b)) *(*ptr)++ = x86_make_rex(w, r, x, b);
}
static inline void x86_emit_modrm_fast(uint8_t **ptr, uint8_t mod, uint8_t reg, uint8_t rm) {
    *(*ptr)++ = x86_make_modrm(mod, reg, rm);
}
static inline void x86_emit_sib_fast(uint8_t **ptr, uint8_t scale, uint8_t index, uint8_t base) {
    *(*ptr)++ = x86_make_sib(scale, index, base);
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

// 编码内存地址：返回是否成功
static bool x86_encode_mem_fast(uint8_t **ptr, MachineRegister base, MachineRegister index,
                                 uint8_t scale, int32_t disp, uint8_t reg_opcode) {
    uint8_t base_enc = x86_reg_encoding(base) & 0x07;
    
    if (index != REG_NONE && scale > 0) {
        uint8_t idx_enc = x86_reg_encoding(index) & 0x07;
        if (disp == 0 && base != REG_RBP && base != REG_R13) {
            x86_emit_modrm_fast(ptr, 0, reg_opcode, 4);
            x86_emit_sib_fast(ptr, scale, idx_enc, base_enc);
        } else if (disp >= -128 && disp <= 127) {
            x86_emit_modrm_fast(ptr, 1, reg_opcode, 4);
            x86_emit_sib_fast(ptr, scale, idx_enc, base_enc);
            x86_emit_imm8_fast(ptr, (uint8_t)disp);
        } else {
            x86_emit_modrm_fast(ptr, 2, reg_opcode, 4);
            x86_emit_sib_fast(ptr, scale, idx_enc, base_enc);
            x86_emit_imm32_fast(ptr, (uint32_t)disp);
        }
        return true;
    }
    
    if (base == REG_RSP || base == REG_R12) {
        if (disp == 0) {
            x86_emit_modrm_fast(ptr, 0, reg_opcode, 4);
            x86_emit_sib_fast(ptr, 0, 4, 4);
        } else if (disp >= -128 && disp <= 127) {
            x86_emit_modrm_fast(ptr, 1, reg_opcode, 4);
            x86_emit_sib_fast(ptr, 0, 4, 4);
            x86_emit_imm8_fast(ptr, (uint8_t)disp);
        } else {
            x86_emit_modrm_fast(ptr, 2, reg_opcode, 4);
            x86_emit_sib_fast(ptr, 0, 4, 4);
            x86_emit_imm32_fast(ptr, (uint32_t)disp);
        }
        return true;
    }
    
    if (base == REG_RIP) {
        x86_emit_modrm_fast(ptr, 0, reg_opcode, 5);
        x86_emit_imm32_fast(ptr, (uint32_t)disp);
        return true;
    }
    
    if (disp == 0 && base != REG_RBP && base != REG_R13) {
        x86_emit_modrm_fast(ptr, 0, reg_opcode, base_enc);
    } else if (disp >= -128 && disp <= 127) {
        x86_emit_modrm_fast(ptr, 1, reg_opcode, base_enc);
        x86_emit_imm8_fast(ptr, (uint8_t)disp);
    } else {
        x86_emit_modrm_fast(ptr, 2, reg_opcode, base_enc);
        x86_emit_imm32_fast(ptr, (uint32_t)disp);
    }
    return true;
}

// ============================================================
// System V AMD64 ABI
// ============================================================
typedef struct { MachineRegister int_regs[6]; uint32_t num_int_regs; uint32_t int_reg_index;
                 MachineRegister float_regs[8]; uint32_t num_float_regs; uint32_t float_reg_index;
                 uint32_t stack_arg_offset; uint32_t arg_count; } CallingConventionState;

static void cc_init(CallingConventionState *cc, const SilverTarget *target) {
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

typedef enum { ARG_LOCATION_REG, ARG_LOCATION_STACK } ArgLocationType;
typedef struct { ArgLocationType type; MachineRegister reg; int32_t stack_offset; } ArgLocation;

static ArgLocation classify_argument(CallingConventionState *cc, IRTypeId type_id, IRTypeTable *types) {
    ArgLocation loc; memset(&loc, 0, sizeof(loc));
    IRType *type = ir_type_get(types, type_id);
    if (!type) { loc.type = ARG_LOCATION_STACK; loc.stack_offset = cc->stack_arg_offset; cc->stack_arg_offset += 8; return loc; }
    if (type->kind == IR_TYPE_INT || type->kind == IR_TYPE_PTR) {
        if (cc->int_reg_index < cc->num_int_regs) { loc.type = ARG_LOCATION_REG; loc.reg = cc->int_regs[cc->int_reg_index++]; }
        else { loc.type = ARG_LOCATION_STACK; loc.stack_offset = cc->stack_arg_offset; cc->stack_arg_offset += 8; }
    } else if (type->kind == IR_TYPE_FLOAT) {
        if (cc->float_reg_index < cc->num_float_regs) { loc.type = ARG_LOCATION_REG; loc.reg = cc->float_regs[cc->float_reg_index++]; }
        else { loc.type = ARG_LOCATION_STACK; loc.stack_offset = cc->stack_arg_offset; cc->stack_arg_offset += 8; }
    } else {
        loc.type = ARG_LOCATION_STACK; loc.stack_offset = cc->stack_arg_offset;
        cc->stack_arg_offset += (type->size + 7) & ~7;
    }
    return loc;
}

// ============================================================
// x86-64 函数序言/尾声
// ============================================================
uint32_t x86_emit_prologue(const SilverTarget *target, IRFunction *func, uint8_t *buffer) {
    if (!target || !func || !buffer) return 0;
    uint8_t *ptr = buffer;
    x86_emit_rex_fast(&ptr, false, 0, 0, x86_rex_b_for_reg(REG_RBP));
    *ptr++ = 0x50 + x86_reg_encoding(REG_RBP); // PUSH RBP
    x86_emit_rex_fast(&ptr, true, 0, 0, x86_rex_b_for_reg(REG_RSP));
    *ptr++ = 0x89; // MOV r/m, r
    x86_emit_modrm_fast(&ptr, 3, x86_reg_encoding(REG_RSP) & 0x07, x86_reg_encoding(REG_RBP) & 0x07);
    if (func->stack_size > 0) {
        uint8_t dst_enc = x86_reg_encoding(REG_RSP) & 0x07;
        x86_emit_rex_fast(&ptr, true, 0, 0, x86_rex_b_for_reg(REG_RSP));
        if (func->stack_size < 128) {
            *ptr++ = 0x83; x86_emit_modrm_fast(&ptr, 3, 5, dst_enc); *ptr++ = (uint8_t)func->stack_size;
        } else {
            *ptr++ = 0x81; x86_emit_modrm_fast(&ptr, 3, 5, dst_enc); x86_emit_imm32_fast(&ptr, func->stack_size);
        }
    }
    return (uint32_t)(ptr - buffer);
}

uint32_t x86_emit_epilogue(const SilverTarget *target, IRFunction *func, uint8_t *buffer) {
    if (!target || !func || !buffer) return 0;
    uint8_t *ptr = buffer;
    if (func->stack_size > 0) {
        uint8_t dst_enc = x86_reg_encoding(REG_RSP) & 0x07;
        x86_emit_rex_fast(&ptr, true, 0, 0, x86_rex_b_for_reg(REG_RSP));
        if (func->stack_size < 128) {
            *ptr++ = 0x83; x86_emit_modrm_fast(&ptr, 3, 0, dst_enc); *ptr++ = (uint8_t)func->stack_size;
        } else {
            *ptr++ = 0x81; x86_emit_modrm_fast(&ptr, 3, 0, dst_enc); x86_emit_imm32_fast(&ptr, func->stack_size);
        }
    }
    x86_emit_rex_fast(&ptr, false, 0, 0, x86_rex_b_for_reg(REG_RBP));
    *ptr++ = 0x58 + x86_reg_encoding(REG_RBP); // POP RBP
    *ptr++ = 0xC3; // RET
    return (uint32_t)(ptr - buffer);
}

// ============================================================
// 完整快速编码函数
// ============================================================
bool x86_encode_instruction(const SilverTarget *target,
                             const MachineInstExt *inst,
                             uint8_t *buffer, uint32_t *length) {
    if (!target || !inst || !buffer || !length) return false;
    uint8_t *ptr = buffer;
    bool success = false;
    bool is64 = true;
    MachineRegister rd = (MachineRegister)inst->base.rd;
    MachineRegister rn = (MachineRegister)inst->base.rn;
    MachineRegister rm = (MachineRegister)inst->base.rm;
    uint8_t rd_e = x86_reg_encoding(rd) & 0x07;
    uint8_t rn_e = x86_reg_encoding(rn) & 0x07;
    uint8_t rm_e = x86_reg_encoding(rm) & 0x07;
    uint8_t r_r = x86_rex_r_for_reg(rn);
    uint8_t r_b = x86_rex_b_for_reg(rd);
    
    switch (inst->base.opcode) {
    case MACH_NOP: *ptr++ = 0x90; success = true; break;
    
    case MACH_MOV:
        if (rn != REG_NONE) {
            r_r = x86_rex_r_for_reg(rn); r_b = x86_rex_b_for_reg(rd);
            x86_emit_rex_fast(&ptr, true, r_r, 0, r_b);
            *ptr++ = 0x89; x86_emit_modrm_fast(&ptr, 3, rn_e, rd_e);
            success = true;
        } else if (inst->extended_imm != 0 || inst->base.imm != 0) {
            r_b = x86_rex_b_for_reg(rd);
            x86_emit_rex_fast(&ptr, true, 0, 0, r_b);
            *ptr++ = 0xB8 + rd_e;
            x86_emit_imm64_fast(&ptr, inst->extended_imm);
            success = true;
        }
        break;
    
    case MACH_MOV_IMM:
        r_b = x86_rex_b_for_reg(rd);
        x86_emit_rex_fast(&ptr, true, 0, 0, r_b);
        *ptr++ = 0xB8 + rd_e;
        x86_emit_imm64_fast(&ptr, inst->extended_imm);
        success = true;
        break;
    
    case MACH_ADD:
        r_r = x86_rex_r_for_reg(rn); r_b = x86_rex_b_for_reg(rd);
        x86_emit_rex_fast(&ptr, true, r_r, 0, r_b);
        *ptr++ = 0x03; x86_emit_modrm_fast(&ptr, 3, rn_e, rd_e);
        success = true;
        break;
    
    case MACH_ADD_IMM: {
        int32_t imm = (int32_t)inst->extended_imm;
        r_b = x86_rex_b_for_reg(rd);
        x86_emit_rex_fast(&ptr, true, 0, 0, r_b);
        if (imm >= -128 && imm <= 127) {
            *ptr++ = 0x83; x86_emit_modrm_fast(&ptr, 3, 0, rd_e); *ptr++ = (uint8_t)imm;
        } else {
            *ptr++ = 0x81; x86_emit_modrm_fast(&ptr, 3, 0, rd_e); x86_emit_imm32_fast(&ptr, (uint32_t)imm);
        }
        success = true;
        break;
    }
    
    case MACH_SUB:
        r_r = x86_rex_r_for_reg(rn); r_b = x86_rex_b_for_reg(rd);
        x86_emit_rex_fast(&ptr, true, r_r, 0, r_b);
        *ptr++ = 0x2B; x86_emit_modrm_fast(&ptr, 3, rn_e, rd_e);
        success = true;
        break;
    
    case MACH_SUB_IMM: {
        int32_t imm = (int32_t)inst->extended_imm;
        r_b = x86_rex_b_for_reg(rd);
        x86_emit_rex_fast(&ptr, true, 0, 0, r_b);
        if (imm >= -128 && imm <= 127) {
            *ptr++ = 0x83; x86_emit_modrm_fast(&ptr, 3, 5, rd_e); *ptr++ = (uint8_t)imm;
        } else {
            *ptr++ = 0x81; x86_emit_modrm_fast(&ptr, 3, 5, rd_e); x86_emit_imm32_fast(&ptr, (uint32_t)imm);
        }
        success = true;
        break;
    }
    
    case MACH_MUL:
        r_r = x86_rex_r_for_reg(rd); r_b = x86_rex_b_for_reg(rn);
        x86_emit_rex_fast(&ptr, true, r_r, 0, r_b);
        *ptr++ = 0x0F; *ptr++ = 0xAF; x86_emit_modrm_fast(&ptr, 3, rd_e, rn_e);
        success = true;
        break;
    
    case MACH_DIV: case MACH_UDIV:
        r_b = x86_rex_b_for_reg(rn);
        x86_emit_rex_fast(&ptr, true, 0, 0, r_b);
        *ptr++ = 0xF7; x86_emit_modrm_fast(&ptr, 3, 7, rn_e);
        success = true;
        break;
    
    case MACH_AND:
        r_r = x86_rex_r_for_reg(rn); r_b = x86_rex_b_for_reg(rd);
        x86_emit_rex_fast(&ptr, true, r_r, 0, r_b);
        *ptr++ = 0x23; x86_emit_modrm_fast(&ptr, 3, rn_e, rd_e);
        success = true;
        break;
    
    case MACH_OR:
        r_r = x86_rex_r_for_reg(rn); r_b = x86_rex_b_for_reg(rd);
        x86_emit_rex_fast(&ptr, true, r_r, 0, r_b);
        *ptr++ = 0x0B; x86_emit_modrm_fast(&ptr, 3, rn_e, rd_e);
        success = true;
        break;
    
    case MACH_XOR:
        r_r = x86_rex_r_for_reg(rn); r_b = x86_rex_b_for_reg(rd);
        x86_emit_rex_fast(&ptr, true, r_r, 0, r_b);
        *ptr++ = 0x33; x86_emit_modrm_fast(&ptr, 3, rn_e, rd_e);
        success = true;
        break;
    
    case MACH_SHL_IMM:
        r_b = x86_rex_b_for_reg(rd);
        x86_emit_rex_fast(&ptr, true, 0, 0, r_b);
        if (inst->extended_imm == 1) { *ptr++ = 0xD1; x86_emit_modrm_fast(&ptr, 3, 4, rd_e); }
        else { *ptr++ = 0xC1; x86_emit_modrm_fast(&ptr, 3, 4, rd_e); *ptr++ = (uint8_t)inst->extended_imm; }
        success = true;
        break;
    
    case MACH_LSHR_IMM:
        r_b = x86_rex_b_for_reg(rd);
        x86_emit_rex_fast(&ptr, true, 0, 0, r_b);
        *ptr++ = 0xC1; x86_emit_modrm_fast(&ptr, 3, 5, rd_e); *ptr++ = (uint8_t)inst->extended_imm;
        success = true;
        break;
    
    case MACH_ASHR_IMM:
        r_b = x86_rex_b_for_reg(rd);
        x86_emit_rex_fast(&ptr, true, 0, 0, r_b);
        *ptr++ = 0xC1; x86_emit_modrm_fast(&ptr, 3, 7, rd_e); *ptr++ = (uint8_t)inst->extended_imm;
        success = true;
        break;
    
    case MACH_CMP:
        r_r = x86_rex_r_for_reg(rm); r_b = x86_rex_b_for_reg(rn);
        x86_emit_rex_fast(&ptr, true, r_r, 0, r_b);
        *ptr++ = 0x3B; x86_emit_modrm_fast(&ptr, 3, rm_e, rn_e);
        success = true;
        break;
    
    case MACH_CMP_IMM: {
        int32_t imm = (int32_t)inst->extended_imm;
        r_b = x86_rex_b_for_reg(rn);
        x86_emit_rex_fast(&ptr, true, 0, 0, r_b);
        if (imm >= -128 && imm <= 127) {
            *ptr++ = 0x83; x86_emit_modrm_fast(&ptr, 3, 7, rn_e); *ptr++ = (uint8_t)imm;
        } else {
            *ptr++ = 0x81; x86_emit_modrm_fast(&ptr, 3, 7, rn_e); x86_emit_imm32_fast(&ptr, (uint32_t)imm);
        }
        success = true;
        break;
    }
    
    case MACH_SETCC: {
        static const uint8_t setcc[]={[COND_O]=0x90,[COND_NO]=0x91,[COND_B]=0x92,[COND_AE]=0x93,
            [COND_E]=0x94,[COND_NE]=0x95,[COND_BE]=0x96,[COND_A]=0x97,
            [COND_S]=0x98,[COND_NS]=0x99,[COND_P]=0x9A,[COND_NP]=0x9B,
            [COND_L]=0x9C,[COND_GE]=0x9D,[COND_LE]=0x9E,[COND_G]=0x9F};
        r_b = x86_rex_b_for_reg(rd);
        x86_emit_rex_fast(&ptr, false, 0, 0, r_b);
        *ptr++ = 0x0F; *ptr++ = setcc[inst->base.imm];
        x86_emit_modrm_fast(&ptr, 3, 0, rd_e);
        success = true;
        break;
    }
    
    case MACH_JMP:
        if (inst->extended_imm != 0) {
            int32_t off = (int32_t)inst->extended_imm;
            if (off >= -128 && off <= 127) { *ptr++ = 0xEB; *ptr++ = (uint8_t)off; }
            else { *ptr++ = 0xE9; x86_emit_imm32_fast(&ptr, (uint32_t)(off - 5)); }
            success = true;
        } else {
            r_b = x86_rex_b_for_reg(rn);
            x86_emit_rex_fast(&ptr, false, 0, 0, r_b);
            *ptr++ = 0xFF; x86_emit_modrm_fast(&ptr, 3, 4, rn_e);
            success = true;
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
        success = true;
        break;
    }
    
    case MACH_CALL:
        { int32_t off = (int32_t)inst->extended_imm; *ptr++ = 0xE8; x86_emit_imm32_fast(&ptr, (uint32_t)(off - 5)); success = true; }
        break;
    
    case MACH_INDIRECT_CALL:
        r_b = x86_rex_b_for_reg(rn);
        x86_emit_rex_fast(&ptr, false, 0, 0, r_b);
        *ptr++ = 0xFF; x86_emit_modrm_fast(&ptr, 3, 2, rn_e);
        success = true;
        break;
    
    case MACH_RET: *ptr++ = 0xC3; success = true; break;
    
    case MACH_PUSH:
        r_b = x86_rex_b_for_reg(rn);
        x86_emit_rex_fast(&ptr, false, 0, 0, r_b);
        *ptr++ = 0x50 + rn_e; success = true; break;
    
    case MACH_POP:
        r_b = x86_rex_b_for_reg(rd);
        x86_emit_rex_fast(&ptr, false, 0, 0, r_b);
        *ptr++ = 0x58 + rd_e; success = true; break;
    
    case MACH_LEA: {
        uint8_t r_r2 = x86_rex_r_for_reg(rd), r_x = x86_rex_r_for_reg(rn);
        uint8_t r_b2 = x86_rex_b_for_reg(rm);
        x86_emit_rex_fast(&ptr, true, r_r2, r_x, r_b2);
        *ptr++ = 0x8D;
        success = x86_encode_mem_fast(&ptr, rm, rn, inst->base.imm, inst->displacement, rd_e);
        break;
    }
    
    case MACH_LOAD:
        r_r = x86_rex_r_for_reg(rd); r_b = x86_rex_b_for_reg(rn);
        x86_emit_rex_fast(&ptr, true, r_r, 0, r_b);
        *ptr++ = 0x8B;
        success = x86_encode_mem_fast(&ptr, rn, REG_NONE, 0, inst->displacement, rd_e);
        break;
    
    case MACH_STORE:
        r_r = x86_rex_r_for_reg(rn); r_b = x86_rex_b_for_reg(rm);
        x86_emit_rex_fast(&ptr, true, r_r, 0, r_b);
        *ptr++ = 0x89;
        success = x86_encode_mem_fast(&ptr, rm, REG_NONE, 0, inst->displacement, rn_e);
        break;
    
    case MACH_CDQ: case MACH_CQO:
        x86_emit_rex_fast(&ptr, true, 0, 0, 0); *ptr++ = 0x99; success = true; break;
    
    case MACH_MOVSX:
        r_r = x86_rex_r_for_reg(rd); r_b = x86_rex_b_for_reg(rn);
        x86_emit_rex_fast(&ptr, true, r_r, 0, r_b);
        *ptr++ = 0x63; x86_emit_modrm_fast(&ptr, 3, rd_e, rn_e);
        success = true;
        break;
    
    case MACH_INT3: *ptr++ = 0xCC; success = true; break;
    
    case MACH_FADD:
        r_r = x86_rex_r_for_reg(rd); r_b = x86_rex_b_for_reg(rn);
        *ptr++ = 0xF2; x86_emit_rex_fast(&ptr, false, r_r, 0, r_b);
        *ptr++ = 0x0F; *ptr++ = 0x58; x86_emit_modrm_fast(&ptr, 3, rd_e, rn_e);
        success = true; break;
    
    case MACH_FSUB:
        r_r = x86_rex_r_for_reg(rd); r_b = x86_rex_b_for_reg(rn);
        *ptr++ = 0xF2; x86_emit_rex_fast(&ptr, false, r_r, 0, r_b);
        *ptr++ = 0x0F; *ptr++ = 0x5C; x86_emit_modrm_fast(&ptr, 3, rd_e, rn_e);
        success = true; break;
    
    case MACH_FMUL:
        r_r = x86_rex_r_for_reg(rd); r_b = x86_rex_b_for_reg(rn);
        *ptr++ = 0xF2; x86_emit_rex_fast(&ptr, false, r_r, 0, r_b);
        *ptr++ = 0x0F; *ptr++ = 0x59; x86_emit_modrm_fast(&ptr, 3, rd_e, rn_e);
        success = true; break;
    
    case MACH_FDIV:
        r_r = x86_rex_r_for_reg(rd); r_b = x86_rex_b_for_reg(rn);
        *ptr++ = 0xF2; x86_emit_rex_fast(&ptr, false, r_r, 0, r_b);
        *ptr++ = 0x0F; *ptr++ = 0x5E; x86_emit_modrm_fast(&ptr, 3, rd_e, rn_e);
        success = true; break;
    
    case MACH_FCMP:
        r_r = x86_rex_r_for_reg(rn); r_b = x86_rex_b_for_reg(rm);
        *ptr++ = 0x66; x86_emit_rex_fast(&ptr, false, r_r, 0, r_b);
        *ptr++ = 0x0F; *ptr++ = 0x2E; x86_emit_modrm_fast(&ptr, 3, rn_e, rm_e);
        success = true; break;
    
    case MACH_CVTSI2SD:
        r_r = x86_rex_r_for_reg(rd); r_b = x86_rex_b_for_reg(rn);
        *ptr++ = 0xF2; x86_emit_rex_fast(&ptr, true, r_r, 0, r_b);
        *ptr++ = 0x0F; *ptr++ = 0x2A; x86_emit_modrm_fast(&ptr, 3, rd_e, rn_e);
        success = true; break;
    
    case MACH_CVTSD2SI:
        r_r = x86_rex_r_for_reg(rd); r_b = x86_rex_b_for_reg(rn);
        *ptr++ = 0xF2; x86_emit_rex_fast(&ptr, true, r_r, 0, r_b);
        *ptr++ = 0x0F; *ptr++ = 0x2D; x86_emit_modrm_fast(&ptr, 3, rd_e, rn_e);
        success = true; break;
    
    default:
        *ptr++ = 0xCC; success = false; break;
    }
    
    *length = (uint32_t)(ptr - buffer);
    return success;
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
    SilverTarget *target = (SilverTarget *)calloc(1, sizeof(SilverTarget));
    if (!target) return NULL;
    target->arch = SILVER_TARGET_X86_64;
    target->name = "x86-64"; target->triple = "x86_64-unknown-none";
    target->is_64_bit = true; target->pointer_size = 8; target->max_alignment = 16;
    target->registers = x86_64_reg_descs;
    target->num_registers = sizeof(x86_64_reg_descs) / sizeof(x86_64_reg_descs[0]);
    target->cc.caller_saved = machine_reg_mask(REG_RAX)|machine_reg_mask(REG_RCX)|
        machine_reg_mask(REG_RDX)|machine_reg_mask(REG_RSI)|machine_reg_mask(REG_RDI)|
        machine_reg_mask(REG_R8)|machine_reg_mask(REG_R9)|machine_reg_mask(REG_R10)|machine_reg_mask(REG_R11);
    target->cc.callee_saved = machine_reg_mask(REG_RBX)|machine_reg_mask(REG_R12)|
        machine_reg_mask(REG_R13)|machine_reg_mask(REG_R14)|machine_reg_mask(REG_R15);
    target->cc.argument_regs[0]=machine_reg_mask(REG_RDI); target->cc.argument_regs[1]=machine_reg_mask(REG_RSI);
    target->cc.argument_regs[2]=machine_reg_mask(REG_RDX); target->cc.argument_regs[3]=machine_reg_mask(REG_RCX);
    target->cc.argument_regs[4]=machine_reg_mask(REG_R8);  target->cc.argument_regs[5]=machine_reg_mask(REG_R9);
    target->cc.return_reg = machine_reg_mask(REG_RAX); target->cc.return_reg_hi = machine_reg_mask(REG_RDX);
    target->cc.stack_alignment = 16; target->cc.red_zone_size = 128;
    target->available_gpr = machine_reg_mask(REG_RAX)|machine_reg_mask(REG_RCX)|machine_reg_mask(REG_RDX)|
        machine_reg_mask(REG_RBX)|machine_reg_mask(REG_RSI)|machine_reg_mask(REG_RDI)|
        machine_reg_mask(REG_R8)|machine_reg_mask(REG_R9)|machine_reg_mask(REG_R10)|machine_reg_mask(REG_R11)|
        machine_reg_mask(REG_R12)|machine_reg_mask(REG_R13)|machine_reg_mask(REG_R14)|machine_reg_mask(REG_R15);
    target->available_fpr = machine_reg_mask_range(REG_XMM0, REG_XMM15);
    target->sp_reg = REG_RSP; target->bp_reg = REG_RBP; target->ip_reg = REG_RIP; target->flag_reg = REG_NONE;
    target->encode = x86_encode_instruction;
    target->emit_prologue = x86_emit_prologue; target->emit_epilogue = x86_emit_epilogue;
    return target;
}

void x86_target_destroy(SilverTarget *target) { free(target); }