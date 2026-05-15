#include "silver/codegen/machine.h"
#include <string.h>

const RegisterDesc *machine_get_register_desc(const SilverTarget *target,
                                               MachineRegister reg) {
    if (!target || !target->registers || reg >= target->num_registers) return NULL;
    return &target->registers[reg];
}

const char *machine_reg_name(const SilverTarget *target, MachineRegister reg) {
    const RegisterDesc *desc = machine_get_register_desc(target, reg);
    return desc ? desc->name : "???";
}

RegisterSet machine_reg_mask(MachineRegister reg) {
    if (reg >= REG_COUNT) return REG_SET_EMPTY;
    return 1ULL << reg;
}

RegisterSet machine_reg_mask_range(MachineRegister start, MachineRegister end) {
    RegisterSet mask = 0;
    for (int i = start; i <= end; i++) {
        mask |= (1ULL << i);
    }
    return mask;
}

const char *machine_opcode_name(MachineOpcode opcode) {
    static const char *names[] = {
        [MACH_NOP] = "nop",
        [MACH_COPY] = "mov",
        [MACH_ADD] = "add",
        [MACH_ADD_IMM] = "add_imm",
        [MACH_SUB] = "sub",
        [MACH_SUB_IMM] = "sub_imm",
        [MACH_MUL] = "mul",
        [MACH_MUL_IMM] = "mul_imm",
        [MACH_DIV] = "div",
        [MACH_UDIV] = "udiv",
        [MACH_REM] = "rem",
        [MACH_NEG] = "neg",
        [MACH_AND] = "and",
        [MACH_AND_IMM] = "and_imm",
        [MACH_OR] = "or",
        [MACH_OR_IMM] = "or_imm",
        [MACH_XOR] = "xor",
        [MACH_XOR_IMM] = "xor_imm",
        [MACH_SHL] = "shl",
        [MACH_SHL_IMM] = "shl_imm",
        [MACH_LSHR] = "shr",
        [MACH_LSHR_IMM] = "shr_imm",
        [MACH_ASHR] = "sar",
        [MACH_ASHR_IMM] = "sar_imm",
        [MACH_NOT] = "not",
        [MACH_FADD] = "addsd",
        [MACH_FSUB] = "subsd",
        [MACH_FMUL] = "mulsd",
        [MACH_FDIV] = "divsd",
        [MACH_FNEG] = "fneg",
        [MACH_CMP] = "cmp",
        [MACH_CMP_IMM] = "cmp_imm",
        [MACH_TEST] = "test",
        [MACH_SETCC] = "setcc",
        [MACH_MOV] = "mov",
        [MACH_MOV_IMM] = "mov_imm",
        [MACH_LOAD] = "load",
        [MACH_STORE] = "store",
        [MACH_LEA] = "lea",
        [MACH_PUSH] = "push",
        [MACH_POP] = "pop",
        [MACH_JMP] = "jmp",
        [MACH_JCC] = "jcc",
        [MACH_CALL] = "call",
        [MACH_RET] = "ret",
    };
    
    if (opcode >= MACH_OP_COUNT || !names[opcode]) return "unknown";
    return names[opcode];
}

const char *machine_cond_name(MachineCondition cond) {
    static const char *names[] = {
        [COND_NONE] = "",
        [COND_E] = "e",
        [COND_NE] = "ne",
        [COND_L] = "l",
        [COND_LE] = "le",
        [COND_G] = "g",
        [COND_GE] = "ge",
        [COND_B] = "b",
        [COND_BE] = "be",
        [COND_A] = "a",
        [COND_AE] = "ae",
        [COND_S] = "s",
        [COND_NS] = "ns",
        [COND_O] = "o",
        [COND_NO] = "no",
        [COND_P] = "p",
        [COND_NP] = "np",
        [COND_ALWAYS] = "",
    };
    
    if (cond >= COND_COUNT || !names[cond]) return "??";
    return names[cond];
}

MachineCondition machine_cond_invert(MachineCondition cond) {
    switch (cond) {
        case COND_E:  return COND_NE;
        case COND_NE: return COND_E;
        case COND_L:  return COND_GE;
        case COND_LE: return COND_G;
        case COND_G:  return COND_LE;
        case COND_GE: return COND_L;
        case COND_B:  return COND_AE;
        case COND_BE: return COND_A;
        case COND_A:  return COND_BE;
        case COND_AE: return COND_B;
        case COND_S:  return COND_NS;
        case COND_NS: return COND_S;
        case COND_O:  return COND_NO;
        case COND_NO: return COND_O;
        case COND_P:  return COND_NP;
        case COND_NP: return COND_P;
        default: return COND_NONE;
    }
}