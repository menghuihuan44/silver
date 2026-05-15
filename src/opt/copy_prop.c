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
        
        // 扫描所有指令
        for (uint32_t i = 0; i < func->num_insts; i++) {
            IRInst *inst = &func->instructions[i];
            
            if (inst->opcode != IR_OP_COPY) continue;
            
            // 找到copy指令的源和目标
            IRValueId src = inst->operand0_id;
            
            // 在值池中查找该copy指令产生的结果值ID
            IRValueId dst = find_inst_result_value(pool, i, inst->type_id);
            if (dst == IR_VALUE_ID_INVALID) continue;
            
            // 在所有后续指令中查找对dst的使用并替换为src
            for (uint32_t j = 0; j < func->num_insts; j++) {
                if (j == i) continue;  // 跳过copy指令自身
                
                IRInst *user = &func->instructions[j];
                bool used = false;
                
                if (user->operand0_id == dst) {
                    user->operand0_id = (uint16_t)src;
                    used = true;
                }
                if (user->operand1_id == dst) {
                    user->operand1_id = (uint16_t)src;
                    used = true;
                }
                if (user->operand2_id == dst) {
                    user->operand2_id = (uint16_t)src;
                    used = true;
                }
                
                if (used) {
                    ctx->stats.copies_propagated++;
                    changed = true;
                }
            }
        }
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
            
            // 模式：连续的copy指令
            if (inst1->opcode == IR_OP_COPY && inst2->opcode == IR_OP_COPY) {
                // 查找inst1的结果值ID
                IRValueId inst1_result = find_inst_result_value(pool, i, inst1->type_id);
                if (inst1_result != IR_VALUE_ID_INVALID &&
                    inst2->operand0_id == inst1_result) {
                    // inst2使用了inst1的结果，直接让inst1指向inst2的源
                    inst1->opcode = IR_OP_NOP;
                    changed = true;
                }
            }
        }
    }
    
    return changed;
}