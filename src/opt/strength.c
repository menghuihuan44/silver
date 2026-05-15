#include "silver/opt/strength.h"

// 检查一个数是否为2的幂
static bool is_power_of_two(int64_t value) {
    return value > 0 && (value & (value - 1)) == 0;
}

// 计算log2（对于2的幂）
static int ilog2(int64_t value) {
    int result = 0;
    while (value > 1) {
        value >>= 1;
        result++;
    }
    return result;
}

bool strength_reduce_instruction(OptContext *ctx, IRFunction *func,
                                  uint32_t inst_idx, IRInst *inst) {
    if (!ctx || !inst) return false;
    
    IRValuePool *pool = &ctx->module->value_pool;
    IRValueId op0 = inst->operand0_id;
    IRValueId op1 = inst->operand1_id;
    
    IRValue *op1_val = ir_value_get(pool, op1);
    if (!op1_val || op1_val->kind != IR_VALUE_CONSTANT) return false;
    
    switch (inst->opcode) {
        // x * 2^n => x << n
        case IR_OP_MUL: {
            int64_t multiplier = op1_val->int_val;
            if (is_power_of_two(multiplier)) {
                int shift = ilog2(multiplier);
                IRValueId shift_val = ir_value_create_int_const(
                    pool, op1_val->type_id, shift);
                
                inst->opcode = IR_OP_SHL;
                inst->operand0_id = op0;
                inst->operand1_id = shift_val;
                ctx->stats.strength_reductions++;
                return true;
            }
            break;
        }
        
        // x / 2^n (有符号) => x >> n (算术右移)
        case IR_OP_DIV: {
            int64_t divisor = op1_val->int_val;
            if (is_power_of_two(divisor)) {
                int shift = ilog2(divisor);
                IRValueId shift_val = ir_value_create_int_const(
                    pool, op1_val->type_id, shift);
                
                inst->opcode = IR_OP_ASHR;
                inst->operand0_id = op0;
                inst->operand1_id = shift_val;
                ctx->stats.strength_reductions++;
                return true;
            }
            break;
        }
        
        // x / 2^n (无符号) => x >> n (逻辑右移)
        case IR_OP_UDIV: {
            int64_t divisor = op1_val->int_val;
            if (is_power_of_two(divisor)) {
                int shift = ilog2(divisor);
                IRValueId shift_val = ir_value_create_int_const(
                    pool, op1_val->type_id, shift);
                
                inst->opcode = IR_OP_LSHR;
                inst->operand0_id = op0;
                inst->operand1_id = shift_val;
                ctx->stats.strength_reductions++;
                return true;
            }
            break;
        }
        
        // x % 2^n (无符号) => x & (2^n - 1)
        case IR_OP_UREM: {
            int64_t divisor = op1_val->int_val;
            if (is_power_of_two(divisor)) {
                IRValueId mask_val = ir_value_create_int_const(
                    pool, op1_val->type_id, divisor - 1);
                
                inst->opcode = IR_OP_AND;
                inst->operand0_id = op0;
                inst->operand1_id = mask_val;
                ctx->stats.strength_reductions++;
                return true;
            }
            break;
        }
        
        default:
            break;
    }
    
    return false;
}

bool opt_strength_reduce(OptContext *ctx) {
    if (!ctx || !ctx->module) return false;
    
    bool changed = false;
    IRModule *module = ctx->module;
    
    for (uint32_t f = 0; f < module->num_functions; f++) {
        IRFunction *func = &module->functions[f];
        ctx->current_func = func;
        
        for (uint32_t i = 0; i < func->num_insts; i++) {
            IRInst *inst = &func->instructions[i];
            
            if (strength_reduce_instruction(ctx, func, i, inst)) {
                changed = true;
            }
        }
    }
    
    return changed;
}