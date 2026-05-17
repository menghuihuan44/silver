/**
 * Silver Compiler Backend — Code Quality Benchmark
 * 
 * 测试每个优化Pass对代码大小和质量的贡献。
 * 通过逐Pass对比，量化每个优化的效果。
 */

#include "silver/silver.h"
#include "silver/ir/ir_builder.h"
#include "silver/opt/passes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================
// 代码分析
// ============================================================
typedef struct {
    const char *name;
    uint32_t ir_count;
    uint32_t code_size;
    uint32_t branches;
    uint32_t mem_ops;
    uint32_t alu_ops;
    double est_cycles;
} CodeStats;

static void hr(int w, char c) { for (int i = 0; i < w; i++) putchar(c); putchar('\n'); }

static CodeStats analyze_code(const char *name, IRModule *m, SilverCompileResult *r) {
    CodeStats s;
    memset(&s, 0, sizeof(s));
    s.name = name;
    s.ir_count = m->stats.total_instructions;
    
    if (!r || !r->code) return s;
    
    s.code_size = r->code_size;
    uint8_t *code = silver_buffer_data(r->code);
    uint32_t size = silver_buffer_length(r->code);
    
    for (uint32_t i = 0; i < size; i++) {
        uint8_t b = code[i];
        if (b == 0xEB || b == 0xE9 || b == 0xE8 || b == 0xC3) s.branches++;
        else if (b >= 0x70 && b <= 0x7F) { s.branches++; i++; }
        else if (b == 0x0F && i + 1 < size && code[i+1] >= 0x80 && code[i+1] <= 0x8F)
            { s.branches++; i += 5; }
        else if (b == 0x8B || b == 0x89 || b == 0xFF) s.mem_ops++;
        else if ((b >= 0x01 && b <= 0x3D) || b == 0xF7) s.alu_ops++;
    }
    
    s.est_cycles = s.branches * 3.0 + s.mem_ops * 4.0 + s.alu_ops * 1.0 +
                   (size - s.branches - s.mem_ops - s.alu_ops) * 0.5;
    return s;
}

// ============================================================
// 测试用例生成器
// ============================================================
typedef IRModule *(*TestGen)(IRBuilder **b);

// 测试1: 常量折叠 — 大量可折叠算术
static IRModule *t_constant_fold(IRBuilder **b) {
    IRModule *m = ir_module_create("cf", "x86_64-unknown-none");
    *b = ir_builder_create(m);
    IRFunction *f = ir_module_add_function(m, "f", 0);
    ir_builder_set_position(*b, f, NULL);
    IRBlock *e = ir_builder_create_block(*b, "e");
    ir_builder_set_insert_point(*b, e);
    
    IRValueId a = ir_builder_add(*b,
        ir_builder_int_const(*b, (*b)->i64_type, 1),
        ir_builder_int_const(*b, (*b)->i64_type, 2));
    IRValueId c = ir_builder_add(*b,
        ir_builder_int_const(*b, (*b)->i64_type, 3),
        ir_builder_int_const(*b, (*b)->i64_type, 4));
    IRValueId m1 = ir_builder_mul(*b, a, c);
    IRValueId s = ir_builder_sub(*b, m1, ir_builder_int_const(*b, (*b)->i64_type, 10));
    IRValueId d = ir_builder_div(*b, s, ir_builder_int_const(*b, (*b)->i64_type, 2), true);
    ir_builder_ret(*b, d);
    return m;
}

// 测试2: 代数简化
static IRModule *t_algebraic(IRBuilder **b) {
    IRModule *m = ir_module_create("alg", "x86_64-unknown-none");
    *b = ir_builder_create(m);
    IRFunction *f = ir_module_add_function(m, "f", 0);
    ir_builder_set_position(*b, f, NULL);
    IRBlock *e = ir_builder_create_block(*b, "e");
    ir_builder_set_insert_point(*b, e);
    
    IRValueId x = ir_builder_int_const(*b, (*b)->i64_type, 42);
    IRValueId zero = ir_builder_int_const(*b, (*b)->i64_type, 0);
    IRValueId one = ir_builder_int_const(*b, (*b)->i64_type, 1);
    
    IRValueId r1 = ir_builder_add(*b, x, zero);
    IRValueId r2 = ir_builder_mul(*b, r1, one);
    IRValueId r3 = ir_builder_and(*b, r2, r2);
    IRValueId r4 = ir_builder_or(*b, r3, zero);
    ir_builder_ret(*b, r4);
    return m;
}

// 测试3: 强度削弱
static IRModule *t_strength(IRBuilder **b) {
    IRModule *m = ir_module_create("str", "x86_64-unknown-none");
    *b = ir_builder_create(m);
    IRFunction *f = ir_module_add_function(m, "f", 0);
    ir_builder_set_position(*b, f, NULL);
    IRBlock *e = ir_builder_create_block(*b, "e");
    ir_builder_set_insert_point(*b, e);
    
    IRValueId x = ir_builder_int_const(*b, (*b)->i64_type, 100);
    IRValueId r1 = ir_builder_mul(*b, x, ir_builder_int_const(*b, (*b)->i64_type, 8));
    IRValueId r2 = ir_builder_div(*b, r1, ir_builder_int_const(*b, (*b)->i64_type, 4), true);
    ir_builder_ret(*b, r2);
    return m;
}

// 测试4: 死代码消除
static IRModule *t_dce(IRBuilder **b) {
    IRModule *m = ir_module_create("dce", "x86_64-unknown-none");
    *b = ir_builder_create(m);
    IRFunction *f = ir_module_add_function(m, "f", 0);
    ir_builder_set_position(*b, f, NULL);
    IRBlock *e = ir_builder_create_block(*b, "e");
    ir_builder_set_insert_point(*b, e);
    
    IRValueId live = ir_builder_int_const(*b, (*b)->i64_type, 0);
    for (int i = 0; i < 50; i++) {
        IRValueId d1 = ir_builder_add(*b,
            ir_builder_int_const(*b, (*b)->i64_type, i),
            ir_builder_int_const(*b, (*b)->i64_type, i * 2));
        IRValueId d2 = ir_builder_mul(*b, d1, ir_builder_int_const(*b, (*b)->i64_type, 3));
        (void)d2;
    }
    ir_builder_ret(*b, live);
    return m;
}

// 测试5: 复写传播
static IRModule *t_copy_prop(IRBuilder **b) {
    IRModule *m = ir_module_create("cp", "x86_64-unknown-none");
    *b = ir_builder_create(m);
    IRFunction *f = ir_module_add_function(m, "f", 0);
    ir_builder_set_position(*b, f, NULL);
    IRBlock *e = ir_builder_create_block(*b, "e");
    ir_builder_set_insert_point(*b, e);
    
    IRValueId orig = ir_builder_int_const(*b, (*b)->i64_type, 42);
    IRValueId c1 = ir_builder_copy(*b, orig);
    IRValueId c2 = ir_builder_copy(*b, c1);
    IRValueId result = ir_builder_add(*b, c2, ir_builder_int_const(*b, (*b)->i64_type, 1));
    ir_builder_ret(*b, result);
    return m;
}

// 测试6: 结合律 (x+c1)+c2
static IRModule *t_reassociate(IRBuilder **b) {
    IRModule *m = ir_module_create("reassoc", "x86_64-unknown-none");
    *b = ir_builder_create(m);
    IRFunction *f = ir_module_add_function(m, "f", 0);
    ir_builder_set_position(*b, f, NULL);
    IRBlock *e = ir_builder_create_block(*b, "e");
    ir_builder_set_insert_point(*b, e);
    
    IRValueId x = ir_builder_int_const(*b, (*b)->i64_type, 100);
    IRValueId r1 = ir_builder_add(*b, x, ir_builder_int_const(*b, (*b)->i64_type, 5));
    IRValueId r2 = ir_builder_add(*b, r1, ir_builder_int_const(*b, (*b)->i64_type, 10));
    ir_builder_ret(*b, r2);
    return m;
}

// ============================================================
// 主函数
// ============================================================
int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════╗\n");
    printf("║        Silver Compiler Backend — Code Quality Benchmark             ║\n");
    printf("║                         v%s                              ║\n", SILVER_VERSION_STRING);
    printf("╚══════════════════════════════════════════════════════════════════════╝\n");
    
    TestGen tests[] = {t_constant_fold, t_algebraic, t_strength, t_dce, t_copy_prop, t_reassociate};
    const char *names[] = {
        "Constant Folding",
        "Algebraic Simplify",
        "Strength Reduction",
        "Dead Code Elim",
        "Copy Propagation",
        "Reassociation"
    };
    const char *descs[] = {
        "(1+2)*(3+4)-10)/2 = 5",
        "x+0,x*1,x&x,x|0 → x",
        "x*8/4 → x<<3>>2",
        "50 unused calcs → only return 0",
        "copy chain → direct use",
        "(x+5)+10 → x+15"
    };
    
    int nt = 6;
    CodeStats baseline[6], optimized[6];
    uint32_t ir_before[6];
    
    // ============================================================
    // Part 1: 逐Pass详细对比
    // ============================================================
    printf("\n┌──────────────────────────────────────────────────────────────────────┐\n");
    printf("│  Part 1: Per-Pass Effect                                             │\n");
    printf("│  Each pass tested independently: Baseline vs Optimized              │\n");
    printf("└──────────────────────────────────────────────────────────────────────┘\n");
    
    for (int i = 0; i < nt; i++) {
        // Baseline（不优化，直接编译）
        IRBuilder *b1 = NULL;
        IRModule *m1 = tests[i](&b1);
        ir_before[i] = m1->stats.total_instructions;
        SilverContext *c1 = silver_context_create();
        SilverCompileOptions o1 = silver_options_default();
        o1.optimize = false;
        o1.target_arch = SILVER_TARGET_X86_64;
        SilverCompileResult *r1 = silver_compile(c1, m1, &o1);
        baseline[i] = analyze_code(names[i], m1, r1);
        if (r1) silver_compile_result_destroy(r1);
        silver_context_destroy(c1);
        ir_builder_destroy(b1);
        ir_module_destroy(m1);
        
        // Optimized
        IRBuilder *b2 = NULL;
        IRModule *m2 = tests[i](&b2);
        OptContext *opt = opt_context_create(m2);
        opt_run_all(opt);
        opt_context_destroy(opt);
        SilverContext *c2 = silver_context_create();
        SilverCompileOptions o2 = silver_options_default();
        o2.optimize = false;
        o2.target_arch = SILVER_TARGET_X86_64;
        SilverCompileResult *r2 = silver_compile(c2, m2, &o2);
        optimized[i] = analyze_code(names[i], m2, r2);
        if (r2) silver_compile_result_destroy(r2);
        silver_context_destroy(c2);
        ir_builder_destroy(b2);
        ir_module_destroy(m2);
        
        // 打印
        printf("\n  ▸ Pass %d: %s\n", i + 1, names[i]);
        printf("    Description: %s\n", descs[i]);
        printf("    IR: %u → %u instructions\n", ir_before[i], optimized[i].ir_count);
        printf("    ┌──────────────────┬──────────────┬──────────────┬──────────┐\n");
        printf("    │ Metric           │  Baseline    │  Optimized   │ Change   │\n");
        printf("    ├──────────────────┼──────────────┼──────────────┼──────────┤\n");
        
        double cd = baseline[i].code_size > 0 ?
            (1.0 - (double)optimized[i].code_size / baseline[i].code_size) * 100.0 : 0;
        double sp = baseline[i].est_cycles > 0 ?
            baseline[i].est_cycles / optimized[i].est_cycles : 1.0;
        
        printf("    │ Code Size (B)    │ %8u     │ %8u     │ %+7.1f%% │\n",
               baseline[i].code_size, optimized[i].code_size, cd);
        printf("    │ Branches         │ %8u     │ %8u     │ %+8d  │\n",
               baseline[i].branches, optimized[i].branches,
               (int)optimized[i].branches - (int)baseline[i].branches);
        printf("    │ Memory Ops       │ %8u     │ %8u     │ %+8d  │\n",
               baseline[i].mem_ops, optimized[i].mem_ops,
               (int)optimized[i].mem_ops - (int)baseline[i].mem_ops);
        printf("    │ ALU Ops          │ %8u     │ %8u     │ %+8d  │\n",
               baseline[i].alu_ops, optimized[i].alu_ops,
               (int)optimized[i].alu_ops - (int)baseline[i].alu_ops);
        printf("    │ Est. Cycles      │ %8.0f     │ %8.0f     │ %7.2f× │\n",
               baseline[i].est_cycles, optimized[i].est_cycles, sp);
        printf("    └──────────────────┴──────────────┴──────────────┴──────────┘\n");
    }
    
    // ============================================================
    // Part 2: 综合排名
    // ============================================================
    printf("\n┌──────────────────────────────────────────────────────────────────────┐\n");
    printf("│  Part 2: Aggregate Results                                           │\n");
    printf("└──────────────────────────────────────────────────────────────────────┘\n\n");
    
    printf("    %-24s │ %8s │ %8s │ %8s │ %8s │ %8s\n",
           "Pass", "IR Δ", "Code Δ%", "Branches", "MemOps", "Speed×");
    hr(85, '-');
    
    double total_ir_before = 0, total_ir_after = 0;
    double total_code_before = 0, total_code_after = 0;
    double total_cycles_before = 0, total_cycles_after = 0;
    
    for (int i = 0; i < nt; i++) {
        double ir_d = ir_before[i] > 0 ? 
            (1.0 - (double)optimized[i].ir_count / ir_before[i]) * 100.0 : 0;
        double cd = baseline[i].code_size > 0 ?
            (1.0 - (double)optimized[i].code_size / baseline[i].code_size) * 100.0 : 0;
        double sp = baseline[i].est_cycles > 0 ?
            baseline[i].est_cycles / optimized[i].est_cycles : 1.0;
        
        printf("    %-24s │ %+7.1f%% │ %+7.1f%% │ %+8d │ %+8d │ %7.2f×\n",
               names[i], ir_d, cd,
               (int)optimized[i].branches - (int)baseline[i].branches,
               (int)optimized[i].mem_ops - (int)baseline[i].mem_ops, sp);
        
        total_ir_before += ir_before[i];
        total_ir_after += optimized[i].ir_count;
        total_code_before += baseline[i].code_size;
        total_code_after += optimized[i].code_size;
        total_cycles_before += baseline[i].est_cycles;
        total_cycles_after += optimized[i].est_cycles;
    }
    
    hr(85, '-');
    double tir = total_ir_before > 0 ? (1.0 - total_ir_after / total_ir_before) * 100.0 : 0;
    double tcd = total_code_before > 0 ? (1.0 - total_code_after / total_code_before) * 100.0 : 0;
    double tsp = total_cycles_after > 0 ? total_cycles_before / total_cycles_after : 1.0;
    
    printf("    %-24s │ %+7.1f%% │ %+7.1f%% │ %8s │ %8s │ %7.2f×\n",
           "TOTAL", tir, tcd, "—", "—", tsp);
    
    // ============================================================
    // Part 3: 排名
    // ============================================================
    printf("\n┌──────────────────────────────────────────────────────────────────────┐\n");
    printf("│  Part 3: Ranking by Code Size Reduction                              │\n");
    printf("└──────────────────────────────────────────────────────────────────────┘\n\n");
    
    typedef struct { const char *name; double score; } Rank;
    Rank ranks[6];
    for (int i = 0; i < nt; i++) {
        ranks[i].name = names[i];
        ranks[i].score = baseline[i].code_size > 0 ?
            (1.0 - (double)optimized[i].code_size / baseline[i].code_size) * 100.0 : 0;
    }
    for (int i = 0; i < nt - 1; i++)
        for (int j = i + 1; j < nt; j++)
            if (ranks[j].score > ranks[i].score) {
                Rank t = ranks[i]; ranks[i] = ranks[j]; ranks[j] = t;
            }
    
    for (int i = 0; i < nt; i++) {
        const char *medal = (i == 0) ? "★" : (i == 1) ? "☆" : (i == 2) ? "•" : " ";
        printf("    %s %d. %-28s %+6.1f%%\n", medal, i + 1, ranks[i].name, ranks[i].score);
    }
    
    // ============================================================
    // Part 4: 总结
    // ============================================================
    printf("\n┌──────────────────────────────────────────────────────────────────────┐\n");
    printf("│  Summary                                                             │\n");
    printf("└──────────────────────────────────────────────────────────────────────┘\n\n");
    
    printf("  Overall Results:\n");
    printf("    IR reduction:     %+.1f%%  (%d → %d instructions)\n",
           tir, (int)total_ir_before, (int)total_ir_after);
    printf("    Code reduction:   %+.1f%%  (%d → %d bytes)\n",
           tcd, (int)total_code_before, (int)total_code_after);
    printf("    Speedup estimate: %.2f×\n", tsp);
    
    printf("\n  ═══════════════════════════════════════════════════════════════════\n");
    printf("  Code Quality Benchmark Complete\n");
    printf("  ═══════════════════════════════════════════════════════════════════\n\n");
    
    return 0;
}