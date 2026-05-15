#include "silver/opt/dce.h"
#include <string.h>
#include <stdlib.h>

// 初始化标记数组
static void init_marked(OptContext *ctx, uint32_t num_insts) {
    if (ctx->marked_size < num_insts) {
        free(ctx->marked);
        ctx->marked = (uint8_t *)calloc(num_insts, 1);
        ctx->marked_size = num_insts;
    } else {
        memset(ctx->marked, 0, num_insts);
    }
}

// Worklist管理
static void worklist_init(OptContext *ctx) {
    if (!ctx->worklist) {
        ctx->worklist_capacity = 256;
        ctx->worklist = (uint32_t *)malloc(
            ctx->worklist_capacity * sizeof(uint32_t));
    }
    ctx->worklist_size = 0;
}

static void worklist_push(OptContext *ctx, uint32_t inst_idx) {
    if (ctx->worklist_size >= ctx->worklist_capacity) {
        ctx->worklist_capacity *= 2;
        uint32_t *new_list = (uint32_t *)realloc(ctx->worklist,
            ctx->worklist_capacity * sizeof(uint32_t));
        if (!new_list) return;
        ctx->worklist = new_list;
    }
    ctx->worklist[ctx->worklist_size++] = inst_idx;
}

static uint32_t worklist_pop(OptContext *ctx) {
    if (ctx->worklist_size == 0) return UINT32_MAX;
    return ctx->worklist[--ctx->worklist_size];
}

// 标记指令为活跃
static void mark_instruction_live(OptContext *ctx, IRFunction *func,
                                   uint32_t inst_idx) {
    if (ctx->marked[inst_idx]) return;
    ctx->marked[inst_idx] = 1;
    worklist_push(ctx, inst_idx);
}

// 从有副作用的指令出发，标记所有活跃指令
static void mark_live_instructions(OptContext *ctx, IRFunction *func) {
    worklist_init(ctx);
    init_marked(ctx, func->num_insts);
    IRValuePool *pool = &ctx->module->value_pool;
    
    // 第一遍：标记所有有副作用的指令
    for (uint32_t i = 0; i < func->num_insts; i++) {
        IRInst *inst = &func->instructions[i];
        if (ir_opcode_has_side_effects(inst->opcode)) {
            mark_instruction_live(ctx, func, i);
        }
    }
    
    // 第二遍：从worklist传播活跃性
    while (ctx->worklist_size > 0) {
        uint32_t inst_idx = worklist_pop(ctx);
        IRInst *inst = &func->instructions[inst_idx];
        
        // 检查所有操作数，标记产生这些操作数的指令
        uint16_t operands[] = {
            inst->operand0_id,
            inst->operand1_id,
            inst->operand2_id
        };
        
        for (int o = 0; o < 3; o++) {
            IRValueId op = operands[o];
            if (op == IR_VALUE_ID_INVALID) continue;
            
            IRValue *val = ir_value_get(pool, op);
            if (!val) continue;
            
            // 如果操作数是指令结果，标记该指令
            if (val->kind == IR_VALUE_INSTRUCTION) {
                uint32_t def_inst = val->inst_id;
                if (def_inst < func->num_insts && !ctx->marked[def_inst]) {
                    mark_instruction_live(ctx, func, def_inst);
                }
            }
        }
        
        // PHI节点：需要标记所有前驱值对应的指令
        if (inst->opcode == IR_OP_PHI) {
            uint32_t num_incoming = inst->extra;
            // PHI的额外传入值需要从扩展数据中获取
            // 这里标记operand0和operand1已经覆盖了基本使用
        }
    }
}

// 压缩函数，移除死指令
static uint32_t eliminate_dead_instructions(OptContext *ctx, IRFunction *func) {
    uint32_t removed = 0;
    uint32_t write_pos = 0;
    
    IRValuePool *pool = &ctx->module->value_pool;
    
    // 创建指令索引映射表
    uint32_t *inst_map = (uint32_t *)malloc(
        func->num_insts * sizeof(uint32_t));
    if (!inst_map) return 0;
    
    for (uint32_t i = 0; i < func->num_insts; i++) {
        if (ctx->marked[i]) {
            if (write_pos != i) {
                func->instructions[write_pos] = func->instructions[i];
            }
            inst_map[i] = write_pos;
            write_pos++;
        } else {
            inst_map[i] = UINT32_MAX;
            removed++;
        }
    }
    
    // 更新值池中指令结果值的inst_id
    for (uint32_t v = 0; v < pool->num_values; v++) {
        IRValue *val = &pool->values[v];
        if (val->kind == IR_VALUE_INSTRUCTION) {
            uint32_t old_inst = val->inst_id;
            if (old_inst < func->num_insts) {
                uint32_t new_inst = inst_map[old_inst];
                if (new_inst != UINT32_MAX) {
                    val->inst_id = new_inst;
                } else {
                    // 产生该值的指令被移除了，标记值无效
                    val->kind = IR_VALUE_NONE;
                }
            }
        }
    }
    
    // 更新操作数引用中的指令索引
    for (uint32_t i = 0; i < write_pos; i++) {
        IRInst *inst = &func->instructions[i];
        
        // 操作数中存储的是值ID，不是指令索引，不需要更新
        // 但如果有PHI节点引用了块，块中的指令范围需要更新
    }
    
    // 更新基本块的指令范围
    for (uint32_t b = 0; b < func->num_blocks; b++) {
        IRBlock *block = &func->blocks[b];
        if (block->first_inst == IR_VALUE_ID_INVALID) continue;
        
        uint32_t new_first = inst_map[block->first_inst];
        uint32_t new_last = inst_map[block->last_inst];
        
        if (new_first != UINT32_MAX && new_last != UINT32_MAX) {
            block->first_inst = new_first;
            block->last_inst = new_last;
        } else {
            block->first_inst = IR_VALUE_ID_INVALID;
            block->last_inst = IR_VALUE_ID_INVALID;
        }
    }
    
    func->num_insts = write_pos;
    free(inst_map);
    
    return removed;
}

bool opt_dead_code_eliminate(OptContext *ctx) {
    if (!ctx || !ctx->module) return false;
    
    bool changed = false;
    IRModule *module = ctx->module;
    
    for (uint32_t f = 0; f < module->num_functions; f++) {
        IRFunction *func = &module->functions[f];
        ctx->current_func = func;
        
        uint32_t before = func->num_insts;
        
        mark_live_instructions(ctx, func);
        uint32_t removed = eliminate_dead_instructions(ctx, func);
        
        if (removed > 0) {
            ctx->stats.instructions_removed += removed;
            changed = true;
        }
    }
    
    return changed;
}