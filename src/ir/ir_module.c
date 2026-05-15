#include "silver/ir/ir_module.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

// 初始容量设置
#define INITIAL_FUNC_CAPACITY 64
#define INITIAL_GLOBAL_CAPACITY 128
#define INITIAL_BLOCK_CAPACITY 128
#define INITIAL_INST_CAPACITY 8192

IRModule *ir_module_create(const char *name, const char *target_triple) {
    IRModule *module = (IRModule *)calloc(1, sizeof(IRModule));
    if (!module) return NULL;
    
    // 创建Arena
    module->arena = arena_create();
    if (!module->arena) {
        free(module);
        return NULL;
    }
    
    // 创建错误上下文
    module->error_ctx = silver_error_context_create();
    if (!module->error_ctx) {
        arena_destroy(module->arena);
        free(module);
        return NULL;
    }
    
    // 设置模块名称
    module->module_name = arena_strdup(module->arena, name ? name : "unnamed");
    
    // 设置目标三元组
    module->target_triple = arena_strdup(module->arena, 
        target_triple ? target_triple : "x86_64-unknown-none");
    
    // 默认数据布局
    module->data_layout = arena_strdup(module->arena, 
        "e-m:e-p:64:64-i64:64-f80:128-n8:16:32:64-S128");
    
    // 初始化类型表
    ir_type_table_init(&module->type_table, module->arena);
    
    // 初始化值池
    ir_value_pool_init(&module->value_pool, module->arena);
    
    // 分配函数数组
    module->functions = (IRFunction *)arena_calloc(module->arena, 
        INITIAL_FUNC_CAPACITY * sizeof(IRFunction));
    if (!module->functions) {
        ir_module_destroy(module);
        return NULL;
    }
    module->func_capacity = INITIAL_FUNC_CAPACITY;
    module->num_functions = 0;
    
    // 分配全局变量数组
    module->globals = (IRGlobal *)arena_calloc(module->arena,
        INITIAL_GLOBAL_CAPACITY * sizeof(IRGlobal));
    if (!module->globals) {
        ir_module_destroy(module);
        return NULL;
    }
    module->global_capacity = INITIAL_GLOBAL_CAPACITY;
    module->num_globals = 0;
    
    module->flags = 0;
    memset(&module->stats, 0, sizeof(module->stats));
    
    return module;
}

void ir_module_destroy(IRModule *module) {
    if (!module) return;
    
    if (module->error_ctx) {
        silver_error_context_destroy(module->error_ctx);
    }
    
    if (module->arena) {
        arena_destroy(module->arena);
    }
    
    free(module);
}

// 扩展数组容量（泛型）
static bool ensure_capacity(void **array, uint32_t *capacity, 
                           uint32_t needed, size_t elem_size,
                           Arena *arena) {
    if (*capacity >= needed) return true;
    
    uint32_t new_capacity = *capacity * 2;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }
    
    void *new_array = arena_calloc(arena, new_capacity * elem_size);
    if (!new_array) return false;
    
    if (*array) {
        memcpy(new_array, *array, (*capacity) * elem_size);
    }
    
    *array = new_array;
    *capacity = new_capacity;
    return true;
}

IRFunction *ir_module_add_function(IRModule *module, const char *name,
                                    IRTypeId func_type_id) {
    if (!module) return NULL;
    
    if (!ensure_capacity((void **)&module->functions, &module->func_capacity,
                         module->num_functions + 1, sizeof(IRFunction),
                         module->arena)) {
        return NULL;
    }
    
    IRFunction *func = &module->functions[module->num_functions];
    memset(func, 0, sizeof(IRFunction));
    
    func->id = module->num_functions;
    func->name = arena_strdup(module->arena, name);
    func->func_type_id = func_type_id;
    func->module = module;
    
    // 分配基本块数组
    func->blocks = (IRBlock *)arena_calloc(module->arena,
        INITIAL_BLOCK_CAPACITY * sizeof(IRBlock));
    func->block_capacity = INITIAL_BLOCK_CAPACITY;
    func->num_blocks = 0;
    
    // 分配指令数组
    func->instructions = (IRInst *)arena_calloc(module->arena,
        INITIAL_INST_CAPACITY * sizeof(IRInst));
    func->inst_capacity = INITIAL_INST_CAPACITY;
    func->num_insts = 0;
    
    // 分配 inst_to_value 映射表
    func->inst_to_value_capacity = INITIAL_INST_CAPACITY;
    func->inst_to_value = (IRValueId *)arena_calloc(module->arena,
        func->inst_to_value_capacity * sizeof(IRValueId));
    if (func->inst_to_value) {
        for (uint32_t i = 0; i < func->inst_to_value_capacity; i++) {
            func->inst_to_value[i] = IR_VALUE_ID_INVALID;
        }
    }
    
    // 安全地处理函数类型
    func->num_args = 0;
    func->arg_values = NULL;
    
    if (func_type_id != IR_TYPE_ID_INVALID && 
        func_type_id < module->type_table.num_types) {
        IRType *func_type = &module->type_table.types[func_type_id];
        if (func_type->kind == IR_TYPE_FUNC) {
            func->num_args = func_type->func_type.num_params;
            if (func->num_args > 0) {
                func->arg_values = (IRValueId *)arena_calloc(module->arena,
                    func->num_args * sizeof(IRValueId));
                
                // 创建参数值
                for (uint32_t i = 0; i < func->num_args; i++) {
                    func->arg_values[i] = ir_value_create_argument(
                        &module->value_pool,
                        func_type->func_type.param_types[i],
                        i);
                }
            }
        }
    }
    
    module->num_functions++;
    return func; 
}

IRFunction *ir_module_get_function(const IRModule *module, uint32_t id) {
    if (!module || id >= module->num_functions) return NULL;
    return &module->functions[id];
}

IRFunction *ir_module_find_function(const IRModule *module, const char *name) {
    if (!module || !name) return NULL;
    
    for (uint32_t i = 0; i < module->num_functions; i++) {
        if (module->functions[i].name && 
            strcmp(module->functions[i].name, name) == 0) {
            return &module->functions[i];
        }
    }
    return NULL;
}

IRGlobal *ir_module_add_global(IRModule *module, const char *name,
                                IRTypeId type_id, IRValueId init_value) {
    if (!module) return NULL;
    
    // 确保容量
    if (!ensure_capacity((void **)&module->globals, &module->global_capacity,
                         module->num_globals + 1, sizeof(IRGlobal),
                         module->arena)) {
        return NULL;
    }
    
    IRGlobal *global = &module->globals[module->num_globals];
    memset(global, 0, sizeof(IRGlobal));
    
    global->id = module->num_globals;
    global->name = arena_strdup(module->arena, name);
    global->type_id = type_id;
    global->init_value = init_value;
    global->alignment = ir_type_alignment(&module->type_table, type_id);
    global->flags = 0;
    global->size = ir_type_size(&module->type_table, type_id);
    
    module->num_globals++;
    return global;
}

IRGlobal *ir_module_get_global(const IRModule *module, uint32_t id) {
    if (!module || id >= module->num_globals) return NULL;
    return &module->globals[id];
}

IRGlobal *ir_module_find_global(const IRModule *module, const char *name) {
    if (!module || !name) return NULL;
    
    for (uint32_t i = 0; i < module->num_globals; i++) {
        if (module->globals[i].name && 
            strcmp(module->globals[i].name, name) == 0) {
            return &module->globals[i];
        }
    }
    return NULL;
}

IRBlock *ir_function_add_block(IRFunction *func, const char *name) {
    if (!func || !func->module) return NULL;
    
    IRModule *module = func->module;
    
    // 检查并扩展基本块数组
    if (!ensure_capacity((void **)&func->blocks, &func->block_capacity,
                                func->num_blocks + 1, sizeof(IRBlock),
                                module->arena)) {
        return NULL;
    } 
    
    IRBlock *block = &func->blocks[func->num_blocks];
    memset(block, 0, sizeof(IRBlock));
    
    block->id = func->num_blocks;
    block->name = name ? arena_strdup(module->arena, name) : NULL;
    block->first_inst = IR_VALUE_ID_INVALID;
    block->last_inst = IR_VALUE_ID_INVALID;
    block->is_sealed = false;
    
    ir_value_create_block(&module->value_pool, block->id);
    
    func->num_blocks++;
    return block; 
}

IRBlock *ir_function_get_block(const IRFunction *func, uint32_t id) {
    if (!func || id >= func->num_blocks) return NULL;
    return &func->blocks[id];
}

IRInst *ir_function_add_inst(IRFunction *func, IROpcode opcode,
                              IRTypeId type_id, uint16_t flags,
                              IRValueId op0, IRValueId op1, IRValueId op2,
                              uint16_t extra) {
    if (!func || !func->module) return NULL;
    
    IRModule *module = func->module;
    
    // 检查并扩展指令数组容量
    if (!ensure_capacity((void **)&func->instructions, &func->inst_capacity,
                                func->num_insts + 1, sizeof(IRInst),
                                module->arena)) {
        return NULL;
    } 

    // 同步扩展 inst_to_value 映射表
    if (func->inst_to_value && func->num_insts >= func->inst_to_value_capacity) {
        uint32_t new_cap = func->inst_to_value_capacity * 2;
        while (new_cap <= func->num_insts) new_cap *= 2;
        IRValueId *new_map = (IRValueId *)arena_calloc(module->arena,
            new_cap * sizeof(IRValueId));
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
    
    IRInst *inst = &func->instructions[func->num_insts];
    memset(inst, 0, sizeof(IRInst));
    
    inst->opcode = opcode;
    inst->type_id = type_id;
    inst->flags = flags;
    inst->operand0_id = (uint16_t)op0;
    inst->operand1_id = (uint16_t)op1;
    inst->operand2_id = (uint16_t)op2;
    inst->extra = extra;
    inst->original_order = (uint16_t)func->num_insts;
    
    func->num_insts++;
    module->stats.total_instructions++;
    
    return inst; 
}

void ir_block_update_range(IRBlock *block, uint32_t first_inst, uint32_t last_inst) {
    if (!block) return;
    block->first_inst = first_inst;
    block->last_inst = last_inst;
}

bool ir_module_verify(IRModule *module) {
    if (!module) return false;
    
    bool valid = true;
    
    // 验证类型表
    // 验证值池
    // 验证函数
    for (uint32_t i = 0; i < module->num_functions && valid; i++) {
        IRFunction *func = &module->functions[i];
        
        // 检查基本块
        for (uint32_t j = 0; j < func->num_blocks && valid; j++) {
            IRBlock *block = &func->blocks[j];
            
            // 检查指令范围
            if (block->first_inst != IR_VALUE_ID_INVALID) {
                if (block->first_inst >= func->num_insts) {
                    silver_error_add(module->error_ctx, SILVER_ERROR_ERROR,
                        SILVER_ERR_IR_INSTRUCTION_LIMIT, NULL,
                        "Block %u in function '%s': first instruction %u out of range",
                        j, func->name, block->first_inst);
                    valid = false;
                }
                
                if (block->last_inst >= func->num_insts) {
                    silver_error_add(module->error_ctx, SILVER_ERROR_ERROR,
                        SILVER_ERR_IR_INSTRUCTION_LIMIT, NULL,
                        "Block %u in function '%s': last instruction %u out of range",
                        j, func->name, block->last_inst);
                    valid = false;
                }
                
                if (block->first_inst > block->last_inst) {
                    silver_error_add(module->error_ctx, SILVER_ERROR_ERROR,
                        SILVER_ERR_IR_INVALID_OPERAND, NULL,
                        "Block %u in function '%s': first_inst > last_inst",
                        j, func->name);
                    valid = false;
                }
            }
            
            // 检查终结指令
            if (block->is_sealed && block->last_inst != IR_VALUE_ID_INVALID) {
                IRInst *term = &func->instructions[block->last_inst];
                if (!ir_opcode_is_terminator(term->opcode)) {
                    silver_error_add(module->error_ctx, SILVER_ERROR_ERROR,
                        SILVER_ERR_IR_INVALID_OPCODE, NULL,
                        "Block %u in function '%s': last instruction is not a terminator",
                        j, func->name);
                    valid = false;
                }
            }
        }
    }
    
    // 验证全局变量
    for (uint32_t i = 0; i < module->num_globals && valid; i++) {
        IRGlobal *global = &module->globals[i];
        
        if (global->type_id >= module->type_table.num_types) {
            silver_error_add(module->error_ctx, SILVER_ERROR_ERROR,
                SILVER_ERR_IR_INVALID_TYPE, NULL,
                "Global '%s': invalid type ID %u",
                global->name, global->type_id);
            valid = false;
        }
    }
    
    return valid;
}

void ir_module_dump(const IRModule *module, FILE *out) {
    if (!module || !out) return;
    
    fprintf(out, ";; Silver IR Module: %s\n", module->module_name);
    fprintf(out, ";; Target: %s\n", module->target_triple);
    fprintf(out, ";; Data Layout: %s\n\n", module->data_layout);
    
    // 打印类型
    fprintf(out, ";; Types (%u):\n", module->type_table.num_types);
    for (uint32_t i = 0; i < module->type_table.num_types; i++) {
        char type_str[128];
        ir_type_to_string(&module->type_table, i, type_str, sizeof(type_str));
        fprintf(out, ";;   %u: %s\n", i, type_str);
    }
    fprintf(out, "\n");
    
    // 打印全局变量
    if (module->num_globals > 0) {
        fprintf(out, ";; Global Variables:\n");
        for (uint32_t i = 0; i < module->num_globals; i++) {
            IRGlobal *global = &module->globals[i];
            char type_str[128];
            ir_type_to_string(&module->type_table, global->type_id, 
                            type_str, sizeof(type_str));
            fprintf(out, "  @%s = global %s", global->name, type_str);
            
            if (global->init_value != IR_VALUE_ID_INVALID) {
                char val_str[64];
                ir_value_to_string(&module->value_pool, global->init_value,
                                  &module->type_table, val_str, sizeof(val_str));
                fprintf(out, " %s", val_str);
            }
            fprintf(out, "\n");
        }
        fprintf(out, "\n");
    }
    
    // 打印函数
    for (uint32_t i = 0; i < module->num_functions; i++) {
        IRFunction *func = &module->functions[i];
        char func_type_str[256];
        ir_type_to_string(&module->type_table, func->func_type_id,
                         func_type_str, sizeof(func_type_str));
        
        fprintf(out, "define %s @%s(", func_type_str, func->name);
        
        // 打印参数
        for (uint32_t j = 0; j < func->num_args; j++) {
            if (j > 0) fprintf(out, ", ");
            char param_str[64];
            ir_value_to_string(&module->value_pool, func->arg_values[j],
                              &module->type_table, param_str, sizeof(param_str));
            fprintf(out, "%s", param_str);
        }
        fprintf(out, ") {\n");
        
        // 打印基本块
        for (uint32_t j = 0; j < func->num_blocks; j++) {
            IRBlock *block = &func->blocks[j];
            fprintf(out, "%sBB%u:", block->name ? block->name : "", j);
            
            // 打印前驱
            if (block->num_predecessors > 0) {
                fprintf(out, "  ; preds = ");
                for (uint32_t k = 0; k < block->num_predecessors; k++) {
                    if (k > 0) fprintf(out, ", ");
                    fprintf(out, "BB%u", block->predecessors[k]);
                }
            }
            fprintf(out, "\n");
            
            // 打印指令
            if (block->first_inst != IR_VALUE_ID_INVALID) {
                for (uint32_t k = block->first_inst; k <= block->last_inst; k++) {
                    IRInst *inst = &func->instructions[k];
                    char inst_str[256];
                    ir_inst_to_string(inst, &module->type_table,
                                     &module->value_pool,
                                     inst_str, sizeof(inst_str));
                    fprintf(out, "  %s\n", inst_str);
                }
            }
        }
        fprintf(out, "}\n\n");
    }
    
    // 打印统计信息
    fprintf(out, ";; Statistics:\n");
    fprintf(out, ";;   Functions: %u\n", module->num_functions);
    fprintf(out, ";;   Globals: %u\n", module->num_globals);
    fprintf(out, ";;   Total Instructions: %u\n", module->stats.total_instructions);
    fprintf(out, ";;   Total Values: %u\n", module->value_pool.num_values);
}

void ir_module_get_stats(const IRModule *module, 
                         char *buffer, size_t buffer_size) {
    if (!module || !buffer) return;
    
    snprintf(buffer, buffer_size,
        "Module: %s\n"
        "  Functions: %u\n"
        "  Globals: %u\n"
        "  Total Instructions: %u\n"
        "  Total Values: %u\n"
        "  Total Types: %u\n"
        "  Arena: %.1f KB used / %.1f KB allocated (%.1f%% utilization)\n"
        "  Errors: %u, Warnings: %u\n",
        module->module_name,
        module->num_functions,
        module->num_globals,
        module->stats.total_instructions,
        module->value_pool.num_values,
        module->type_table.num_types,
        (double)module->arena->total_used / 1024.0,
        (double)module->arena->total_allocated / 1024.0,
        module->arena->total_allocated > 0 ? 
            (double)module->arena->total_used / (double)module->arena->total_allocated * 100.0 : 0.0,
        module->error_ctx->error_count,
        module->error_ctx->warning_count);
}