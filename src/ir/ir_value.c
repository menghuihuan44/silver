#include "silver/ir/ir_value.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define INITIAL_VALUE_CAPACITY 8192

void ir_value_pool_init(IRValuePool *pool, Arena *arena) {
    if (!pool) return;
    
    pool->capacity = INITIAL_VALUE_CAPACITY;
    pool->num_values = 0;
    pool->arena = arena;
    
    // 使用Arena分配初始数组
    if (arena) {
        pool->values = (IRValue *)arena_calloc(arena, 
            pool->capacity * sizeof(IRValue));
    } else {
        pool->values = (IRValue *)calloc(pool->capacity, sizeof(IRValue));
    }
    
    // 初始化常量缓存
    memset(pool->const_cache.ids, 0xFF, sizeof(pool->const_cache.ids));
    
    // 确保分配成功
    if (!pool->values) {
        pool->capacity = 0;
    } 
}

static bool ir_value_pool_ensure_capacity(IRValuePool *pool, uint32_t needed) {
    if (!pool) return false;
    if (needed <= pool->capacity) return true;
    
    uint32_t new_capacity = pool->capacity;
    while (new_capacity < needed) {
        new_capacity *= 2;
    }
    
    IRValue *new_values;
    if (pool->arena) {
        // 使用Arena分配
        new_values = (IRValue *)arena_calloc(pool->arena, 
            new_capacity * sizeof(IRValue));
        if (!new_values) {
            // Arena分配失败，回退到malloc
            new_values = (IRValue *)calloc(new_capacity, sizeof(IRValue));
        }
    } else {
        // 没有Arena，使用realloc
        new_values = (IRValue *)realloc(pool->values, 
            new_capacity * sizeof(IRValue));
        if (new_values) {
            // 清零新增部分
            memset(new_values + pool->capacity, 0,
                   (new_capacity - pool->capacity) * sizeof(IRValue));
        }
    }
    
    if (!new_values) return false;
    
    // 如果使用Arena（新分配），复制旧数据
    if (pool->arena && new_values != pool->values) {
        memcpy(new_values, pool->values, pool->num_values * sizeof(IRValue));
    }
    
    pool->values = new_values;
    pool->capacity = new_capacity;
    return true; 
}

IRValue *ir_value_get(IRValuePool *pool, IRValueId id) {
    if (!pool || !pool->values || id >= pool->num_values) return NULL;
    return &pool->values[id];
}

bool ir_value_is_valid(const IRValuePool *pool, IRValueId id) {
    if (!pool || !pool->values || id >= pool->num_values) return false;
    return pool->values[id].kind != IR_VALUE_NONE;
}

// 简单哈希函数（用于常量缓存）
static inline uint32_t hash_int_const(int64_t value, IRTypeId type_id) {
    uint64_t v = (uint64_t)value;
    v ^= (uint64_t)type_id << 32;
    v ^= v >> 32;
    return v % CONST_CACHE_SIZE;
}

static inline uint32_t hash_float_const(double value, IRTypeId type_id) {
    union { double d; uint64_t u; } u = { .d = value };
    uint64_t hash = u.u ^ ((uint64_t)type_id << 32);
    hash ^= hash >> 32;
    return hash % CONST_CACHE_SIZE;
}

IRValueId ir_value_create_int_const(IRValuePool *pool, IRTypeId type, int64_t value) {
    if (!pool || !pool->values) return IR_VALUE_ID_INVALID;
    
    // 检查常量缓存
    uint32_t hash = hash_int_const(value, type);
    IRValueId cached = pool->const_cache.ids[hash];
    if (cached != IR_VALUE_ID_INVALID && cached < pool->num_values) {
        IRValue *cv = &pool->values[cached];
        if (cv->kind == IR_VALUE_CONSTANT && cv->type_id == type && 
            cv->int_val == value) {
            return cached;
        }
    }
    
    if (!ir_value_pool_ensure_capacity(pool, pool->num_values + 1)) {
        return IR_VALUE_ID_INVALID;
    } 

    IRValueId id = pool->num_values++;
    IRValue *v = &pool->values[id];
    
    v->kind = IR_VALUE_CONSTANT;
    v->type_id = type;
    v->int_val = value;
    v->use_count = 0;
    
    pool->const_cache.ids[hash] = id;
    
    return id;
}

IRValueId ir_value_create_float_const(IRValuePool *pool, IRTypeId type, double value) {
    if (!pool || !pool->values) return IR_VALUE_ID_INVALID;
    
    // 检查NaN（NaN != NaN，所以无法去重）
    if (!isnan(value)) {
        uint32_t hash = hash_float_const(value, type);
        IRValueId cached = pool->const_cache.ids[hash];
        if (cached != IR_VALUE_ID_INVALID && cached < pool->num_values) {
            IRValue *cv = &pool->values[cached];
            if (cv->kind == IR_VALUE_CONSTANT && cv->type_id == type && 
                cv->float_val == value) {
                return cached;
            }
        }
    }

    if (!ir_value_pool_ensure_capacity(pool, pool->num_values + 1)) {
        return IR_VALUE_ID_INVALID;
    }
    
    IRValueId id = pool->num_values++;
    IRValue *v = &pool->values[id];
    
    v->kind = IR_VALUE_CONSTANT;
    v->type_id = type;
    v->float_val = value;
    v->use_count = 0;
    
    if (!isnan(value)) {
        uint32_t hash = hash_float_const(value, type);
        pool->const_cache.ids[hash] = id;
    }
    
    return id;
}

IRValueId ir_value_create_null_ptr(IRValuePool *pool, IRTypeId type) {
    return ir_value_create_int_const(pool, type, 0);
}

IRValueId ir_value_create_instruction(IRValuePool *pool, IRTypeId type, uint32_t inst_id) {
    if (!pool || !pool->values) return IR_VALUE_ID_INVALID;
    
    if (!ir_value_pool_ensure_capacity(pool, pool->num_values + 1)) {
        return IR_VALUE_ID_INVALID;
    }

    IRValueId id = pool->num_values++;
    IRValue *v = &pool->values[id];
    
    v->kind = IR_VALUE_INSTRUCTION;
    v->type_id = type;
    v->inst_id = inst_id;
    v->value_number = 0;
    v->use_count = 0;
    
    return id;
}

IRValueId ir_value_create_argument(IRValuePool *pool, IRTypeId type, uint32_t param_index) {
    if (!pool || !pool->values) return IR_VALUE_ID_INVALID;
    
    if (!ir_value_pool_ensure_capacity(pool, pool->num_values + 1)) {
        return IR_VALUE_ID_INVALID;
    }
 
    IRValueId id = pool->num_values++;
    IRValue *v = &pool->values[id];
    
    v->kind = IR_VALUE_ARGUMENT;
    v->type_id = type;
    v->param_index = param_index;
    v->param_attr = 0;
    v->use_count = 0;
    
    return id;
}

IRValueId ir_value_create_block(IRValuePool *pool, uint32_t block_id) {
    if (!pool || !pool->values) return IR_VALUE_ID_INVALID;
    
    if (!ir_value_pool_ensure_capacity(pool, pool->num_values + 1)) {
        return IR_VALUE_ID_INVALID;
    }

    IRValueId id = pool->num_values++;
    IRValue *v = &pool->values[id];
    
    v->kind = IR_VALUE_BLOCK;
    v->type_id = 0;  // 块没有类型
    v->block_id = block_id;
    v->block_flags = 0;
    v->use_count = 0;
    
    return id;
}

IRValueId ir_value_create_global(IRValuePool *pool, IRTypeId type, const char *name) {
    if (!pool || !pool->values) return IR_VALUE_ID_INVALID;
    
    if (!ir_value_pool_ensure_capacity(pool, pool->num_values + 1)) {
        return IR_VALUE_ID_INVALID;
    }

    IRValueId id = pool->num_values++;
    IRValue *v = &pool->values[id];
    
    v->kind = IR_VALUE_GLOBAL;
    v->type_id = type;
    v->global_name = name;
    v->global_flags = 0;
    v->use_count = 0;
    
    return id;
}

IRValueId ir_value_create_function(IRValuePool *pool, IRTypeId type, const char *name) {
    if (!pool || !pool->values) return IR_VALUE_ID_INVALID;
    
    if (!ir_value_pool_ensure_capacity(pool, pool->num_values + 1)) {
        return IR_VALUE_ID_INVALID;
    }

    IRValueId id = pool->num_values++;
    IRValue *v = &pool->values[id];
    
    v->kind = IR_VALUE_FUNCTION;
    v->type_id = type;
    v->func_name = name;
    v->func_flags = 0;
    v->use_count = 0;
    
    return id;
}

bool ir_value_is_constant(const IRValuePool *pool, IRValueId id) {
    if (!pool || id >= pool->num_values) return false;
    return pool->values[id].kind == IR_VALUE_CONSTANT;
}

bool ir_value_is_zero(const IRValuePool *pool, IRValueId id) {
    if (!pool || id >= pool->num_values) return false;
    const IRValue *v = &pool->values[id];
    
    if (v->kind != IR_VALUE_CONSTANT) return false;
    
    switch (v->type_id) {
        case 0: // 假设类型表中有i1, i8, i16, i32, i64等
        default:
            return v->int_val == 0;
    }
}

bool ir_value_is_all_ones(const IRValuePool *pool, IRValueId id) {
    if (!pool || id >= pool->num_values) return false;
    const IRValue *v = &pool->values[id];
    
    if (v->kind != IR_VALUE_CONSTANT) return false;
    return v->int_val == -1;
}

bool ir_value_get_int_constant(const IRValuePool *pool, IRValueId id, int64_t *out) {
    if (!pool || !out || id >= pool->num_values) return false;
    const IRValue *v = &pool->values[id];
    
    if (v->kind != IR_VALUE_CONSTANT) return false;
    *out = v->int_val;
    return true;
}

bool ir_value_get_float_constant(const IRValuePool *pool, IRValueId id, double *out) {
    if (!pool || !out || id >= pool->num_values) return false;
    const IRValue *v = &pool->values[id];
    
    if (v->kind != IR_VALUE_CONSTANT) return false;
    *out = v->float_val;
    return true;
}

const char *ir_value_to_string(const IRValuePool *pool, IRValueId id,
                                const IRTypeTable *types,
                                char *buffer, size_t buffer_size) {
    if (!buffer || buffer_size == 0) return "";
    if (!pool || id >= pool->num_values) {
        snprintf(buffer, buffer_size, "<invalid>");
        return buffer;
    }
    
    const IRValue *v = &pool->values[id];
    
    switch (v->kind) {
        case IR_VALUE_CONSTANT: {
            // 根据类型打印常量
            if (types && v->type_id < types->num_types) {
                const IRType *t = &types->types[v->type_id];
                if (t->kind == IR_TYPE_FLOAT) {
                    if (t->float_type.width == 32) {
                        snprintf(buffer, buffer_size, "%f", (float)v->float_val);
                    } else {
                        snprintf(buffer, buffer_size, "%lf", v->float_val);
                    }
                } else {
                    snprintf(buffer, buffer_size, "%lld", (long long)v->int_val);
                }
            } else {
                snprintf(buffer, buffer_size, "%lld", (long long)v->int_val);
            }
            break;
        }
        
        case IR_VALUE_INSTRUCTION:
            snprintf(buffer, buffer_size, "%%r%u", v->inst_id);
            break;
        
        case IR_VALUE_ARGUMENT:
            snprintf(buffer, buffer_size, "%%arg%u", v->param_index);
            break;
        
        case IR_VALUE_BLOCK:
            snprintf(buffer, buffer_size, "BB%u", v->block_id);
            break;
        
        case IR_VALUE_GLOBAL:
            snprintf(buffer, buffer_size, "@%s", v->global_name ? v->global_name : "<unnamed>");
            break;
        
        case IR_VALUE_FUNCTION:
            snprintf(buffer, buffer_size, "@%s", v->func_name ? v->func_name : "<unnamed>");
            break;
        
        default:
            snprintf(buffer, buffer_size, "<unknown>");
            break;
    }
    
    return buffer;
}