#include "silver/target/riscv/riscv.h"
#include "silver/target/riscv/riscv_encode.h"
#include <stdlib.h>
#include <string.h>

static const uint8_t riscv_reg_map[] = {
    [REG_RAX]=10,[REG_RCX]=11,[REG_RDX]=12,[REG_RBX]=13,[REG_RSI]=14,[REG_RDI]=15,
    [REG_R8]=16,[REG_R9]=17,[REG_R10]=5,[REG_R11]=6,[REG_R12]=7,[REG_R13]=28,
    [REG_R14]=29,[REG_R15]=30,[REG_RSP]=2,[REG_RBP]=8,
};

uint8_t riscv64_reg_encoding(MachineRegister reg) { return (reg < REG_COUNT) ? riscv_reg_map[reg] : 0; }

// ============================================================
// RISC-V 指令字生成内联函数
// ============================================================
#define RV_R(opcode,rd,funct3,rs1,rs2,funct7) \
    (((uint32_t)(opcode)&0x7F)|(((uint32_t)(rd)&0x1F)<<7)|(((uint32_t)(funct3)&0x7)<<12)| \
     (((uint32_t)(rs1)&0x1F)<<15)|(((uint32_t)(rs2)&0x1F)<<20)|(((uint32_t)(funct7)&0x7F)<<25))

#define RV_I(opcode,rd,funct3,rs1,imm) \
    (((uint32_t)(opcode)&0x7F)|(((uint32_t)(rd)&0x1F)<<7)|(((uint32_t)(funct3)&0x7)<<12)| \
     (((uint32_t)(rs1)&0x1F)<<15)|(((uint32_t)(imm)&0xFFF)<<20))

#define RV_S(opcode,funct3,rs2,rs1,imm) \
    (((uint32_t)(opcode)&0x7F)|(((uint32_t)(imm)&0x1F)<<7)|(((uint32_t)(funct3)&0x7)<<12)| \
     (((uint32_t)(rs1)&0x1F)<<15)|(((uint32_t)(rs2)&0x1F)<<20)|(((uint32_t)((imm)>>5)&0x7F)<<25))

#define RV_B(opcode,funct3,rs1,rs2,offset) do { \
    uint32_t _off=(uint32_t)(offset)&0x1FFE; \
    word=((uint32_t)(opcode)&0x7F)|(((_off>>11)&1)<<7)|(((_off>>1)&0xF)<<8)| \
         (((uint32_t)(funct3)&0x7)<<12)|(((uint32_t)(rs1)&0x1F)<<15)| \
         (((uint32_t)(rs2)&0x1F)<<20)|(((_off>>5)&0x3F)<<25)|(((_off>>12)&1)<<31); \
} while(0)

#define RV_J(opcode,rd,offset) do { \
    uint32_t _off=(uint32_t)(offset)&0x1FFFFF; \
    word=((uint32_t)(opcode)&0x7F)|(((uint32_t)(rd)&0x1F)<<7)| \
         (((_off>>12)&0xFF)<<12)|(((_off>>11)&1)<<20)|(((_off>>1)&0x3FF)<<21)|(((_off>>20)&1)<<31); \
} while(0)

// ============================================================
// 序言/尾声
// ============================================================
uint32_t riscv64_emit_prologue(const SilverTarget *target, IRFunction *func, uint8_t *buffer) {
    if (!target || !func || !buffer) return 0;
    uint8_t *ptr = buffer;
    uint32_t w = RV_I(0x13, 2, 0, 2, -16 & 0xFFF); // ADDI SP, SP, -16
    *ptr++=w&0xFF; *ptr++=(w>>8)&0xFF; *ptr++=(w>>16)&0xFF; *ptr++=(w>>24)&0xFF;
    w = RV_S(0x23, 3, 1, 2, 8); // SD RA, 8(SP)
    *ptr++=w&0xFF; *ptr++=(w>>8)&0xFF; *ptr++=(w>>16)&0xFF; *ptr++=(w>>24)&0xFF;
    w = RV_S(0x23, 3, 8, 2, 0); // SD FP, 0(SP)
    *ptr++=w&0xFF; *ptr++=(w>>8)&0xFF; *ptr++=(w>>16)&0xFF; *ptr++=(w>>24)&0xFF;
    w = RV_I(0x13, 8, 0, 2, 16); // ADDI FP, SP, 16
    *ptr++=w&0xFF; *ptr++=(w>>8)&0xFF; *ptr++=(w>>16)&0xFF; *ptr++=(w>>24)&0xFF;
    (void)func;
    return (uint32_t)(ptr - buffer);
}

uint32_t riscv64_emit_epilogue(const SilverTarget *target, IRFunction *func, uint8_t *buffer) {
    if (!target || !func || !buffer) return 0;
    uint8_t *ptr = buffer;
    (void)func;
    uint32_t w = RV_I(0x03, 8, 3, 2, 0); // LD FP, 0(SP)
    *ptr++=w&0xFF; *ptr++=(w>>8)&0xFF; *ptr++=(w>>16)&0xFF; *ptr++=(w>>24)&0xFF;
    w = RV_I(0x03, 1, 3, 2, 8); // LD RA, 8(SP)
    *ptr++=w&0xFF; *ptr++=(w>>8)&0xFF; *ptr++=(w>>16)&0xFF; *ptr++=(w>>24)&0xFF;
    w = RV_I(0x13, 2, 0, 2, 16); // ADDI SP, SP, 16
    *ptr++=w&0xFF; *ptr++=(w>>8)&0xFF; *ptr++=(w>>16)&0xFF; *ptr++=(w>>24)&0xFF;
    w = RV_I(0x67, 0, 0, 1, 0); // JALR X0, RA, 0
    *ptr++=w&0xFF; *ptr++=(w>>8)&0xFF; *ptr++=(w>>16)&0xFF; *ptr++=(w>>24)&0xFF;
    return (uint32_t)(ptr - buffer);
}

// ============================================================
// 完整编码函数
// ============================================================
bool riscv64_encode_instruction(const SilverTarget *target,
                                 const MachineInstExt *inst,
                                 uint8_t *buffer, uint32_t *length) {
    if (!target || !inst || !buffer || !length) return false;
    uint32_t word = 0x00000013U; // NOP
    bool success = true;
    uint8_t rd = riscv64_reg_encoding((MachineRegister)inst->base.rd);
    uint8_t rn = riscv64_reg_encoding((MachineRegister)inst->base.rn);
    uint8_t rm = riscv64_reg_encoding((MachineRegister)inst->base.rm);
    
    switch (inst->base.opcode) {
    case MACH_NOP: word = RV_I(0x13,0,0,0,0); break;
    case MACH_MOV:
        if (rn != REG_NONE) word = RV_I(0x13, rd, 0, rn, 0);
        else word = RV_I(0x13, rd, 0, 0, 0); // 简化li
        break;
    case MACH_ADD: word = RV_R(0x33, rd, 0, rn, rm, 0x00); break;
    case MACH_SUB: word = RV_R(0x33, rd, 0, rn, rm, 0x20); break;
    case MACH_MUL: word = RV_R(0x33, rd, 0, rn, rm, 0x01); break;
    case MACH_DIV: word = RV_R(0x33, rd, 4, rn, rm, 0x01); break;
    case MACH_UDIV: word = RV_R(0x33, rd, 5, rn, rm, 0x01); break;
    case MACH_REM: word = RV_R(0x33, rd, 6, rn, rm, 0x01); break;
    case MACH_UREM: word = RV_R(0x33, rd, 7, rn, rm, 0x01); break;
    case MACH_AND: word = RV_R(0x33, rd, 7, rn, rm, 0x00); break;
    case MACH_OR:  word = RV_R(0x33, rd, 6, rn, rm, 0x00); break;
    case MACH_XOR: word = RV_R(0x33, rd, 4, rn, rm, 0x00); break;
    case MACH_SHL: word = RV_R(0x33, rd, 1, rn, rm, 0x00); break;
    case MACH_LSHR: word = RV_R(0x33, rd, 5, rn, rm, 0x00); break;
    case MACH_ASHR: word = RV_R(0x33, rd, 5, rn, rm, 0x20); break;
    case MACH_CMP: word = RV_R(0x33, 0, 0, rn, rm, 0x20); break; // SUB X0, rn, rm
    case MACH_LOAD: word = RV_I(0x03, rd, 3, rn, inst->displacement & 0xFFF); break;
    case MACH_STORE: word = RV_S(0x23, 3, rn, rm, inst->displacement); break;
    case MACH_RET: word = RV_I(0x67, 0, 0, 1, 0); break;
    case MACH_JMP: RV_J(0x6F, 0, (int32_t)inst->extended_imm); break;
    case MACH_JCC: {
        MachineCondition cond = (MachineCondition)inst->base.imm;
        switch (cond) {
            case COND_E: RV_B(0x63, 0, rn, rm, (int32_t)inst->extended_imm); break;
            case COND_NE: RV_B(0x63, 1, rn, rm, (int32_t)inst->extended_imm); break;
            case COND_L: RV_B(0x63, 4, rn, rm, (int32_t)inst->extended_imm); break;
            case COND_GE: RV_B(0x63, 5, rn, rm, (int32_t)inst->extended_imm); break;
            case COND_B: RV_B(0x63, 6, rn, rm, (int32_t)inst->extended_imm); break;
            case COND_AE: RV_B(0x63, 7, rn, rm, (int32_t)inst->extended_imm); break;
            case COND_LE: RV_B(0x63, 5, rm, rn, (int32_t)inst->extended_imm); break;
            case COND_G: RV_B(0x63, 4, rm, rn, (int32_t)inst->extended_imm); break;
            case COND_A: RV_B(0x63, 7, rm, rn, (int32_t)inst->extended_imm); break;
            case COND_BE: RV_B(0x63, 6, rm, rn, (int32_t)inst->extended_imm); break;
            default: success = false; break;
        }
        break;
    }
    case MACH_CALL: RV_J(0x6F, 1, (int32_t)inst->extended_imm); break;
    case MACH_INDIRECT_CALL: word = RV_I(0x67, 1, 0, rn, 0); break;
    case MACH_FADD: word = RV_R(0x53, rd, 0, rn, rm, 0x01) | (1U<<26); break;
    case MACH_FSUB: word = RV_R(0x53, rd, 0, rn, rm, 0x05) | (1U<<26); break;
    case MACH_FMUL: word = RV_R(0x53, rd, 0, rn, rm, 0x09) | (1U<<26); break;
    case MACH_FDIV: word = RV_R(0x53, rd, 0, rn, rm, 0x0D) | (1U<<26); break;
    default: success = false; break;
    }
    
    buffer[0] = (uint8_t)(word & 0xFF); buffer[1] = (uint8_t)((word >> 8) & 0xFF);
    buffer[2] = (uint8_t)((word >> 16) & 0xFF); buffer[3] = (uint8_t)((word >> 24) & 0xFF);
    *length = 4;
    return success;
}

// RISC-V寄存器描述表
static const RegisterDesc riscv64_reg_descs[] = {
    [REG_NONE]={REG_NONE,REG_CLASS_NONE,"none",0,false,false,false,0},
    [REG_RAX]={REG_RAX,REG_CLASS_GPR,"a0",10,true,false,false,0},
    [REG_RCX]={REG_RCX,REG_CLASS_GPR,"a1",11,true,false,false,0},
    [REG_RDX]={REG_RDX,REG_CLASS_GPR,"a2",12,true,false,false,0},
    [REG_RBX]={REG_RBX,REG_CLASS_GPR,"a3",13,true,false,false,0},
    [REG_RSI]={REG_RSI,REG_CLASS_GPR,"a4",14,true,false,false,0},
    [REG_RDI]={REG_RDI,REG_CLASS_GPR,"a5",15,true,false,false,0},
    [REG_R8] ={REG_R8, REG_CLASS_GPR,"a6",16,true,false,false,0},
    [REG_R9] ={REG_R9, REG_CLASS_GPR,"a7",17,true,false,false,0},
    [REG_R10]={REG_R10,REG_CLASS_GPR,"t0",5,true,false,false,0},
    [REG_R11]={REG_R11,REG_CLASS_GPR,"t1",6,true,false,false,0},
    [REG_R12]={REG_R12,REG_CLASS_GPR,"t2",7,true,false,false,0},
    [REG_R13]={REG_R13,REG_CLASS_GPR,"s0",8,false,true,false,0},
    [REG_R14]={REG_R14,REG_CLASS_GPR,"s1",9,false,true,false,0},
    [REG_R15]={REG_R15,REG_CLASS_GPR,"s2",18,false,true,false,0},
    [REG_RSP]={REG_RSP,REG_CLASS_SPECIAL,"sp",2,false,false,true,0},
    [REG_RBP]={REG_RBP,REG_CLASS_SPECIAL,"fp",8,false,false,true,0},
    [REG_XMM0]={REG_XMM0,REG_CLASS_FPR,"f0",0,true,false,false,0},
    [REG_XMM1]={REG_XMM1,REG_CLASS_FPR,"f1",1,true,false,false,0},
    [REG_XMM2]={REG_XMM2,REG_CLASS_FPR,"f2",2,true,false,false,0},
    [REG_XMM3]={REG_XMM3,REG_CLASS_FPR,"f3",3,true,false,false,0},
    [REG_XMM4]={REG_XMM4,REG_CLASS_FPR,"f4",4,true,false,false,0},
    [REG_XMM5]={REG_XMM5,REG_CLASS_FPR,"f5",5,true,false,false,0},
    [REG_XMM6]={REG_XMM6,REG_CLASS_FPR,"f6",6,true,false,false,0},
    [REG_XMM7]={REG_XMM7,REG_CLASS_FPR,"f7",7,true,false,false,0},
    [REG_XMM8]={REG_XMM8,REG_CLASS_FPR,"f8",8,false,true,false,0},
    [REG_XMM9]={REG_XMM9,REG_CLASS_FPR,"f9",9,false,true,false,0},
    [REG_XMM10]={REG_XMM10,REG_CLASS_FPR,"f10",10,false,true,false,0},
    [REG_XMM11]={REG_XMM11,REG_CLASS_FPR,"f11",11,false,true,false,0},
    [REG_XMM12]={REG_XMM12,REG_CLASS_FPR,"f12",12,false,true,false,0},
    [REG_XMM13]={REG_XMM13,REG_CLASS_FPR,"f13",13,false,true,false,0},
    [REG_XMM14]={REG_XMM14,REG_CLASS_FPR,"f14",14,false,true,false,0},
    [REG_XMM15]={REG_XMM15,REG_CLASS_FPR,"f15",15,false,true,false,0},
};

SilverTarget *riscv64_target_create(void) {
    SilverTarget *t = (SilverTarget *)calloc(1, sizeof(SilverTarget));
    if (!t) return NULL;
    t->arch = SILVER_TARGET_RISCV64; t->name = "RISC-V64"; t->triple = "riscv64-unknown-none";
    t->is_64_bit = true; t->pointer_size = 8; t->max_alignment = 16;
    t->registers = riscv64_reg_descs;
    t->num_registers = sizeof(riscv64_reg_descs) / sizeof(riscv64_reg_descs[0]);
    t->cc.argument_regs[0]=machine_reg_mask(REG_RAX); t->cc.argument_regs[1]=machine_reg_mask(REG_RCX);
    t->cc.argument_regs[2]=machine_reg_mask(REG_RDX); t->cc.argument_regs[3]=machine_reg_mask(REG_RBX);
    t->cc.argument_regs[4]=machine_reg_mask(REG_RSI); t->cc.argument_regs[5]=machine_reg_mask(REG_RDI);
    t->cc.argument_regs[6]=machine_reg_mask(REG_R8);  t->cc.argument_regs[7]=machine_reg_mask(REG_R9);
    t->cc.return_reg = machine_reg_mask(REG_RAX); t->cc.return_reg_hi = machine_reg_mask(REG_RCX);
    t->cc.stack_alignment = 16; t->cc.red_zone_size = 0;
    t->available_gpr = machine_reg_mask(REG_RAX)|machine_reg_mask(REG_RCX)|machine_reg_mask(REG_RDX)|
        machine_reg_mask(REG_RBX)|machine_reg_mask(REG_RSI)|machine_reg_mask(REG_RDI)|
        machine_reg_mask(REG_R8)|machine_reg_mask(REG_R9)|machine_reg_mask(REG_R10)|machine_reg_mask(REG_R11)|
        machine_reg_mask(REG_R12)|machine_reg_mask(REG_R13)|machine_reg_mask(REG_R14)|machine_reg_mask(REG_R15);
    t->available_fpr = machine_reg_mask_range(REG_XMM0, REG_XMM15);
    t->sp_reg = REG_RSP; t->bp_reg = REG_RBP; t->ip_reg = REG_RIP;
    t->encode = riscv64_encode_instruction;
    t->emit_prologue = riscv64_emit_prologue; t->emit_epilogue = riscv64_emit_epilogue;
    return t;
}
void riscv64_target_destroy(SilverTarget *target) { free(target); }