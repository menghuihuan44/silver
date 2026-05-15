#include "silver/target/arm/arm.h"
#include "silver/target/arm/arm_encode.h"
#include <stdlib.h>
#include <string.h>

// 寄存器映射
static const uint8_t arm64_reg_map[] = {
    [REG_RAX]=0,[REG_RCX]=1,[REG_RDX]=2,[REG_RBX]=3,[REG_RSI]=4,[REG_RDI]=5,
    [REG_R8]=8,[REG_R9]=9,[REG_R10]=10,[REG_R11]=11,[REG_R12]=12,[REG_R13]=13,
    [REG_R14]=14,[REG_R15]=15,[REG_RSP]=31,[REG_RBP]=29,
    [REG_XMM0]=0,[REG_XMM1]=1,[REG_XMM2]=2,[REG_XMM3]=3,
    [REG_XMM4]=4,[REG_XMM5]=5,[REG_XMM6]=6,[REG_XMM7]=7,
    [REG_XMM8]=8,[REG_XMM9]=9,[REG_XMM10]=10,[REG_XMM11]=11,
    [REG_XMM12]=12,[REG_XMM13]=13,[REG_XMM14]=14,[REG_XMM15]=15,
};

uint8_t arm64_reg_encoding(MachineRegister reg) { return (reg < REG_COUNT) ? arm64_reg_map[reg] : 0; }
bool arm64_is_gpr(MachineRegister reg) { return reg >= REG_RAX && reg <= REG_R15; }
bool arm64_is_fpr(MachineRegister reg) { return reg >= REG_XMM0 && reg <= REG_XMM15; }

ARM64Condition arm64_cond_from_machine(MachineCondition cond) {
    switch (cond) {
        case COND_E: return ARM64_COND_EQ; case COND_NE: return ARM64_COND_NE;
        case COND_L: return ARM64_COND_LT; case COND_LE: return ARM64_COND_LE;
        case COND_G: return ARM64_COND_GT; case COND_GE: return ARM64_COND_GE;
        case COND_B: return ARM64_COND_LO; case COND_BE: return ARM64_COND_LS;
        case COND_A: return ARM64_COND_HI; case COND_AE: return ARM64_COND_HS;
        case COND_ALWAYS: return ARM64_COND_AL; default: return ARM64_COND_AL;
    }
}

// ============================================================
// ARM64 指令编码（直接生成32位word）
// ============================================================
static inline uint32_t arm64_add_reg(bool sf, uint8_t rd, uint8_t rn, uint8_t rm) {
    return (sf ? 0x8B000000U : 0x0B000000U) | ((uint32_t)(rm & 0x1F) << 16) | ((uint32_t)(rn & 0x1F) << 5) | (rd & 0x1F);
}
static inline uint32_t arm64_sub_reg(bool sf, uint8_t rd, uint8_t rn, uint8_t rm) {
    return (sf ? 0xCB000000U : 0x4B000000U) | ((uint32_t)(rm & 0x1F) << 16) | ((uint32_t)(rn & 0x1F) << 5) | (rd & 0x1F);
}
static inline uint32_t arm64_and_reg(bool sf, uint8_t rd, uint8_t rn, uint8_t rm) {
    return (sf ? 0x8A000000U : 0x0A000000U) | ((uint32_t)(rm & 0x1F) << 16) | ((uint32_t)(rn & 0x1F) << 5) | (rd & 0x1F);
}
static inline uint32_t arm64_orr_reg(bool sf, uint8_t rd, uint8_t rn, uint8_t rm) {
    return (sf ? 0xAA000000U : 0x2A000000U) | ((uint32_t)(rm & 0x1F) << 16) | ((uint32_t)(rn & 0x1F) << 5) | (rd & 0x1F);
}
static inline uint32_t arm64_eor_reg(bool sf, uint8_t rd, uint8_t rn, uint8_t rm) {
    return (sf ? 0xCA000000U : 0x4A000000U) | ((uint32_t)(rm & 0x1F) << 16) | ((uint32_t)(rn & 0x1F) << 5) | (rd & 0x1F);
}
static inline uint32_t arm64_lsl_reg(bool sf, uint8_t rd, uint8_t rn, uint8_t rm) {
    return (sf ? 0x9AC02000U : 0x1AC02000U) | ((uint32_t)(rm & 0x1F) << 16) | ((uint32_t)(rn & 0x1F) << 5) | (rd & 0x1F);
}
static inline uint32_t arm64_lsr_reg(bool sf, uint8_t rd, uint8_t rn, uint8_t rm) {
    return (sf ? 0x9AC02400U : 0x1AC02400U) | ((uint32_t)(rm & 0x1F) << 16) | ((uint32_t)(rn & 0x1F) << 5) | (rd & 0x1F);
}
static inline uint32_t arm64_asr_reg(bool sf, uint8_t rd, uint8_t rn, uint8_t rm) {
    return (sf ? 0x9AC02800U : 0x1AC02800U) | ((uint32_t)(rm & 0x1F) << 16) | ((uint32_t)(rn & 0x1F) << 5) | (rd & 0x1F);
}
static inline uint32_t arm64_madd(bool sf, uint8_t rd, uint8_t rn, uint8_t rm, uint8_t ra) {
    return (sf ? 0x9B000000U : 0x1B000000U) | ((uint32_t)(rm & 0x1F) << 16) | ((uint32_t)(ra & 0x1F) << 10) | ((uint32_t)(rn & 0x1F) << 5) | (rd & 0x1F);
}
static inline uint32_t arm64_sdiv(bool sf, uint8_t rd, uint8_t rn, uint8_t rm) {
    return (sf ? 0x9AC00C00U : 0x1AC00C00U) | ((uint32_t)(rm & 0x1F) << 16) | ((uint32_t)(rn & 0x1F) << 5) | (rd & 0x1F);
}
static inline uint32_t arm64_udiv(bool sf, uint8_t rd, uint8_t rn, uint8_t rm) {
    return (sf ? 0x9AC00800U : 0x1AC00800U) | ((uint32_t)(rm & 0x1F) << 16) | ((uint32_t)(rn & 0x1F) << 5) | (rd & 0x1F);
}

static bool arm64_encode_ldr_str(bool is_load, bool sf, uint8_t rt, uint8_t rn, int32_t offset, uint32_t *word) {
    int32_t scaled = sf ? (offset >> 3) : (offset >> 2);
    if (scaled < 0 || scaled > 4095) return false;
    uint32_t size = sf ? 3 : 2;
    *word = (size << 30) | (0x1U << 27) | (0x1U << 24) | (is_load ? (1U << 22) : 0) |
            ((uint32_t)(scaled & 0xFFF) << 10) | ((uint32_t)(rn & 0x1F) << 5) | (rt & 0x1F);
    return true;
}

// ============================================================
// 序言/尾声
// ============================================================
uint32_t arm64_emit_prologue(const SilverTarget *target, IRFunction *func, uint8_t *buffer) {
    if (!target || !func || !buffer) return 0;
    uint8_t *ptr = buffer;
    uint32_t w = 0xA9BF7BF0U; // STP X29, X30, [SP, #-16]!
    *ptr++=w&0xFF; *ptr++=(w>>8)&0xFF; *ptr++=(w>>16)&0xFF; *ptr++=(w>>24)&0xFF;
    w = 0x910003FDU; // MOV X29, SP
    *ptr++=w&0xFF; *ptr++=(w>>8)&0xFF; *ptr++=(w>>16)&0xFF; *ptr++=(w>>24)&0xFF;
    if (func->stack_size > 0) {
        w = arm64_sub_reg(true, 31, 31, 0); // 实际需要处理立即数
        // 简化：使用 SUB SP, SP, #imm (需要编码立即数)
        (void)func;
    }
    return (uint32_t)(ptr - buffer);
}

uint32_t arm64_emit_epilogue(const SilverTarget *target, IRFunction *func, uint8_t *buffer) {
    if (!target || !func || !buffer) return 0;
    uint8_t *ptr = buffer;
    (void)func;
    uint32_t w = 0xA8C17BF0U; // LDP X29, X30, [SP], #16
    *ptr++=w&0xFF; *ptr++=(w>>8)&0xFF; *ptr++=(w>>16)&0xFF; *ptr++=(w>>24)&0xFF;
    w = 0xD65F03C0U; // RET
    *ptr++=w&0xFF; *ptr++=(w>>8)&0xFF; *ptr++=(w>>16)&0xFF; *ptr++=(w>>24)&0xFF;
    return (uint32_t)(ptr - buffer);
}

// ============================================================
// 完整编码函数
// ============================================================
bool arm64_encode_instruction(const SilverTarget *target,
                               const MachineInstExt *inst,
                               uint8_t *buffer, uint32_t *length) {
    if (!target || !inst || !buffer || !length) return false;
    uint32_t word = 0xD503201FU; // NOP default
    bool success = true;
    bool sf = true;
    uint8_t rd = arm64_reg_encoding((MachineRegister)inst->base.rd);
    uint8_t rn = arm64_reg_encoding((MachineRegister)inst->base.rn);
    uint8_t rm = arm64_reg_encoding((MachineRegister)inst->base.rm);
    
    switch (inst->base.opcode) {
    case MACH_NOP: word = 0xD503201FU; break;
    case MACH_MOV: word = arm64_orr_reg(sf, rd, 31, rn); break;
    case MACH_ADD: word = arm64_add_reg(sf, rd, rn, rm); break;
    case MACH_SUB: word = arm64_sub_reg(sf, rd, rn, rm); break;
    case MACH_MUL: word = arm64_madd(sf, rd, rn, rm, 31); break;
    case MACH_DIV: word = arm64_sdiv(sf, rd, rn, rm); break;
    case MACH_UDIV: word = arm64_udiv(sf, rd, rn, rm); break;
    case MACH_AND: word = arm64_and_reg(sf, rd, rn, rm); break;
    case MACH_OR:  word = arm64_orr_reg(sf, rd, rn, rm); break;
    case MACH_XOR: word = arm64_eor_reg(sf, rd, rn, rm); break;
    case MACH_SHL: word = arm64_lsl_reg(sf, rd, rn, rm); break;
    case MACH_LSHR: word = arm64_lsr_reg(sf, rd, rn, rm); break;
    case MACH_ASHR: word = arm64_asr_reg(sf, rd, rn, rm); break;
    case MACH_CMP: word = arm64_sub_reg(sf, 31, rn, rm); break; // SUBS XZR, rn, rm
    case MACH_LOAD: success = arm64_encode_ldr_str(true, sf, rd, rn, inst->displacement, &word); break;
    case MACH_STORE: success = arm64_encode_ldr_str(false, sf, rn, rm, inst->displacement, &word); break;
    case MACH_RET: word = 0xD65F03C0U; break;
    case MACH_JMP:
        if (inst->extended_imm != 0) {
            int32_t off = (int32_t)inst->extended_imm >> 2;
            word = 0x14000000U | ((uint32_t)(off & 0x3FFFFFF));
        } else { word = 0xD61F0000U | ((uint32_t)(rn & 0x1F) << 5); }
        break;
    case MACH_CALL:
        { int32_t off = (int32_t)inst->extended_imm >> 2;
          word = 0x94000000U | ((uint32_t)(off & 0x3FFFFFF)); }
        break;
    case MACH_INDIRECT_CALL: word = 0xD63F0000U | ((uint32_t)(rn & 0x1F) << 5); break;
    case MACH_FADD: word = (sf ? 0x1E202800U : 0x1E202800U) | ((uint32_t)(rm & 0x1F) << 16) | ((uint32_t)(rn & 0x1F) << 5) | (rd & 0x1F); break;
    case MACH_FSUB: word = (sf ? 0x1E203800U : 0x1E203800U) | ((uint32_t)(rm & 0x1F) << 16) | ((uint32_t)(rn & 0x1F) << 5) | (rd & 0x1F); break;
    case MACH_FMUL: word = (sf ? 0x1E200800U : 0x1E200800U) | ((uint32_t)(rm & 0x1F) << 16) | ((uint32_t)(rn & 0x1F) << 5) | (rd & 0x1F); break;
    case MACH_FDIV: word = (sf ? 0x1E201800U : 0x1E201800U) | ((uint32_t)(rm & 0x1F) << 16) | ((uint32_t)(rn & 0x1F) << 5) | (rd & 0x1F); break;
    default: success = false; break;
    }
    
    buffer[0] = (uint8_t)(word & 0xFF); buffer[1] = (uint8_t)((word >> 8) & 0xFF);
    buffer[2] = (uint8_t)((word >> 16) & 0xFF); buffer[3] = (uint8_t)((word >> 24) & 0xFF);
    *length = 4;
    return success;
}

// ARM64寄存器描述表
static const RegisterDesc arm64_reg_descs[] = {
    [REG_NONE]={REG_NONE,REG_CLASS_NONE,"none",0,false,false,false,0},
    [REG_RAX]={REG_RAX,REG_CLASS_GPR,"x0",0,true,false,false,0},
    [REG_RCX]={REG_RCX,REG_CLASS_GPR,"x1",1,true,false,false,0},
    [REG_RDX]={REG_RDX,REG_CLASS_GPR,"x2",2,true,false,false,0},
    [REG_RBX]={REG_RBX,REG_CLASS_GPR,"x3",3,true,false,false,0},
    [REG_RSI]={REG_RSI,REG_CLASS_GPR,"x4",4,true,false,false,0},
    [REG_RDI]={REG_RDI,REG_CLASS_GPR,"x5",5,true,false,false,0},
    [REG_R8] ={REG_R8, REG_CLASS_GPR,"x6",6,true,false,false,0},
    [REG_R9] ={REG_R9, REG_CLASS_GPR,"x7",7,true,false,false,0},
    [REG_R10]={REG_R10,REG_CLASS_GPR,"x16",16,true,false,false,0},
    [REG_R11]={REG_R11,REG_CLASS_GPR,"x17",17,true,false,false,0},
    [REG_R12]={REG_R12,REG_CLASS_GPR,"x9",9,false,true,false,0},
    [REG_R13]={REG_R13,REG_CLASS_GPR,"x10",10,false,true,false,0},
    [REG_R14]={REG_R14,REG_CLASS_GPR,"x11",11,false,true,false,0},
    [REG_R15]={REG_R15,REG_CLASS_GPR,"x12",12,false,true,false,0},
    [REG_RSP]={REG_RSP,REG_CLASS_SPECIAL,"sp",31,false,false,true,0},
    [REG_RBP]={REG_RBP,REG_CLASS_SPECIAL,"fp",29,false,false,true,0},
    [REG_XMM0]={REG_XMM0,REG_CLASS_FPR,"v0",0,true,false,false,0},
    [REG_XMM1]={REG_XMM1,REG_CLASS_FPR,"v1",1,true,false,false,0},
    [REG_XMM2]={REG_XMM2,REG_CLASS_FPR,"v2",2,true,false,false,0},
    [REG_XMM3]={REG_XMM3,REG_CLASS_FPR,"v3",3,true,false,false,0},
    [REG_XMM4]={REG_XMM4,REG_CLASS_FPR,"v4",4,true,false,false,0},
    [REG_XMM5]={REG_XMM5,REG_CLASS_FPR,"v5",5,true,false,false,0},
    [REG_XMM6]={REG_XMM6,REG_CLASS_FPR,"v6",6,true,false,false,0},
    [REG_XMM7]={REG_XMM7,REG_CLASS_FPR,"v7",7,true,false,false,0},
    [REG_XMM8]={REG_XMM8,REG_CLASS_FPR,"v8",8,false,true,false,0},
    [REG_XMM9]={REG_XMM9,REG_CLASS_FPR,"v9",9,false,true,false,0},
    [REG_XMM10]={REG_XMM10,REG_CLASS_FPR,"v10",10,false,true,false,0},
    [REG_XMM11]={REG_XMM11,REG_CLASS_FPR,"v11",11,false,true,false,0},
    [REG_XMM12]={REG_XMM12,REG_CLASS_FPR,"v12",12,false,true,false,0},
    [REG_XMM13]={REG_XMM13,REG_CLASS_FPR,"v13",13,false,true,false,0},
    [REG_XMM14]={REG_XMM14,REG_CLASS_FPR,"v14",14,false,true,false,0},
    [REG_XMM15]={REG_XMM15,REG_CLASS_FPR,"v15",15,false,true,false,0},
};

SilverTarget *arm64_target_create(void) {
    SilverTarget *t = (SilverTarget *)calloc(1, sizeof(SilverTarget));
    if (!t) return NULL;
    t->arch = SILVER_TARGET_ARM64; t->name = "ARM64"; t->triple = "aarch64-unknown-none";
    t->is_64_bit = true; t->pointer_size = 8; t->max_alignment = 16;
    t->registers = arm64_reg_descs;
    t->num_registers = sizeof(arm64_reg_descs) / sizeof(arm64_reg_descs[0]);
    t->cc.argument_regs[0]=machine_reg_mask(REG_RAX); t->cc.argument_regs[1]=machine_reg_mask(REG_RCX);
    t->cc.argument_regs[2]=machine_reg_mask(REG_RDX); t->cc.argument_regs[3]=machine_reg_mask(REG_RBX);
    t->cc.argument_regs[4]=machine_reg_mask(REG_RSI); t->cc.argument_regs[5]=machine_reg_mask(REG_RDI);
    t->cc.argument_regs[6]=machine_reg_mask(REG_R8);  t->cc.argument_regs[7]=machine_reg_mask(REG_R9);
    t->cc.return_reg = machine_reg_mask(REG_RAX); t->cc.return_reg_hi = machine_reg_mask(REG_RCX);
    t->cc.stack_alignment = 16; t->cc.red_zone_size = 0;
    t->cc.caller_saved = machine_reg_mask(REG_RAX)|machine_reg_mask(REG_RCX)|machine_reg_mask(REG_RDX)|
        machine_reg_mask(REG_RBX)|machine_reg_mask(REG_RSI)|machine_reg_mask(REG_RDI)|
        machine_reg_mask(REG_R8)|machine_reg_mask(REG_R9)|machine_reg_mask(REG_R10)|machine_reg_mask(REG_R11)|
        machine_reg_mask(REG_R12)|machine_reg_mask(REG_R13)|machine_reg_mask(REG_R14)|machine_reg_mask(REG_R15);
    t->cc.callee_saved = 0;
    t->available_gpr = machine_reg_mask(REG_RAX)|machine_reg_mask(REG_RCX)|machine_reg_mask(REG_RDX)|
        machine_reg_mask(REG_RBX)|machine_reg_mask(REG_RSI)|machine_reg_mask(REG_RDI)|
        machine_reg_mask(REG_R8)|machine_reg_mask(REG_R9)|machine_reg_mask(REG_R10)|machine_reg_mask(REG_R11)|
        machine_reg_mask(REG_R12)|machine_reg_mask(REG_R13)|machine_reg_mask(REG_R14)|machine_reg_mask(REG_R15);
    t->available_fpr = machine_reg_mask_range(REG_XMM0, REG_XMM15);
    t->sp_reg = REG_RSP; t->bp_reg = REG_RBP; t->ip_reg = REG_RIP;
    t->encode = arm64_encode_instruction;
    t->emit_prologue = arm64_emit_prologue; t->emit_epilogue = arm64_emit_epilogue;
    return t;
}
void arm64_target_destroy(SilverTarget *target) { free(target); }