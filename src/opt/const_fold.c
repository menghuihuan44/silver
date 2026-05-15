#include "silver/opt/const_fold.h"
#include <math.h>

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

// 安全的有符号乘法（检测溢出）
static bool safe_mul_i64(int64_t a, int64_t b, int64_t *result) {
    if (a == 0 || b == 0) {
        *result = 0;
        return true;
    }
    if (a > 0) {
        if (b > 0) {
            if (a > INT64_MAX / b) return false;
        } else {
            if (b < INT64_MIN / a) return false;
        }
    } else {
        if (b > 0) {
            if (a < INT64_MIN / b) return false;
        } else {
            if (a < INT64_MAX / b) return false;
        }
    }
    *result = a * b;
    return true;
}

// 安全的有符号除法（检测除零和溢出）
static bool safe_div_i64(int64_t a, int64_t b, int64_t *result) {
    if (b == 0) return false;
    if (a == INT64_MIN && b == -1) return false;  // 溢出
    *result = a / b;
    return true;
}

// 安全的无符号除法
static bool safe_udiv_u64(uint64_t a, uint64_t b, uint64_t *result) {
    if (b == 0) return false;
    *result = a / b;
    return true;
}

// 安全的有符号取余
static bool safe_rem_i64(int64_t a, int64_t b, int64_t *result) {
    if (b == 0) return false;
    if (a == INT64_MIN && b == -1) return false;
    *result = a % b;
    return true;
}

// 安全的无符号取余
static bool safe_urem_u64(uint64_t a, uint64_t b, uint64_t *result) {
    if (b == 0) return false;
    *result = a % b;
    return true;
}

// 安全的左移（检测溢出和无定义行为）
static bool safe_shl_i64(int64_t a, int64_t b, int64_t *result) {
    if (b < 0 || b >= 64) return false;
    
    // 检查有符号溢出
    uint64_t ua = (uint64_t)a;
    uint64_t shifted = ua << b;
    
    // 检查符号位是否改变（对于有符号数）
    if (a >= 0 && shifted > (uint64_t)INT64_MAX) return false;
    if (a < 0) {
        // 负数左移后必须仍然是负数
        if ((int64_t)shifted >= 0) return false;
    }
    
    *result = (int64_t)shifted;
    return true;
}

// 安全的逻辑右移
static bool safe_lshr_i64(int64_t a, int64_t b, int64_t *result) {
    if (b < 0 || b >= 64) return false;
    *result = (int64_t)((uint64_t)a >> b);
    return true;
}

// 安全的算术右移
static bool safe_ashr_i64(int64_t a, int64_t b, int64_t *result) {
    if (b < 0 || b >= 64) return false;
    *result = a >> b;
    return true;
}

// 为不同类型宽度执行常量折叠
static bool fold_int_operation(IROpcode opcode,
                                int64_t lhs, int64_t rhs,
                                int64_t *result) {
    switch (opcode) {
        case IR_OP_ADD:
            return safe_add_i64(lhs, rhs, result);
        
        case IR_OP_SUB:
            return safe_sub_i64(lhs, rhs, result);
        
        case IR_OP_MUL:
            return safe_mul_i64(lhs, rhs, result);
        
        case IR_OP_DIV:
            return safe_div_i64(lhs, rhs, result);
        
        case IR_OP_UDIV:
            return safe_udiv_u64((uint64_t)lhs, (uint64_t)rhs, 
                                 (uint64_t *)result);
        
        case IR_OP_REM:
            return safe_rem_i64(lhs, rhs, result);
        
        case IR_OP_UREM:
            return safe_urem_u64((uint64_t)lhs, (uint64_t)rhs, 
                                 (uint64_t *)result);
        
        case IR_OP_AND:
            *result = lhs & rhs;
            return true;
        
        case IR_OP_OR:
            *result = lhs | rhs;
            return true;
        
        case IR_OP_XOR:
            *result = lhs ^ rhs;
            return true;
        
        case IR_OP_SHL:
            return safe_shl_i64(lhs, rhs, result);
        
        case IR_OP_LSHR:
            return safe_lshr_i64(lhs, rhs, result);
        
        case IR_OP_ASHR:
            return safe_ashr_i64(lhs, rhs, result);
        
        default:
            return false;
    }
}

// 比较操作的常量折叠
static bool fold_int_comparison(IROpcode opcode, int64_t lhs, int64_t rhs,
                                 bool *result) {
    switch (opcode) {
        case IR_OP_CMPEQ:
            *result = (lhs == rhs);
            return true;
        case IR_OP_CMPNE:
            *result = (lhs != rhs);
            return true;
        case IR_OP_CMPLT:
            *result = (lhs < rhs);
            return true;
        case IR_OP_CMPLE:
            *result = (lhs <= rhs);
            return true;
        case IR_OP_CMPGT:
            *result = (lhs > rhs);
            return true;
        case IR_OP_CMPGE:
            *result = (lhs >= rhs);
            return true;
        case IR_OP_CMPULT:
            *result = ((uint64_t)lhs < (uint64_t)rhs);
            return true;
        case IR_OP_CMPULE:
            *result = ((uint64_t)lhs <= (uint64_t)rhs);
            return true;
        case IR_OP_CMPUGT:
            *result = ((uint64_t)lhs > (uint64_t)rhs);
            return true;
        case IR_OP_CMPUGE:
            *result = ((uint64_t)lhs >= (uint64_t)rhs);
            return true;
        default:
            return false;
    }
}

// 浮点操作的常量折叠
static bool fold_float_operation(IROpcode opcode, double lhs, double rhs,
                                  double *result) {
    switch (opcode) {
        case IR_OP_FADD:
            *result = lhs + rhs;
            return true;
        case IR_OP_FSUB:
            *result = lhs - rhs;
            return true;
        case IR_OP_FMUL:
            *result = lhs * rhs;
            return true;
        case IR_OP_FDIV:
            if (rhs == 0.0) return false;
            *result = lhs / rhs;
            return true;
        case IR_OP_FREM:
            *result = fmod(lhs, rhs);
            return true;
        default:
            return false;
    }
}

// 浮点比较的常量折叠
static bool fold_float_comparison(IROpcode opcode, double lhs, double rhs,
                                   bool *result) {
    switch (opcode) {
        case IR_OP_FCMPOEQ:
            *result = !isnan(lhs) && !isnan(rhs) && lhs == rhs;
            return true;
        case IR_OP_FCMPONE:
            *result = !isnan(lhs) && !isnan(rhs) && lhs != rhs;
            return true;
        case IR_OP_FCMPOLT:
            *result = !isnan(lhs) && !isnan(rhs) && lhs < rhs;
            return true;
        case IR_OP_FCMPOLE:
            *result = !isnan(lhs) && !isnan(rhs) && lhs <= rhs;
            return true;
        case IR_OP_FCMPOGT:
            *result = !isnan(lhs) && !isnan(rhs) && lhs > rhs;
            return true;
        case IR_OP_FCMPOGE:
            *result = !isnan(lhs) && !isnan(rhs) && lhs >= rhs;
            return true;
        case IR_OP_FCMPUEQ:
            *result = isnan(lhs) || isnan(rhs) || lhs == rhs;
            return true;
        case IR_OP_FCMPUNE:
            *result = isnan(lhs) || isnan(rhs) || lhs != rhs;
            return true;
        case IR_OP_FCMPULT:
            *result = isnan(lhs) || isnan(rhs) || lhs < rhs;
            return true;
        case IR_OP_FCMPULE:
            *result = isnan(lhs) || isnan(rhs) || lhs <= rhs;
            return true;
        case IR_OP_FCMPUGT:
            *result = isnan(lhs) || isnan(rhs) || lhs > rhs;
            return true;
        case IR_OP_FCMPUGE:
            *result = isnan(lhs) || isnan(rhs) || lhs >= rhs;
            return true;
        default:
            return false;
    }
}

bool const_fold_instruction(OptContext *ctx, IRFunction *func,
                             uint32_t inst_idx, IRInst *inst) {
    if (!ctx || !func || !inst) return false;
    
    IRModule *module = ctx->module;
    IRValuePool *pool = &module->value_pool;
    
    // 检查操作数是否都是常量
    int num_operands = ir_opcode_num_operands(inst->opcode);
    
    if (num_operands == 0) return false;
    
    IRValueId op0 = inst->operand0_id;
    IRValueId op1 = inst->operand1_id;
    
    IRValue *op0_val = ir_value_get(pool, op0);
    if (!op0_val || op0_val->kind != IR_VALUE_CONSTANT) return false;
    
    // 对于二元操作，检查第二个操作数
    if (num_operands >= 2) {
        IRValue *op1_val = ir_value_get(pool, op1);
        if (!op1_val || op1_val->kind != IR_VALUE_CONSTANT) return false;
    }
    
    // 一元操作
    if (num_operands == 1) {
        IRValue *val = op0_val;
        
        switch (inst->opcode) {
            case IR_OP_NEG: {
                IRValue *val = op0_val;
                if (val->type_id < module->type_table.num_types) {
                    IRType *type = &module->type_table.types[val->type_id];
                    if (type->kind == IR_TYPE_INT) {
                        int64_t result;
                        if (safe_sub_i64(0, val->int_val, &result)) {
                            inst->opcode = IR_OP_COPY;
                            inst->operand0_id = ir_value_create_int_const(
                                pool, val->type_id, result);
                            ctx->stats.constants_folded++;
                            return true;
                        }
                    }
                }
                break;
            }

            case IR_OP_FNEG: {
                IRValue *val = op0_val;
                double result = -val->float_val;
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = ir_value_create_float_const(
                    pool, val->type_id, result);
                ctx->stats.constants_folded++;
                return true; 
            }
            
        case IR_OP_TRUNC: {
            // 截断：如 i64 → i32，取低32位
            IRType *src_type = ir_type_get(&module->type_table, val->type_id);
            IRType *dst_type = ir_type_get(&module->type_table, inst->type_id);
            if (src_type && dst_type && src_type->kind == IR_TYPE_INT && dst_type->kind == IR_TYPE_INT) {
                uint64_t mask = (1ULL << dst_type->int_type.width) - 1;
                int64_t truncated = val->int_val & (int64_t)mask;
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = ir_value_create_int_const(pool, inst->type_id, truncated);
                ctx->stats.constants_folded++;
                return true;
            }
            break;
        }
        case IR_OP_ZEXT: {
            // 零扩展：如 i32 → i64，高位填0
            IRType *src_type = ir_type_get(&module->type_table, val->type_id);
            IRType *dst_type = ir_type_get(&module->type_table, inst->type_id);
            if (src_type && dst_type && src_type->kind == IR_TYPE_INT && dst_type->kind == IR_TYPE_INT) {
                uint64_t mask = (1ULL << src_type->int_type.width) - 1;
                uint64_t extended = (uint64_t)val->int_val & mask;
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = ir_value_create_int_const(pool, inst->type_id, (int64_t)extended);
                ctx->stats.constants_folded++;
                return true;
            }
            break;
        }
        case IR_OP_SEXT: {
            // 符号扩展
            IRType *src_type = ir_type_get(&module->type_table, val->type_id);
            IRType *dst_type = ir_type_get(&module->type_table, inst->type_id);
            if (src_type && dst_type && src_type->kind == IR_TYPE_INT && dst_type->kind == IR_TYPE_INT) {
                uint32_t src_bits = src_type->int_type.width;
                int64_t value = val->int_val;
                // 符号扩展：将src_bits位的符号位扩展到64位
                int64_t sign_bit = 1LL << (src_bits - 1);
                if (value & sign_bit) {
                    value |= ~((1LL << src_bits) - 1);
                }
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = ir_value_create_int_const(pool, inst->type_id, value);
                ctx->stats.constants_folded++;
                return true;
            }
            break;
        }
        case IR_OP_FPTOSI: {
            // 浮点 → 有符号整数
            if (ir_type_get_kind(&module->type_table, val->type_id) == IR_TYPE_FLOAT) {
                int64_t result = (int64_t)val->float_val;
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = ir_value_create_int_const(pool, inst->type_id, result);
                ctx->stats.constants_folded++;
                return true;
            }
            break;
        }
        case IR_OP_FPTOUI: {
            // 浮点 → 无符号整数
            if (ir_type_get_kind(&module->type_table, val->type_id) == IR_TYPE_FLOAT) {
                uint64_t result = (uint64_t)val->float_val;
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = ir_value_create_int_const(pool, inst->type_id, (int64_t)result);
                ctx->stats.constants_folded++;
                return true;
            }
            break;
        }
        case IR_OP_SITOFP: {
            // 有符号整数 → 浮点
            if (ir_type_get_kind(&module->type_table, val->type_id) == IR_TYPE_INT) {
                double result = (double)val->int_val;
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = ir_value_create_float_const(pool, inst->type_id, result);
                ctx->stats.constants_folded++;
                return true;
            }
            break;
        }
        case IR_OP_UITOFP: {
            // 无符号整数 → 浮点
            if (ir_type_get_kind(&module->type_table, val->type_id) == IR_TYPE_INT) {
                double result = (double)((uint64_t)val->int_val);
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = ir_value_create_float_const(pool, inst->type_id, result);
                ctx->stats.constants_folded++;
                return true;
            }
            break;
        }
        case IR_OP_FPTRUNC: {
            // 浮点截断：f64 → f32
            if (ir_type_get_kind(&module->type_table, val->type_id) == IR_TYPE_FLOAT) {
                float result = (float)val->float_val;
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = ir_value_create_float_const(pool, inst->type_id, (double)result);
                ctx->stats.constants_folded++;
                return true;
            }
            break;
        }
        case IR_OP_FPEXT: {
            // 浮点扩展：f32 → f64
            if (ir_type_get_kind(&module->type_table, val->type_id) == IR_TYPE_FLOAT) {
                double result = (double)((float)val->float_val);
                inst->opcode = IR_OP_COPY;
                inst->operand0_id = ir_value_create_float_const(pool, inst->type_id, result);
                ctx->stats.constants_folded++;
                return true;
            }
            break;
        }
        case IR_OP_BITCAST: {
            // 位模式重解释：保持原始位模式
            // int ↔ float 或相同宽度的类型
            IRType *src_type = ir_type_get(&module->type_table, val->type_id);
            IRType *dst_type = ir_type_get(&module->type_table, inst->type_id);
            if (src_type && dst_type) {
                if (src_type->kind == IR_TYPE_INT && dst_type->kind == IR_TYPE_FLOAT) {
                    // int → float: 重新解释位模式
                    union { int64_t i; double d; } u = { .i = val->int_val };
                    inst->opcode = IR_OP_COPY;
                    inst->operand0_id = ir_value_create_float_const(pool, inst->type_id, u.d);
                    ctx->stats.constants_folded++;
                    return true;
                } else if (src_type->kind == IR_TYPE_FLOAT && dst_type->kind == IR_TYPE_INT) {
                    // float → int: 重新解释位模式
                    union { double d; int64_t i; } u = { .d = val->float_val };
                    inst->opcode = IR_OP_COPY;
                    inst->operand0_id = ir_value_create_int_const(pool, inst->type_id, u.i);
                    ctx->stats.constants_folded++;
                    return true;
                }
            }
            break;
        } 
            
            default:
                break;
        }
        
        return false;
    }
    
    // 二元操作
    if (num_operands >= 2) {
        IRValue *lhs = op0_val;
        IRValue *rhs = ir_value_get(pool, op1);
        
        // 检查类型是否为整数或浮点
        IRTypeKind kind = ir_type_get_kind(&module->type_table, lhs->type_id);
        
        if (kind == IR_TYPE_INT) {
            if (ir_opcode_is_comparison(inst->opcode)) {
                bool cmp_result;
                if (fold_int_comparison(inst->opcode, lhs->int_val, 
                                        rhs->int_val, &cmp_result)) {
                    int64_t result = cmp_result ? 1 : 0;
                    inst->opcode = IR_OP_COPY;
                    inst->operand0_id = ir_value_create_int_const(
                        pool, inst->type_id, result);
                    ctx->stats.constants_folded++;
                    return true;
                }
            } else {
                int64_t result;
                if (fold_int_operation(inst->opcode, lhs->int_val,
                                       rhs->int_val, &result)) {
                    inst->opcode = IR_OP_COPY;
                    inst->operand0_id = ir_value_create_int_const(
                        pool, lhs->type_id, result);
                    ctx->stats.constants_folded++;
                    return true;
                }
            }
        } else if (kind == IR_TYPE_FLOAT) {
            if (op0_val->kind == IR_VALUE_CONSTANT && 
                ir_value_get(pool, op1)->kind == IR_VALUE_CONSTANT) {
                
                if (ir_opcode_is_comparison(inst->opcode)) {
                    bool cmp_result;
                    if (fold_float_comparison(inst->opcode, lhs->float_val,
                                              rhs->float_val, &cmp_result)) {
                        int64_t result = cmp_result ? 1 : 0;
                        inst->opcode = IR_OP_COPY;
                        inst->operand0_id = ir_value_create_int_const(
                            pool, inst->type_id, result);
                        ctx->stats.constants_folded++;
                        return true;
                    }
                } else {
                    double result;
                    if (fold_float_operation(inst->opcode, lhs->float_val,
                                             rhs->float_val, &result)) {
                        inst->opcode = IR_OP_COPY;
                        inst->operand0_id = ir_value_create_float_const(
                            pool, lhs->type_id, result);
                        ctx->stats.constants_folded++;
                        return true;
                    }
                }
            }
        }
    }
    
    return false;
}

bool opt_const_fold(OptContext *ctx) {
    if (!ctx || !ctx->module) return false;
    
    bool changed = false;
    IRModule *module = ctx->module;
    
    // 遍历所有函数的所有指令
    for (uint32_t f = 0; f < module->num_functions; f++) {
        IRFunction *func = &module->functions[f];
        ctx->current_func = func;
        
        for (uint32_t i = 0; i < func->num_insts; i++) {
            IRInst *inst = &func->instructions[i];
            
            if (const_fold_instruction(ctx, func, i, inst)) {
                changed = true;
            }
        }
    }
    
    return changed;
}