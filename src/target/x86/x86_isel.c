#include "silver/target/x86/x86_isel.h"

// x86-64完整的指令选择匹配表
const MatchEntry x86_match_table[] = {
    // 算术操作
    {
        .ir_opcode = IR_OP_ADD,
        .operand_constraints = {OPERAND_REG, OPERAND_REG, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_GPR, REG_CLASS_NONE},
        .mach_opcode = MACH_ADD,
        .operand_mapping = {0, 1, -1},  // rn=dst, rm=src
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_COMMUTATIVE,
        .cost = 1
    },
    {
        .ir_opcode = IR_OP_ADD,
        .operand_constraints = {OPERAND_REG, OPERAND_IMM, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_ADD_IMM,
        .operand_mapping = {0, -1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_HAS_IMM_VARIANT,
        .cost = 1
    },
    
    {
        .ir_opcode = IR_OP_SUB,
        .operand_constraints = {OPERAND_REG, OPERAND_REG, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_GPR, REG_CLASS_NONE},
        .mach_opcode = MACH_SUB,
        .operand_mapping = {0, 1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = 0,
        .cost = 1
    },
    {
        .ir_opcode = IR_OP_SUB,
        .operand_constraints = {OPERAND_REG, OPERAND_IMM, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_SUB_IMM,
        .operand_mapping = {0, -1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_HAS_IMM_VARIANT,
        .cost = 1
    },
    
    {
        .ir_opcode = IR_OP_MUL,
        .operand_constraints = {OPERAND_REG, OPERAND_REG, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_GPR, REG_CLASS_NONE},
        .mach_opcode = MACH_MUL,
        .operand_mapping = {0, 1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_COMMUTATIVE | MATCH_DESTROYS_OPERAND,
        .cost = 2
    },
    {
        .ir_opcode = IR_OP_MUL,
        .operand_constraints = {OPERAND_REG, OPERAND_IMM, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_MUL_IMM,
        .operand_mapping = {0, -1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_HAS_IMM_VARIANT,
        .cost = 2
    },
    
    {
        .ir_opcode = IR_OP_DIV,
        .operand_constraints = {OPERAND_REG, OPERAND_REG, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_GPR, REG_CLASS_NONE},
        .mach_opcode = MACH_DIV,
        .operand_mapping = {0, 1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_NEEDS_SPECIAL_REG | MATCH_DESTROYS_OPERAND,
        .cost = 5
    },
    {
        .ir_opcode = IR_OP_UDIV,
        .operand_constraints = {OPERAND_REG, OPERAND_REG, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_GPR, REG_CLASS_NONE},
        .mach_opcode = MACH_UDIV,
        .operand_mapping = {0, 1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_NEEDS_SPECIAL_REG | MATCH_DESTROYS_OPERAND,
        .cost = 5
    },
    
    // 逻辑操作
    {
        .ir_opcode = IR_OP_AND,
        .operand_constraints = {OPERAND_REG, OPERAND_REG, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_GPR, REG_CLASS_NONE},
        .mach_opcode = MACH_AND,
        .operand_mapping = {0, 1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_COMMUTATIVE,
        .cost = 1
    },
    {
        .ir_opcode = IR_OP_OR,
        .operand_constraints = {OPERAND_REG, OPERAND_REG, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_GPR, REG_CLASS_NONE},
        .mach_opcode = MACH_OR,
        .operand_mapping = {0, 1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_COMMUTATIVE,
        .cost = 1
    },
    {
        .ir_opcode = IR_OP_XOR,
        .operand_constraints = {OPERAND_REG, OPERAND_REG, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_GPR, REG_CLASS_NONE},
        .mach_opcode = MACH_XOR,
        .operand_mapping = {0, 1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_COMMUTATIVE,
        .cost = 1
    },
    
    // 移位操作
    {
        .ir_opcode = IR_OP_SHL,
        .operand_constraints = {OPERAND_REG, OPERAND_REG, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_GPR, REG_CLASS_NONE},
        .mach_opcode = MACH_SHL,
        .operand_mapping = {0, 1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_NEEDS_SPECIAL_REG,  // 移位量必须在CL
        .cost = 2
    },
    {
        .ir_opcode = IR_OP_SHL,
        .operand_constraints = {OPERAND_REG, OPERAND_IMM, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_SHL_IMM,
        .operand_mapping = {0, -1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_HAS_IMM_VARIANT,
        .cost = 1
    },
    {
        .ir_opcode = IR_OP_LSHR,
        .operand_constraints = {OPERAND_REG, OPERAND_REG, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_GPR, REG_CLASS_NONE},
        .mach_opcode = MACH_LSHR,
        .operand_mapping = {0, 1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_NEEDS_SPECIAL_REG,
        .cost = 2
    },
    {
        .ir_opcode = IR_OP_LSHR,
        .operand_constraints = {OPERAND_REG, OPERAND_IMM, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_LSHR_IMM,
        .operand_mapping = {0, -1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_HAS_IMM_VARIANT,
        .cost = 1
    },
    {
        .ir_opcode = IR_OP_ASHR,
        .operand_constraints = {OPERAND_REG, OPERAND_REG, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_GPR, REG_CLASS_NONE},
        .mach_opcode = MACH_ASHR,
        .operand_mapping = {0, 1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_NEEDS_SPECIAL_REG,
        .cost = 2
    },
    {
        .ir_opcode = IR_OP_ASHR,
        .operand_constraints = {OPERAND_REG, OPERAND_IMM, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_ASHR_IMM,
        .operand_mapping = {0, -1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_HAS_IMM_VARIANT,
        .cost = 1
    },
    
    // 比较操作
    {
        .ir_opcode = IR_OP_CMPEQ,  // 这个会匹配所有整数比较
        .operand_constraints = {OPERAND_REG, OPERAND_REG, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_GPR, REG_CLASS_NONE},
        .mach_opcode = MACH_CMP,
        .operand_mapping = {0, 1, -1},
        .result_reg_class = REG_CLASS_NONE,  // 比较不产生值，设置标志
        .flags = 0,
        .cost = 1
    },
    {
        .ir_opcode = IR_OP_CMPNE,
        .operand_constraints = {OPERAND_REG, OPERAND_REG, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_GPR, REG_CLASS_NONE},
        .mach_opcode = MACH_CMP,
        .operand_mapping = {0, 1, -1},
        .result_reg_class = REG_CLASS_NONE,
        .flags = 0,
        .cost = 1
    },
    // 注意：CMPLT, CMPLE, CMPGT, CMPGE, CMPULT, CMPULE, CMPUGT, CMPUGE
    // 都会映射到MACH_CMP，条件码的转换在ISel中处理
    
    // 加载/存储
    {
        .ir_opcode = IR_OP_LOAD,
        .operand_constraints = {OPERAND_REG, OPERAND_ANY, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_LOAD,
        .operand_mapping = {0, -1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = 0,
        .cost = 2
    },
    {
        .ir_opcode = IR_OP_STORE,
        .operand_constraints = {OPERAND_REG, OPERAND_REG, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_GPR, REG_CLASS_NONE},
        .mach_opcode = MACH_STORE,
        .operand_mapping = {0, 1, -1},  // rn=value, rm=address
        .result_reg_class = REG_CLASS_NONE,
        .flags = 0,
        .cost = 2
    },
    
    // GEP（地址计算）
    {
        .ir_opcode = IR_OP_GEP,
        .operand_constraints = {OPERAND_REG, OPERAND_REG, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_GPR, REG_CLASS_NONE},
        .mach_opcode = MACH_LEA,
        .operand_mapping = {0, 1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = 0,
        .cost = 1
    },
    
    // 函数调用
    {
        .ir_opcode = IR_OP_CALL,
        .operand_constraints = {OPERAND_REG, OPERAND_ANY, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_INDIRECT_CALL,
        .operand_mapping = {0, -1, -1},
        .result_reg_class = REG_CLASS_GPR,  // 返回值在RAX
        .flags = MATCH_DESTROYS_OPERAND,
        .cost = 3
    },
    {
        .ir_opcode = IR_OP_CALL,
        .operand_constraints = {OPERAND_LABEL, OPERAND_ANY, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_NONE, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_CALL,
        .operand_mapping = {-1, -1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = 0,
        .cost = 3
    },
    
    // 返回
    {
        .ir_opcode = IR_OP_RET,
        .operand_constraints = {OPERAND_ANY, OPERAND_ANY, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_NONE, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_RET,
        .operand_mapping = {-1, -1, -1},
        .result_reg_class = REG_CLASS_NONE,
        .flags = 0,
        .cost = 1
    },
    
    // 复制/MOV
    {
        .ir_opcode = IR_OP_COPY,
        .operand_constraints = {OPERAND_REG, OPERAND_ANY, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_MOV,
        .operand_mapping = {0, -1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = 0,
        .cost = 1
    },
    {
        .ir_opcode = IR_OP_COPY,
        .operand_constraints = {OPERAND_IMM, OPERAND_ANY, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_NONE, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_MOV_IMM,
        .operand_mapping = {-1, -1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_HAS_IMM_VARIANT,
        .cost = 2
    },
    
    // 类型转换
    {
        .ir_opcode = IR_OP_SEXT,
        .operand_constraints = {OPERAND_REG, OPERAND_ANY, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_MOVSX,
        .operand_mapping = {0, -1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = 0,
        .cost = 1
    },
    {
        .ir_opcode = IR_OP_ZEXT,
        .operand_constraints = {OPERAND_REG, OPERAND_ANY, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_MOVZX,
        .operand_mapping = {0, -1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = 0,
        .cost = 1
    },
    
    // 分支
    {
        .ir_opcode = IR_OP_BR,
        .operand_constraints = {OPERAND_LABEL, OPERAND_ANY, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_NONE, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_JMP,
        .operand_mapping = {-1, -1, -1},
        .result_reg_class = REG_CLASS_NONE,
        .flags = 0,
        .cost = 1
    },
    {
        .ir_opcode = IR_OP_BRCOND,
        .operand_constraints = {OPERAND_REG, OPERAND_LABEL, OPERAND_LABEL},
        .operand_reg_class = {REG_CLASS_GPR, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_JCC,
        .operand_mapping = {0, 1, 2},
        .result_reg_class = REG_CLASS_NONE,
        .flags = 0,
        .cost = 1
    },
    
    // ALLOCA
    {
        .ir_opcode = IR_OP_ALLOCA,
        .operand_constraints = {OPERAND_IMM, OPERAND_ANY, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_NONE, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_SUB_IMM,  // sub rsp, size
        .operand_mapping = {-1, -1, -1},
        .result_reg_class = REG_CLASS_GPR,
        .flags = MATCH_NEEDS_SPECIAL_REG,  // 需要RSP
        .cost = 1
    },
    
    // 空操作（用于消除死代码后的占位）
    {
        .ir_opcode = IR_OP_UNREACHABLE,
        .operand_constraints = {OPERAND_ANY, OPERAND_ANY, OPERAND_ANY},
        .operand_reg_class = {REG_CLASS_NONE, REG_CLASS_NONE, REG_CLASS_NONE},
        .mach_opcode = MACH_INT3,
        .operand_mapping = {-1, -1, -1},
        .result_reg_class = REG_CLASS_NONE,
        .flags = 0,
        .cost = 1
    },
};

const uint32_t x86_match_table_size = sizeof(x86_match_table) / sizeof(x86_match_table[0]);