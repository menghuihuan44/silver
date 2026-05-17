#include "silver/opt/algebraic.h"

// 安全的有符号加法（检测溢出）
static bool safe_add_i64(int64_t a, int64_t b, int64_t *result) {
    if ((b > 0 && a > INT64_MAX - b) || 
        (b < 0 && a < INT64_MIN - b)) {
        return false;
    }
    *result = a + b;
    return true;
}

// 安全的有符号减法（检测溢出）
static bool safe_sub_i64(int64_t a, int64_t b, int64_t *result) {
    if ((b < 0 && a > INT64_MAX + b) || 
        (b > 0 && a < INT64_MIN + b)) {
        return false;
    }
    *result = a - b;
    return true;
}

bool algebraic_simplify_instruction(OptContext *ctx, IRFunction *func,
                                     uint32_t inst_idx, IRInst *inst) {
    if (!ctx || !inst) return false;
    
    IRModule *module = ctx->module;
    IRValuePool *pool = &module->value_pool;
    
    IRValueId op0 = inst->operand0_id;
    IRValueId op1 = inst->operand1_id;
    
    // 获取操作数值（如果它们是常量）
    IRValue *op0_val = ir_value_get(pool, op0);
    IRValue *op1_val = ir_value_get(pool, op1);
    
    if (!op0_val) return false;
    
    switch (inst->opcode) {
        // x + 0 = x
        case IR_OP_ADD:
        case IR_OP_FADD:
            if (op1_val && ir_value_is_zero(pool, op1)) {
                // 替换为 copy x
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = op0;
                inst->operand1_id = IR_VALUE_ID_INVALID;
                ctx->stats.algebraic_simplifications++;
                return true;
            }
            // 0 + x = x
            if (ir_value_is_zero(pool, op0)) {
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = op1;
                inst->operand1_id = IR_VALUE_ID_INVALID;
                ctx->stats.algebraic_simplifications++;
                return true;
            }
            // ============================================================
            // ✅ 结合律：(x + c1) + c2  →  x + (c1 + c2)
            // 当第一个操作数本身是 add 指令的结果，且其第二个操作数是常量时
            // ============================================================
            if (op1_val) {
                // 不是两个常量，尝试结合律
                IRValue *op0_info = ir_value_get(pool, op0);
                if (op0_info && op0_info->kind == IR_VALUE_INSTRUCTION) {
                    // op0 是一条指令的结果，检查它是否是 add 且第二个操作数是常量
                    IRInst *def_inst = &func->instructions[op0_info->inst_id];
                    if (def_inst->opcode == IR_OP_ADD || def_inst->opcode == IR_OP_SUB) {
                        IRValueId inner_const = def_inst->operand1_id;
                        IRValue *inner_cv = ir_value_get(pool, inner_const);
                        if (inner_cv && inner_cv->kind == IR_VALUE_CONSTANT) {
                            // 检查 op1 是否也是常量
                            IRValue *outer_cv = ir_value_get(pool, op1);
                            if (outer_cv && outer_cv->kind == IR_VALUE_CONSTANT) {
                                int64_t c1 = inner_cv->int_val;
                                int64_t c2 = outer_cv->int_val;
                                int64_t combined;
                                bool overflow = false;
                                
                                if (def_inst->opcode == IR_OP_ADD) {
                                    overflow = !safe_add_i64(c1, c2, &combined);
                                } else {
                                    // (x - c1) + c2 = x + (c2 - c1)
                                    overflow = !safe_sub_i64(c2, c1, &combined);
                                }
                                
                                if (!overflow) {
                                    // 创建新的常量 combined
                                    IRValueId new_const = ir_value_create_int_const(
                                        pool, outer_cv->type_id, combined);
                                    // 修改指令：op0 变为内层指令的第一个操作数，op1 变为新常量
                                    inst->operand0_id = def_inst->operand0_id;
                                    inst->operand1_id = new_const;
                                    ctx->stats.algebraic_simplifications++;
                                    return true;
                                }
                            }
                        }
                    }
                }
            }
            break;
        
        // x - 0 = x
        case IR_OP_SUB:
        case IR_OP_FSUB:
            if (op1_val && ir_value_is_zero(pool, op1)) {
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = op0;
                inst->operand1_id = IR_VALUE_ID_INVALID;
                ctx->stats.algebraic_simplifications++;
                return true;
            }
            // x - x = 0
            if (op0 == op1) {
                IRValueId zero = ir_value_create_int_const(pool, inst->type_id, 0);
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = zero;
                inst->operand1_id = IR_VALUE_ID_INVALID;
                ctx->stats.algebraic_simplifications++;
                return true;
            }
            // (x + c1) - c2  →  x + (c1 - c2)  如果 c1 > c2
            // (x - c1) - c2  →  x - (c1 + c2)
            if (op0_val->kind == IR_VALUE_INSTRUCTION && op1_val && op1_val->kind == IR_VALUE_CONSTANT) {
                IRInst *def_inst = &func->instructions[op0_val->inst_id];
                if (def_inst->opcode == IR_OP_ADD || def_inst->opcode == IR_OP_SUB) {
                    IRValueId inner_const = def_inst->operand1_id;
                    IRValue *inner_cv = ir_value_get(pool, inner_const);
                    if (inner_cv && inner_cv->kind == IR_VALUE_CONSTANT) {
                        int64_t c1 = inner_cv->int_val;
                        int64_t c2 = op1_val->int_val;
                        int64_t combined;
                        bool overflow = false;
                        
                        if (def_inst->opcode == IR_OP_ADD) {
                            // (x + c1) - c2 = x + (c1 - c2)
                            overflow = !safe_sub_i64(c1, c2, &combined);
                        } else {
                            // (x - c1) - c2 = x - (c1 + c2)
                            overflow = !safe_add_i64(c1, c2, &combined);
                        }
                        
                        if (!overflow) {
                            IRValueId new_const = ir_value_create_int_const(
                                pool, inner_cv->type_id, combined);
                            inst->operand0_id = def_inst->operand0_id;
                            inst->operand1_id = new_const;
                            // 如果结果符号变了，改操作码
                            if (def_inst->opcode == IR_OP_ADD && c1 < c2) {
                                inst->opcode = IR_OP_ADD;  // x + (c1-c2) 中 c1-c2 可能是负数
                            }
                            ctx->stats.algebraic_simplifications++;
                            return true;
                        }
                    }
                }
            }
            break;
        
        // x * 1 = x
        case IR_OP_MUL:
        case IR_OP_FMUL:
            if (op1_val && op1_val->kind == IR_VALUE_CONSTANT) {
                if (op1_val->int_val == 1 || op1_val->float_val == 1.0) {
                    inst->opcode = IR_OP_COPY;
                    inst->operand0_id = op0;
                    inst->operand1_id = IR_VALUE_ID_INVALID;
                    ctx->stats.algebraic_simplifications++;
                    return true;
                }
                // x * 0 = 0
                if (op1_val->int_val == 0 || op1_val->float_val == 0.0) {
                    inst->opcode = IR_OP_COPY;
                    inst->operand0_id = op1;
                    inst->operand1_id = IR_VALUE_ID_INVALID;
                    ctx->stats.algebraic_simplifications++;
                    return true;
                }
                // x * -1 = -x
                if (op1_val->int_val == -1) {
                    inst->opcode = IR_OP_SUB;
                    inst->operand0_id = ir_value_create_int_const(
                        pool, inst->type_id, 0);
                    inst->operand1_id = op0;
                    ctx->stats.algebraic_simplifications++;
                    return true;
                }
            }
            // 1 * x = x
            if (op0_val && op0_val->kind == IR_VALUE_CONSTANT) {
                if (op0_val->int_val == 1 || op0_val->float_val == 1.0) {
                    inst->opcode = IR_OP_COPY;
                    inst->operand0_id = op1;
                    inst->operand1_id = IR_VALUE_ID_INVALID;
                    ctx->stats.algebraic_simplifications++;
                    return true;
                }
            }
            break;
        
        // x / 1 = x
        case IR_OP_DIV:
        case IR_OP_UDIV:
        case IR_OP_FDIV:
            if (op1_val && op1_val->kind == IR_VALUE_CONSTANT) {
                if (op1_val->int_val == 1 || op1_val->float_val == 1.0) {
                    inst->opcode = IR_OP_COPY;
                    inst->operand0_id = op0;
                    inst->operand1_id = IR_VALUE_ID_INVALID;
                    ctx->stats.algebraic_simplifications++;
                    return true;
                }
            }
            break;
        
        // x & 0 = 0
        case IR_OP_AND:
            if (op1_val && ir_value_is_zero(pool, op1)) {
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = op1;
                inst->operand1_id = IR_VALUE_ID_INVALID;
                ctx->stats.algebraic_simplifications++;
                return true;
            }
            // x & x = x
            if (op0 == op1) {
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = op0;
                inst->operand1_id = IR_VALUE_ID_INVALID;
                ctx->stats.algebraic_simplifications++;
                return true;
            }
            // x & -1 = x
            if (op1_val && ir_value_is_all_ones(pool, op1)) {
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = op0;
                inst->operand1_id = IR_VALUE_ID_INVALID;
                ctx->stats.algebraic_simplifications++;
                return true;
            }
            break;
        
        // x | 0 = x
        case IR_OP_OR:
            if (op1_val && ir_value_is_zero(pool, op1)) {
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = op0;
                inst->operand1_id = IR_VALUE_ID_INVALID;
                ctx->stats.algebraic_simplifications++;
                return true;
            }
            // x | x = x
            if (op0 == op1) {
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = op0;
                inst->operand1_id = IR_VALUE_ID_INVALID;
                ctx->stats.algebraic_simplifications++;
                return true;
            }
            // x | -1 = -1
            if (op1_val && ir_value_is_all_ones(pool, op1)) {
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = op1;
                inst->operand1_id = IR_VALUE_ID_INVALID;
                ctx->stats.algebraic_simplifications++;
                return true;
            }
            break;
        
        // x ^ 0 = x
        case IR_OP_XOR:
            if (op1_val && ir_value_is_zero(pool, op1)) {
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = op0;
                inst->operand1_id = IR_VALUE_ID_INVALID;
                ctx->stats.algebraic_simplifications++;
                return true;
            }
            // x ^ x = 0
            if (op0 == op1) {
                IRValueId zero = ir_value_create_int_const(pool, inst->type_id, 0);
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = zero;
                inst->operand1_id = IR_VALUE_ID_INVALID;
                ctx->stats.algebraic_simplifications++;
                return true;
            }
            break;
        
        // x << 0 = x
        // x >> 0 = x
        case IR_OP_SHL:
        case IR_OP_LSHR:
        case IR_OP_ASHR:
            if (op1_val && ir_value_is_zero(pool, op1)) {
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = op0;
                inst->operand1_id = IR_VALUE_ID_INVALID;
                ctx->stats.algebraic_simplifications++;
                return true;
            }
            break;
        
        default:
            break;
    }
    
    return false;
}

bool opt_algebraic_simplify(OptContext *ctx) {
    if (!ctx || !ctx->module) return false;
    
    bool changed = false;
    IRModule *module = ctx->module;
    
    for (uint32_t f = 0; f < module->num_functions; f++) {
        IRFunction *func = &module->functions[f];
        ctx->current_func = func;
        
        for (uint32_t i = 0; i < func->num_insts; i++) {
            IRInst *inst = &func->instructions[i];
            
            if (algebraic_simplify_instruction(ctx, func, i, inst)) {
                changed = true;
            }
        }
    }
    
    return changed;
}