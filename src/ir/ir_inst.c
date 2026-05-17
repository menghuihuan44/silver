#include "silver/ir/ir_inst.h"
#include <string.h>
#include <stdio.h>

const char *ir_opcode_name(IROpcode opcode) {
    static const char *names[] = {
        [IR_OP_RET] = "ret",
        [IR_OP_BR] = "br",
        [IR_OP_BRCOND] = "brcond",
        [IR_OP_SWITCH] = "switch",
        [IR_OP_UNREACHABLE] = "unreachable",
        
        [IR_OP_LOAD] = "load",
        [IR_OP_STORE] = "store",
        [IR_OP_ALLOCA] = "alloca",
        [IR_OP_GEP] = "gep",
        
        [IR_OP_ADD] = "add",
        [IR_OP_SUB] = "sub",
        [IR_OP_MUL] = "mul",
        [IR_OP_DIV] = "div",
        [IR_OP_UDIV] = "udiv",
        [IR_OP_REM] = "rem",
        [IR_OP_UREM] = "urem",
        [IR_OP_NEG] = "neg",
        
        [IR_OP_AND] = "and",
        [IR_OP_OR] = "or",
        [IR_OP_XOR] = "xor",
        [IR_OP_SHL] = "shl",
        [IR_OP_LSHR] = "lshr",
        [IR_OP_ASHR] = "ashr",
        
        [IR_OP_CMPEQ] = "cmpeq",
        [IR_OP_CMPNE] = "cmpne",
        [IR_OP_CMPLT] = "cmplt",
        [IR_OP_CMPLE] = "cmple",
        [IR_OP_CMPGT] = "cmpgt",
        [IR_OP_CMPGE] = "cmpge",
        [IR_OP_CMPULT] = "cmpult",
        [IR_OP_CMPULE] = "cmpule",
        [IR_OP_CMPUGT] = "cmpugt",
        [IR_OP_CMPUGE] = "cmpuge",
        
        [IR_OP_FADD] = "fadd",
        [IR_OP_FSUB] = "fsub",
        [IR_OP_FMUL] = "fmul",
        [IR_OP_FDIV] = "fdiv",
        [IR_OP_FREM] = "frem",
        [IR_OP_FNEG] = "fneg",
        
        [IR_OP_FCMPOEQ] = "fcmpoeq",
        [IR_OP_FCMPONE] = "fcmpone",
        [IR_OP_FCMPOLT] = "fcmplt",
        [IR_OP_FCMPOLE] = "fcmple",
        [IR_OP_FCMPOGT] = "fcmpgt",
        [IR_OP_FCMPOGE] = "fcmpge",
        [IR_OP_FCMPUEQ] = "fcmpueq",
        [IR_OP_FCMPUNE] = "fcmpune",
        [IR_OP_FCMPULT] = "fcmpult",
        [IR_OP_FCMPULE] = "fcmpule",
        [IR_OP_FCMPUGT] = "fcmpugt",
        [IR_OP_FCMPUGE] = "fcmpuge",
        
        [IR_OP_TRUNC] = "trunc",
        [IR_OP_ZEXT] = "zext",
        [IR_OP_SEXT] = "sext",
        [IR_OP_FPTOUI] = "fptoui",
        [IR_OP_FPTOSI] = "fptosi",
        [IR_OP_UITOFP] = "uitofp",
        [IR_OP_SITOFP] = "sitofp",
        [IR_OP_FPTRUNC] = "fptrunc",
        [IR_OP_FPEXT] = "fpext",
        [IR_OP_PTRTOI] = "ptrtoi",
        [IR_OP_ITOPTR] = "itoptr",
        [IR_OP_BITCAST] = "bitcast",
        
        [IR_OP_PHI] = "phi",
        [IR_OP_CALL] = "call",
        [IR_OP_SELECT] = "select",
        [IR_OP_COPY] = "copy",
        
        [IR_OP_ATOMIC_LOAD] = "atomic_load",
        [IR_OP_ATOMIC_STORE] = "atomic_store",
        [IR_OP_ATOMIC_RMW] = "atomic_rmw",
        [IR_OP_ATOMIC_CAS] = "atomic_cas",
        [IR_OP_FENCE] = "fence",

        [IR_OP_NOP] = "nop",
    };
    
    if (opcode >= IR_OP_COUNT) return "unknown";
    return names[opcode];
}

int ir_opcode_num_operands(IROpcode opcode) {
    switch (opcode) {
        case IR_OP_RET:
            return 1;  // 返回值（可选）
        
        case IR_OP_BR:
            return 1;  // 目标块
        
        case IR_OP_BRCOND:
            return 3;  // 条件, 真块, 假块
        
        case IR_OP_SWITCH:
            return 2;  // 值, 默认块（其他case在extra数据中）
        
        case IR_OP_UNREACHABLE:
            return 0;
        
        case IR_OP_LOAD:
            return 1;  // 地址
        
        case IR_OP_STORE:
            return 2;  // 值, 地址
        
        case IR_OP_ALLOCA:
            return 0;  // 大小由类型决定
        
        case IR_OP_GEP:
            return 2;  // 基址, 偏移
        
        case IR_OP_ADD:
        case IR_OP_SUB:
        case IR_OP_MUL:
        case IR_OP_DIV:
        case IR_OP_UDIV:
        case IR_OP_REM:
        case IR_OP_UREM:
        case IR_OP_AND:
        case IR_OP_OR:
        case IR_OP_XOR:
        case IR_OP_SHL:
        case IR_OP_LSHR:
        case IR_OP_ASHR:
            return 2;
        
        case IR_OP_CMPEQ:
        case IR_OP_CMPNE:
        case IR_OP_CMPLT:
        case IR_OP_CMPLE:
        case IR_OP_CMPGT:
        case IR_OP_CMPGE:
        case IR_OP_CMPULT:
        case IR_OP_CMPULE:
        case IR_OP_CMPUGT:
        case IR_OP_CMPUGE:
            return 2;
        
        case IR_OP_FADD:
        case IR_OP_FSUB:
        case IR_OP_FMUL:
        case IR_OP_FDIV:
        case IR_OP_FREM:
            return 2;
        
        case IR_OP_FNEG:
            return 1;
        
        case IR_OP_FCMPOEQ:
        case IR_OP_FCMPONE:
        case IR_OP_FCMPOLT:
        case IR_OP_FCMPOLE:
        case IR_OP_FCMPOGT:
        case IR_OP_FCMPOGE:
        case IR_OP_FCMPUEQ:
        case IR_OP_FCMPUNE:
        case IR_OP_FCMPULT:
        case IR_OP_FCMPULE:
        case IR_OP_FCMPUGT:
        case IR_OP_FCMPUGE:
            return 2;
        
        case IR_OP_TRUNC:
        case IR_OP_ZEXT:
        case IR_OP_SEXT:
        case IR_OP_FPTOUI:
        case IR_OP_FPTOSI:
        case IR_OP_UITOFP:
        case IR_OP_SITOFP:
        case IR_OP_FPTRUNC:
        case IR_OP_FPEXT:
        case IR_OP_PTRTOI:
        case IR_OP_ITOPTR:
        case IR_OP_BITCAST:
            return 1;
        
        case IR_OP_PHI:
            return 2;  // PHI的操作数在extra部分
        
        case IR_OP_CALL:
            return 1;  // 被调用函数（参数在extra部分）
        
        case IR_OP_SELECT:
            return 3;  // 条件, 真值, 假值
        
        case IR_OP_COPY:
            return 1;
        
        case IR_OP_ATOMIC_LOAD:
            return 1;  // 地址
        
        case IR_OP_ATOMIC_STORE:
            return 2;  // 值, 地址
        
        case IR_OP_ATOMIC_RMW:
            return 2;  // 地址, 值
        
        case IR_OP_ATOMIC_CAS:
            return 3;  // 地址, 期望值, 新值
        
        case IR_OP_FENCE:
            return 0;
        
        default:
            return 0;
    }
}

bool ir_opcode_has_side_effects(IROpcode opcode) {
    switch (opcode) {
        case IR_OP_STORE:
        case IR_OP_CALL:
        case IR_OP_RET:
        case IR_OP_BR:
        case IR_OP_BRCOND:
        case IR_OP_SWITCH:
        case IR_OP_UNREACHABLE:
        case IR_OP_ATOMIC_STORE:
        case IR_OP_ATOMIC_RMW:
        case IR_OP_ATOMIC_CAS:
        case IR_OP_FENCE:
            return true;
        
        case IR_OP_DIV:
        case IR_OP_UDIV:
        case IR_OP_REM:
        case IR_OP_UREM:
            // 除零可能引发异常
            return true;
        
        default:
            return false;
    }
}

bool ir_opcode_is_terminator(IROpcode opcode) {
    switch (opcode) {
        case IR_OP_RET:
        case IR_OP_BR:
        case IR_OP_BRCOND:
        case IR_OP_SWITCH:
        case IR_OP_UNREACHABLE:
            return true;
        default:
            return false;
    }
}

bool ir_opcode_is_commutative(IROpcode opcode) {
    switch (opcode) {
        case IR_OP_ADD:
        case IR_OP_MUL:
        case IR_OP_AND:
        case IR_OP_OR:
        case IR_OP_XOR:
        case IR_OP_CMPEQ:
        case IR_OP_CMPNE:
        case IR_OP_FADD:
        case IR_OP_FMUL:
            return true;
        default:
            return false;
    }
}

bool ir_opcode_is_comparison(IROpcode opcode) {
    switch (opcode) {
        case IR_OP_CMPEQ:
        case IR_OP_CMPNE:
        case IR_OP_CMPLT:
        case IR_OP_CMPLE:
        case IR_OP_CMPGT:
        case IR_OP_CMPGE:
        case IR_OP_CMPULT:
        case IR_OP_CMPULE:
        case IR_OP_CMPUGT:
        case IR_OP_CMPUGE:
        case IR_OP_FCMPOEQ:
        case IR_OP_FCMPONE:
        case IR_OP_FCMPOLT:
        case IR_OP_FCMPOLE:
        case IR_OP_FCMPOGT:
        case IR_OP_FCMPOGE:
        case IR_OP_FCMPUEQ:
        case IR_OP_FCMPUNE:
        case IR_OP_FCMPULT:
        case IR_OP_FCMPULE:
        case IR_OP_FCMPUGT:
        case IR_OP_FCMPUGE:
            return true;
        default:
            return false;
    }
}

IROpcode ir_opcode_reverse_comparison(IROpcode opcode) {
    switch (opcode) {
        case IR_OP_CMPEQ: return IR_OP_CMPNE;
        case IR_OP_CMPNE: return IR_OP_CMPEQ;
        case IR_OP_CMPLT: return IR_OP_CMPGE;
        case IR_OP_CMPLE: return IR_OP_CMPGT;
        case IR_OP_CMPGT: return IR_OP_CMPLE;
        case IR_OP_CMPGE: return IR_OP_CMPLT;
        case IR_OP_CMPULT: return IR_OP_CMPUGE;
        case IR_OP_CMPULE: return IR_OP_CMPUGT;
        case IR_OP_CMPUGT: return IR_OP_CMPULE;
        case IR_OP_CMPUGE: return IR_OP_CMPULT;
        
        case IR_OP_FCMPOEQ: return IR_OP_FCMPUNE;
        case IR_OP_FCMPONE: return IR_OP_FCMPUEQ;
        case IR_OP_FCMPOLT: return IR_OP_FCMPUGE;
        case IR_OP_FCMPOLE: return IR_OP_FCMPUGT;
        case IR_OP_FCMPOGT: return IR_OP_FCMPULE;
        case IR_OP_FCMPOGE: return IR_OP_FCMPULT;
        case IR_OP_FCMPUEQ: return IR_OP_FCMPONE;
        case IR_OP_FCMPUNE: return IR_OP_FCMPOEQ;
        case IR_OP_FCMPULT: return IR_OP_FCMPOGE;
        case IR_OP_FCMPULE: return IR_OP_FCMPOGT;
        case IR_OP_FCMPUGT: return IR_OP_FCMPOLE;
        case IR_OP_FCMPUGE: return IR_OP_FCMPOLT;
        
        default:
            return opcode;
    }
}

const char *ir_inst_to_string(const IRInst *inst, const IRTypeTable *types,
                               const IRValuePool *values,
                               char *buffer, size_t buffer_size) {
    if (!inst || !buffer || buffer_size == 0) return "";
    
    char operand0_str[64], operand1_str[64], operand2_str[64];
    
    const char *op0_str = ir_value_to_string(values, inst->operand0_id, 
                                              types, operand0_str, sizeof(operand0_str));
    const char *op1_str = ir_value_to_string(values, inst->operand1_id, 
                                              types, operand1_str, sizeof(operand1_str));
    const char *op2_str = ir_value_to_string(values, inst->operand2_id, 
                                              types, operand2_str, sizeof(operand2_str));
    
    switch (inst->opcode) {
        case IR_OP_RET:
            if (inst->operand0_id != IR_VALUE_ID_INVALID) {
                snprintf(buffer, buffer_size, "ret %s", op0_str);
            } else {
                snprintf(buffer, buffer_size, "ret void");
            }
            break;
        
        case IR_OP_BR:
            snprintf(buffer, buffer_size, "br %s", op0_str);
            break;
        
        case IR_OP_BRCOND:
            snprintf(buffer, buffer_size, "brcond %s, %s, %s", 
                     op0_str, op1_str, op2_str);
            break;
        
        default:
            snprintf(buffer, buffer_size, "%%r = %s %s, %s", 
                     ir_opcode_name(inst->opcode), op0_str, op1_str);
            break;
    }
    
    return buffer;
}