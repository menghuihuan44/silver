#include "silver/opt/passes.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 前向声明各个pass的实现
#include "silver/opt/const_fold.h"
#include "silver/opt/algebraic.h"
#include "silver/opt/strength.h"
#include "silver/opt/dce.h"
#include "silver/opt/copy_prop.h"

OptContext *opt_context_create(IRModule *module) {
    if (!module) return NULL;
    
    OptContext *ctx = (OptContext *)calloc(1, sizeof(OptContext));
    if (!ctx) return NULL;
    
    ctx->module = module;
    ctx->builder = ir_builder_create(module);
    ctx->changed = false;
    
    memset(&ctx->stats, 0, sizeof(ctx->stats));
    
    return ctx;
}

void opt_context_destroy(OptContext *ctx) {
    if (!ctx) return;
    
    if (ctx->builder) {
        ir_builder_destroy(ctx->builder);
    }
    
    free(ctx->worklist);
    free(ctx->marked);
    free(ctx->use_counts);
    free(ctx);
}

bool opt_run_all(OptContext *ctx) {
    if (!ctx) return false;
    
    bool changed = false;
    
    // ============================================================
    // 迭代优化直到收敛（最多10遍）
    // 每遍：常量传播 → 代数简化 → 强度削弱 → 复写传播 → DCE → 窥孔
    // 上一遍的DCE可能暴露新的优化机会，下一遍继续处理
    // ============================================================
    for (int iter = 0; iter < 10; iter++) {
        bool pass_changed = false;
        
        // 常量传播 + 折叠
        pass_changed |= opt_const_fold(ctx);
        
        // 代数简化（x+0→x, x*1→x等）
        pass_changed |= opt_algebraic_simplify(ctx);
        
        // 强度削弱（x*2→x<<1, x/4→x>>2等）
        pass_changed |= opt_strength_reduce(ctx);
        
        // 复写传播（copy链替换）
        pass_changed |= opt_copy_propagate(ctx);
        
        // 死代码消除
        pass_changed |= opt_dead_code_eliminate(ctx);
        
        // 窥孔优化
        pass_changed |= opt_peephole(ctx);
        
        changed |= pass_changed;
        
        // 如果本遍没有任何优化触发，说明收敛了，提前退出
        if (!pass_changed) break;
    }
    
    ctx->changed = changed;
    return changed;
}

void opt_get_stats(const OptContext *ctx, char *buffer, size_t buffer_size) {
    if (!ctx || !buffer) return;
    
    snprintf(buffer, buffer_size,
        "Optimization Statistics:\n"
        "  Instructions removed: %u\n"
        "  Constants folded: %u\n"
        "  Algebraic simplifications: %u\n"
        "  Strength reductions: %u\n"
        "  Copies propagated: %u\n"
        "  Dead blocks removed: %u\n"
        "  Time: %.2f ms\n",
        ctx->stats.instructions_removed,
        ctx->stats.constants_folded,
        ctx->stats.algebraic_simplifications,
        ctx->stats.strength_reductions,
        ctx->stats.copies_propagated,
        ctx->stats.dead_blocks_removed,
        ctx->stats.time_ms);
}