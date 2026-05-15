#ifndef SILVER_OPT_PASSES_H
#define SILVER_OPT_PASSES_H

#include "silver/ir/ir_module.h"
#include "silver/ir/ir_builder.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 优化Pass统计
typedef struct {
    uint32_t instructions_removed;
    uint32_t constants_folded;
    uint32_t algebraic_simplifications;
    uint32_t strength_reductions;
    uint32_t copies_propagated;
    uint32_t dead_blocks_removed;
    double time_ms;
} OptPassStats;

// 优化上下文
typedef struct {
    IRModule *module;
    IRFunction *current_func;
    IRBuilder *builder;         // 用于创建新指令
    OptPassStats stats;
    bool changed;              // 是否做了任何修改
    
    // 用于DCE的worklist
    uint32_t *worklist;
    uint32_t worklist_size;
    uint32_t worklist_capacity;
    
    // 标记数组（用于DCE）
    uint8_t *marked;           // 指令标记位
    uint32_t marked_size;
    
    // 值使用计数缓存
    uint32_t *use_counts;
} OptContext;

// 创建优化上下文
OptContext *opt_context_create(IRModule *module);

// 销毁优化上下文
void opt_context_destroy(OptContext *ctx);

// 运行所有优化Pass
bool opt_run_all(OptContext *ctx);

// 单个Pass
bool opt_const_fold(OptContext *ctx);
bool opt_algebraic_simplify(OptContext *ctx);
bool opt_strength_reduce(OptContext *ctx);
bool opt_dead_code_eliminate(OptContext *ctx);
bool opt_copy_propagate(OptContext *ctx);

// 基本的块内窥孔优化
bool opt_peephole(OptContext *ctx);

// 获取优化统计信息
void opt_get_stats(const OptContext *ctx, char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // SILVER_OPT_PASSES_H