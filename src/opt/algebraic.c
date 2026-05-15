#include "silver/opt/algebraic.h"

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
            if (op0_val && ir_value_is_zero(pool, op0)) {
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = op1;
                inst->operand1_id = IR_VALUE_ID_INVALID;
                ctx->stats.algebraic_simplifications++;
                return true;
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