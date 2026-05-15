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
    
    // 第一遍：前向扫描优化
    // ✅ 第一遍：常量传播
    changed |= opt_const_fold(ctx);
    
    // DCE
    changed |= opt_dead_code_eliminate(ctx);
    
    // ✅ 第二遍：常量传播（DCE可能暴露新的常量机会）
    changed |= opt_const_fold(ctx);

    // 代数简化
    changed |= opt_algebraic_simplify(ctx);
    
    // 强度削弱
    changed |= opt_strength_reduce(ctx);
    
    // 窥孔优化
    changed |= opt_peephole(ctx);
    
    // 第二遍：后向扫描+Worklist优化
    // 复写传播
    changed |= opt_copy_propagate(ctx);
    
    // 死代码消除
    changed |= opt_dead_code_eliminate(ctx);
    
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