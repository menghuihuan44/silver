#include "silver/ir/ir_builder.h"
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// ============================================================
// 值映射管理 - 维护 指令索引 -> 值ID 的映射
// ============================================================

static bool extend_value_map(IRBuilder *builder, uint32_t new_size) {
    if (new_size <= builder->inst_to_value_size) return true;
    
    uint32_t *new_map = (uint32_t *)realloc(builder->inst_to_value,
        new_size * sizeof(uint32_t));
    if (!new_map) return false;
    
    for (uint32_t i = builder->inst_to_value_size; i < new_size; i++) {
        new_map[i] = IR_VALUE_ID_INVALID;
    }
    
    builder->inst_to_value = new_map;
    builder->inst_to_value_size = new_size;
    return true;
}

static IRValueId get_or_create_inst_value(IRBuilder *builder, uint32_t inst_idx, 
                                           IRTypeId type_id) {
    if (!builder) return IR_VALUE_ID_INVALID;
    
    if (inst_idx >= builder->inst_to_value_size) {
        if (!extend_value_map(builder, inst_idx + 4096)) {
            return IR_VALUE_ID_INVALID;
        }
    }
    
    if (builder->inst_to_value[inst_idx] != IR_VALUE_ID_INVALID) {
        return builder->inst_to_value[inst_idx];
    }
    
    IRValueId value_id = ir_value_create_instruction(
        &builder->module->value_pool, type_id, inst_idx);
    if (value_id == IR_VALUE_ID_INVALID) return IR_VALUE_ID_INVALID;
    
    builder->inst_to_value[inst_idx] = value_id;

    // 同步更新函数的映射表
    IRFunction *func = builder->current_func;
    if (func && func->inst_to_value) {
        if (inst_idx >= func->inst_to_value_capacity) {
            // 扩展函数映射表
            uint32_t new_cap = func->inst_to_value_capacity * 2;
            while (new_cap <= inst_idx) new_cap *= 2;
            IRValueId *new_map = (IRValueId *)arena_calloc(
                builder->module->arena, new_cap * sizeof(IRValueId));
            if (new_map) {
                memcpy(new_map, func->inst_to_value,
                       func->inst_to_value_capacity * sizeof(IRValueId));
                for (uint32_t i = func->inst_to_value_capacity; i < new_cap; i++) {
                    new_map[i] = IR_VALUE_ID_INVALID;
                }
                func->inst_to_value = new_map;
                func->inst_to_value_capacity = new_cap;
            }
        }
        if (inst_idx < func->inst_to_value_capacity) {
            func->inst_to_value[inst_idx] = value_id;
        }
    }

    return value_id;
}

// ============================================================
// 构建器生命周期
// ============================================================

IRBuilder *ir_builder_create(IRModule *module) {
    if (!module) return NULL;
    
    IRBuilder *builder = (IRBuilder *)calloc(1, sizeof(IRBuilder));
    if (!builder) return NULL;
    
    builder->module = module;
    
    // ✅ 直接使用预定义类型ID
    builder->void_type = module->type_table.void_type;
    builder->i1_type   = module->type_table.i1_type;
    builder->i8_type   = module->type_table.i8_type;
    builder->i16_type  = module->type_table.i16_type;
    builder->i32_type  = module->type_table.i32_type;
    builder->i64_type  = module->type_table.i64_type;
    builder->f32_type  = module->type_table.f32_type;
    builder->f64_type  = module->type_table.f64_type;
    builder->ptr_type  = module->type_table.ptr_type;
    
    // ✅ 如果任何基本类型无效，说明ir_type_table_init没正确初始化
    if (builder->i64_type == IR_TYPE_ID_INVALID ||
        builder->i32_type == IR_TYPE_ID_INVALID ||
        builder->void_type == IR_TYPE_ID_INVALID) {
        free(builder);
        return NULL;
    }
    
    builder->inst_to_value_size = 4096;
    builder->inst_to_value = (uint32_t *)malloc(
        builder->inst_to_value_size * sizeof(uint32_t));
    if (!builder->inst_to_value) {
        free(builder);
        return NULL;
    }
    for (uint32_t i = 0; i < builder->inst_to_value_size; i++) {
        builder->inst_to_value[i] = IR_VALUE_ID_INVALID;
    }
    
    return builder;
}

void ir_builder_destroy(IRBuilder *builder) {
    if (!builder) return;
    free(builder->inst_to_value);
    free(builder);
}

void ir_builder_set_position(IRBuilder *builder, IRFunction *func, IRBlock *block) {
    if (!builder) return;
    builder->current_func = func;
    builder->current_block = block;
}

void ir_builder_set_insert_point(IRBuilder *builder, IRBlock *block) {
    if (!builder) return;
    builder->current_block = block;
}

IRBlock *ir_builder_create_block(IRBuilder *builder, const char *name) {
    if (!builder || !builder->current_func) return NULL;
    return ir_function_add_block(builder->current_func, name);
}

void ir_builder_seal_block(IRBuilder *builder, IRBlock *block) {
    if (!builder || !block) return;
    block->is_sealed = true;
}

// ============================================================
// 核心指令添加函数 - 返回正确创建的值ID
// ============================================================

static IRValueId add_inst(IRBuilder *builder, IROpcode opcode, IRTypeId type_id,
                           uint16_t flags, IRValueId op0, IRValueId op1, 
                           IRValueId op2, uint16_t extra) {
    if (!builder || !builder->current_func) return IR_VALUE_ID_INVALID;
    
    IRFunction *func = builder->current_func;
    IRInst *inst = ir_function_add_inst(func, opcode, type_id, flags,
                                         op0, op1, op2, extra);
    if (!inst) return IR_VALUE_ID_INVALID;
    
    uint32_t inst_idx = func->num_insts - 1;
    
    if (builder->current_block) {
        printf("DEBUG add_inst: block %s, setting first/last to %u\n",
           builder->current_block->name ? builder->current_block->name : "?",
           inst_idx);
        if (builder->current_block->first_inst == IR_VALUE_ID_INVALID) {
            builder->current_block->first_inst = inst_idx;
        }
        builder->current_block->last_inst = inst_idx;
    } else {
    printf("DEBUG add_inst: current_block is NULL!\n");
}
    
    // 如果指令产生结果，通过映射表获取/创建正确的值ID
    if (type_id != IR_TYPE_ID_INVALID && type_id != 0) {
        return get_or_create_inst_value(builder, inst_idx, type_id);
    }
    
    return IR_VALUE_ID_INVALID;
}

// ============================================================
// 常量创建
// ============================================================

IRValueId ir_builder_int_const(IRBuilder *builder, IRTypeId type, int64_t value) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return ir_value_create_int_const(&builder->module->value_pool, type, value);
}

IRValueId ir_builder_float_const(IRBuilder *builder, IRTypeId type, double value) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return ir_value_create_float_const(&builder->module->value_pool, type, value);
}

IRValueId ir_builder_null(IRBuilder *builder, IRTypeId type) {
    return ir_builder_int_const(builder, type, 0);
}

IRValueId ir_builder_true(IRBuilder *builder) {
    return ir_builder_int_const(builder, builder->i1_type, 1);
}

IRValueId ir_builder_false(IRBuilder *builder) {
    return ir_builder_int_const(builder, builder->i1_type, 0);
}

// ============================================================
// 内存操作
// ============================================================

IRValueId ir_builder_alloca(IRBuilder *builder, IRTypeId type, uint32_t alignment) {
    if (!builder) return IR_VALUE_ID_INVALID;
    IRTypeId ptr_type = ir_type_create_ptr(&builder->module->type_table, type);
    return add_inst(builder, IR_OP_ALLOCA, ptr_type, 0,
                    IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID,
                    IR_VALUE_ID_INVALID, alignment);
}

IRValueId ir_builder_load(IRBuilder *builder, IRTypeId type, IRValueId ptr) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_LOAD, type, 0,
                    ptr, IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_store(IRBuilder *builder, IRValueId value, IRValueId ptr) {
    if (!builder) return IR_VALUE_ID_INVALID;
    add_inst(builder, IR_OP_STORE, IR_TYPE_ID_INVALID, 0,
             value, ptr, IR_VALUE_ID_INVALID, 0);
    return value;
}

IRValueId ir_builder_gep(IRBuilder *builder, IRTypeId result_type,
                          IRValueId base, IRValueId offset) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_GEP, result_type, 0,
                    base, offset, IR_VALUE_ID_INVALID, 0);
}

// ============================================================
// 算术操作
// ============================================================

static IRTypeId get_value_type(IRBuilder *builder, IRValueId val) {
    IRValue *v = ir_value_get(&builder->module->value_pool, val);
    return v ? v->type_id : builder->i64_type;
}

IRValueId ir_builder_add(IRBuilder *builder, IRValueId lhs, IRValueId rhs) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_ADD, get_value_type(builder, lhs), 0,
                    lhs, rhs, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_sub(IRBuilder *builder, IRValueId lhs, IRValueId rhs) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_SUB, get_value_type(builder, lhs), 0,
                    lhs, rhs, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_mul(IRBuilder *builder, IRValueId lhs, IRValueId rhs) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_MUL, get_value_type(builder, lhs), 0,
                    lhs, rhs, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_div(IRBuilder *builder, IRValueId lhs, IRValueId rhs, bool is_signed) {
    if (!builder) return IR_VALUE_ID_INVALID;
    IROpcode op = is_signed ? IR_OP_DIV : IR_OP_UDIV;
    return add_inst(builder, op, get_value_type(builder, lhs), 0,
                    lhs, rhs, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_rem(IRBuilder *builder, IRValueId lhs, IRValueId rhs, bool is_signed) {
    if (!builder) return IR_VALUE_ID_INVALID;
    IROpcode op = is_signed ? IR_OP_REM : IR_OP_UREM;
    return add_inst(builder, op, get_value_type(builder, lhs), 0,
                    lhs, rhs, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_neg(IRBuilder *builder, IRValueId val) {
    if (!builder) return IR_VALUE_ID_INVALID;
    IRValue *val_info = ir_value_get(&builder->module->value_pool, val);
    if (!val_info) return IR_VALUE_ID_INVALID;
    IRValueId zero = ir_builder_int_const(builder, val_info->type_id, 0);
    return ir_builder_sub(builder, zero, val);
}

// ============================================================
// 位操作
// ============================================================

IRValueId ir_builder_and(IRBuilder *builder, IRValueId lhs, IRValueId rhs) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_AND, get_value_type(builder, lhs), 0,
                    lhs, rhs, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_or(IRBuilder *builder, IRValueId lhs, IRValueId rhs) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_OR, get_value_type(builder, lhs), 0,
                    lhs, rhs, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_xor(IRBuilder *builder, IRValueId lhs, IRValueId rhs) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_XOR, get_value_type(builder, lhs), 0,
                    lhs, rhs, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_shl(IRBuilder *builder, IRValueId lhs, IRValueId rhs) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_SHL, get_value_type(builder, lhs), 0,
                    lhs, rhs, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_lshr(IRBuilder *builder, IRValueId lhs, IRValueId rhs) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_LSHR, get_value_type(builder, lhs), 0,
                    lhs, rhs, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_ashr(IRBuilder *builder, IRValueId lhs, IRValueId rhs) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_ASHR, get_value_type(builder, lhs), 0,
                    lhs, rhs, IR_VALUE_ID_INVALID, 0);
}

// ============================================================
// 比较操作
// ============================================================

IRValueId ir_builder_icmp(IRBuilder *builder, IROpcode cmp,
                           IRValueId lhs, IRValueId rhs) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, cmp, builder->i1_type, 0,
                    lhs, rhs, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_fcmp(IRBuilder *builder, IROpcode cmp,
                           IRValueId lhs, IRValueId rhs) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, cmp, builder->i1_type, 0,
                    lhs, rhs, IR_VALUE_ID_INVALID, 0);
}

// ============================================================
// 类型转换
// ============================================================

IRValueId ir_builder_trunc(IRBuilder *builder, IRValueId val, IRTypeId to_type) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_TRUNC, to_type, 0,
                    val, IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_zext(IRBuilder *builder, IRValueId val, IRTypeId to_type) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_ZEXT, to_type, 0,
                    val, IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_sext(IRBuilder *builder, IRValueId val, IRTypeId to_type) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_SEXT, to_type, 0,
                    val, IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_fptosi(IRBuilder *builder, IRValueId val, IRTypeId to_type) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_FPTOSI, to_type, 0,
                    val, IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_fptoui(IRBuilder *builder, IRValueId val, IRTypeId to_type) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_FPTOUI, to_type, 0,
                    val, IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_sitofp(IRBuilder *builder, IRValueId val, IRTypeId to_type) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_SITOFP, to_type, 0,
                    val, IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_uitofp(IRBuilder *builder, IRValueId val, IRTypeId to_type) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_UITOFP, to_type, 0,
                    val, IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
}

IRValueId ir_builder_bitcast(IRBuilder *builder, IRValueId val, IRTypeId to_type) {
    if (!builder) return IR_VALUE_ID_INVALID;
    return add_inst(builder, IR_OP_BITCAST, to_type, 0,
                    val, IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
}

// ============================================================
// 控制流
// ============================================================

IRValueId ir_builder_ret(IRBuilder *builder, IRValueId val) {
    if (!builder) return IR_VALUE_ID_INVALID;
    add_inst(builder, IR_OP_RET, IR_TYPE_ID_INVALID, 0,
             val, IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
    return IR_VALUE_ID_INVALID;
}

IRValueId ir_builder_ret_void(IRBuilder *builder) {
    if (!builder) return IR_VALUE_ID_INVALID;
    add_inst(builder, IR_OP_RET, IR_TYPE_ID_INVALID, 0,
             IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
    return IR_VALUE_ID_INVALID;
}

IRValueId ir_builder_br(IRBuilder *builder, IRBlock *target) {
    if (!builder || !target) return IR_VALUE_ID_INVALID;
    IRValueId target_val = ir_value_create_block(&builder->module->value_pool, target->id);
    add_inst(builder, IR_OP_BR, IR_TYPE_ID_INVALID, 0,
             target_val, IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
    return IR_VALUE_ID_INVALID;
}

IRValueId ir_builder_condbr(IRBuilder *builder, IRValueId cond,
                             IRBlock *true_block, IRBlock *false_block) {
    if (!builder || !true_block || !false_block) return IR_VALUE_ID_INVALID;
    IRValueId true_val  = ir_value_create_block(&builder->module->value_pool, true_block->id);
    IRValueId false_val = ir_value_create_block(&builder->module->value_pool, false_block->id);
    add_inst(builder, IR_OP_BRCOND, IR_TYPE_ID_INVALID, 0,
             cond, true_val, false_val, 0);
    return IR_VALUE_ID_INVALID;
}

IRValueId ir_builder_unreachable(IRBuilder *builder) {
    if (!builder) return IR_VALUE_ID_INVALID;
    add_inst(builder, IR_OP_UNREACHABLE, IR_TYPE_ID_INVALID, 0,
             IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
    return IR_VALUE_ID_INVALID;
}

// ============================================================
// PHI节点
// ============================================================

IRValueId ir_builder_phi(IRBuilder *builder, IRTypeId type,
                          uint32_t num_incoming,
                          IRValueId *incoming_values,
                          IRBlock **incoming_blocks) {
    if (!builder) return IR_VALUE_ID_INVALID;
    
    // ✅ 只支持两路分支 PHI（if-else）
    if (num_incoming > 2) {
        // 超过2个前驱的 PHI 暂不支持
        silver_error_add(builder->module->error_ctx, SILVER_ERROR_ERROR,
            SILVER_ERR_IR_INVALID_OPERAND, NULL,
            "PHI nodes with more than 2 incoming values are not yet supported (got %u)",
            num_incoming);
        return IR_VALUE_ID_INVALID;
    }
    
    if (num_incoming == 0 || !incoming_values || !incoming_blocks) {
        return IR_VALUE_ID_INVALID;
    }
    
    // ✅ 获取传入块的值ID
    IRValueId block0_val = ir_value_create_block(&builder->module->value_pool,
                                                   incoming_blocks[0]->id);
    IRValueId block1_val = IR_VALUE_ID_INVALID;
    if (num_incoming == 2 && incoming_blocks[1]) {
        block1_val = ir_value_create_block(&builder->module->value_pool,
                                            incoming_blocks[1]->id);
    }
    
    // extra字段存储传入数量
    // operand0 = 第一个值, operand1 = 第一个块, operand2 = 第二个值
    return add_inst(builder, IR_OP_PHI, type, 0,
                    incoming_values[0],                    // 值1
                    block0_val,                            // 块1
                    (num_incoming == 2) ? incoming_values[1] : IR_VALUE_ID_INVALID,  // 值2
                    num_incoming);
}

// ============================================================
// 函数调用
// ============================================================

IRValueId ir_builder_call(IRBuilder *builder, IRTypeId return_type,
                           IRValueId callee, uint32_t num_args, ...) {
    if (!builder) return IR_VALUE_ID_INVALID;
    
    va_list args;
    va_start(args, num_args);
    
    // extra字段存储参数数量
    IRValueId result = add_inst(builder, IR_OP_CALL, return_type, 0,
                                 callee, IR_VALUE_ID_INVALID,
                                 IR_VALUE_ID_INVALID, num_args);
    
    va_end(args);
    return result;
}

// ============================================================
// 选择
// ============================================================

IRValueId ir_builder_select(IRBuilder *builder, IRValueId cond,
                             IRValueId true_val, IRValueId false_val) {
    if (!builder) return IR_VALUE_ID_INVALID;
    IRTypeId type = get_value_type(builder, true_val);
    return add_inst(builder, IR_OP_SELECT, type, 0,
                    cond, true_val, false_val, 0);
}

// ============================================================
// 复制
// ============================================================

IRValueId ir_builder_copy(IRBuilder *builder, IRValueId val) {
    if (!builder) return IR_VALUE_ID_INVALID;
    IRTypeId type = get_value_type(builder, val);
    return add_inst(builder, IR_OP_COPY, type, 0,
                    val, IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
}

// ============================================================
// 查询
// ============================================================

IRFunction *ir_builder_get_function(const IRBuilder *builder) {
    return builder ? builder->current_func : NULL;
}

IRBlock *ir_builder_get_block(const IRBuilder *builder) {
    return builder ? builder->current_block : NULL;
}

IRValueId ir_builder_get_inst_value(IRBuilder *builder, uint32_t inst_idx) {
    if (!builder || inst_idx >= builder->inst_to_value_size) 
        return IR_VALUE_ID_INVALID;
    return builder->inst_to_value[inst_idx];
}

bool ir_builder_is_valid(const IRBuilder *builder) {
    return builder && builder->module && builder->current_func;
}