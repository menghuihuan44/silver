#include "silver/opt/copy_prop.h"
#include "silver/ir/ir_builder.h"
#include <string.h>
#include <stdlib.h>

// 从值池中查找指令的结果值ID
static IRValueId find_inst_result_value(IRValuePool *pool, uint32_t inst_idx, 
                                         IRTypeId type_id) {
    if (!pool) return IR_VALUE_ID_INVALID;
    
    for (uint32_t v = 0; v < pool->num_values; v++) {
        IRValue *val = &pool->values[v];
        if (val->kind == IR_VALUE_INSTRUCTION &&
            val->inst_id == inst_idx &&
            val->type_id == type_id) {
            return v;
        }
    }
    return IR_VALUE_ID_INVALID;
}

// 复写传播：将copy指令的用户替换为源操作数
bool opt_copy_propagate(OptContext *ctx) {
    if (!ctx || !ctx->module) return false;
    
    bool changed = false;
    IRModule *module = ctx->module;
    IRValuePool *pool = &module->value_pool;
    
    for (uint32_t f = 0; f < module->num_functions; f++) {
        IRFunction *func = &module->functions[f];
        ctx->current_func = func;
        
        // ✅ 构建值到其定义指令的映射
        uint32_t *value_to_inst = (uint32_t *)malloc(
            pool->num_values * sizeof(uint32_t));
        if (!value_to_inst) continue;
        for (uint32_t i = 0; i < pool->num_values; i++) {
            value_to_inst[i] = UINT32_MAX;
        }
        
        // 填充映射
        for (uint32_t v = 0; v < pool->num_values; v++) {
            if (pool->values[v].kind == IR_VALUE_INSTRUCTION) {
                uint32_t inst_idx = pool->values[v].inst_id;
                if (inst_idx < func->num_insts) {
                    value_to_inst[v] = inst_idx;
                }
            }
        }
        
        // ✅ 多遍扫描，展开多层copy链
        bool local_changed = true;
        int max_passes = 10;
        
        while (local_changed && max_passes > 0) {
            local_changed = false;
            max_passes--;
            
            for (uint32_t i = 0; i < func->num_insts; i++) {
                IRInst *inst = &func->instructions[i];
                
                // 跳过copy指令本身
                if (inst->opcode == IR_OP_COPY) continue;
                
                // 检查所有操作数，替换引用copy指令结果的值
                for (int o = 0; o < 3; o++) {
                    uint16_t *op_ptr = (o == 0) ? &inst->operand0_id :
                                       (o == 1) ? &inst->operand1_id :
                                       &inst->operand2_id;
                    
                    IRValueId op_id = *op_ptr;
                    if (op_id == IR_VALUE_ID_INVALID) continue;
                    
                    IRValue *op_val = ir_value_get(pool, op_id);
                    if (!op_val || op_val->kind != IR_VALUE_INSTRUCTION) continue;
                    
                    // 检查定义这个值的指令是否是COPY
                    uint32_t def_inst = op_val->inst_id;
                    if (def_inst >= func->num_insts) continue;
                    
                    IRInst *def = &func->instructions[def_inst];
                    if (def->opcode == IR_OP_COPY) {
                        // ✅ 替换为copy的源操作数
                        *op_ptr = def->operand0_id;
                        ctx->stats.copies_propagated++;
                        local_changed = true;
                        changed = true;
                    }
                }
            }
        }
        
        free(value_to_inst);
    }
    
    return changed;
}

// 窥孔优化
bool opt_peephole(OptContext *ctx) {
    if (!ctx || !ctx->module) return false;
    
    bool changed = false;
    IRModule *module = ctx->module;
    IRValuePool *pool = &module->value_pool;
    
    for (uint32_t f = 0; f < module->num_functions; f++) {
        IRFunction *func = &module->functions[f];
        ctx->current_func = func;
        
        for (uint32_t i = 0; i + 1 < func->num_insts; i++) {
            IRInst *inst1 = &func->instructions[i];
            IRInst *inst2 = &func->instructions[i + 1];
            
            // ✅ 模式1: 连续的 copy 指令
            if (inst1->opcode == IR_OP_COPY && inst2->opcode == IR_OP_COPY) {
                // 如果 inst1 的结果被 inst2 引用
                for (uint32_t v = 0; v < pool->num_values; v++) {
                    if (pool->values[v].kind == IR_VALUE_INSTRUCTION &&
                        pool->values[v].inst_id == i &&
                        pool->values[v].type_id == inst1->type_id) {
                        if (inst2->operand0_id == v) {
                            inst1->opcode = IR_OP_NOP;
                            changed = true;
                            break;
                        }
                    }
                }
            }
            
            // ✅ 模式2: load 后紧跟 load 同一地址 → 复用第一个结果
            if (inst1->opcode == IR_OP_LOAD && inst2->opcode == IR_OP_LOAD) {
                if (inst1->operand0_id == inst2->operand0_id) {
                    // 相同地址，inst2 可以替换为 copy inst1 的结果
                    IRValueId inst1_result = IR_VALUE_ID_INVALID;
                    for (uint32_t v = 0; v < pool->num_values; v++) {
                        if (pool->values[v].kind == IR_VALUE_INSTRUCTION &&
                            pool->values[v].inst_id == i &&
                            pool->values[v].type_id == inst1->type_id) {
                            inst1_result = v;
                            break;
                        }
                    }
                    if (inst1_result != IR_VALUE_ID_INVALID) {
                        inst2->opcode = IR_OP_COPY;
                        inst2->operand0_id = inst1_result;
                        inst2->operand1_id = IR_VALUE_ID_INVALID;
                        changed = true;
                    }
                }
            }
            
            // ✅ 模式3: add r, 0 后跟 cmp → cmp 可以直接用 r
            if ((inst1->opcode == IR_OP_ADD || inst1->opcode == IR_OP_SUB) &&
                ir_opcode_is_comparison(inst2->opcode)) {
                // x86 上 ADD 已经设置标志位，但 IR 层面不做修改
                // 这个模式留给代码生成阶段处理
            }
            
            // ✅ 模式4: 无用的 copy（copy %x 后如果没有被使用 → 消除）
            if (inst1->opcode == IR_OP_COPY) {
                bool used = false;
                IRValueId copy_result = IR_VALUE_ID_INVALID;
                for (uint32_t v = 0; v < pool->num_values; v++) {
                    if (pool->values[v].kind == IR_VALUE_INSTRUCTION &&
                        pool->values[v].inst_id == i) {
                        copy_result = v;
                        break;
                    }
                }
                if (copy_result != IR_VALUE_ID_INVALID) {
                    for (uint32_t j = 0; j < func->num_insts; j++) {
                        if (j == i) continue;
                        IRInst *user = &func->instructions[j];
                        if (user->operand0_id == copy_result ||
                            user->operand1_id == copy_result ||
                            user->operand2_id == copy_result) {
                            used = true;
                            break;
                        }
                    }
                    if (!used) {
                        inst1->opcode = IR_OP_NOP;
                        changed = true;
                    }
                }
            }
        }
    }
    
    return changed;
}