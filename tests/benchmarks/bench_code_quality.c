#include "silver/silver.h"
#include "silver/ir/ir_builder.h"
#include "silver/opt/passes.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

// ============================================================
// 代码质量指标结构
// ============================================================
typedef struct {
    const char *test_name;
    uint32_t ir_before;
    uint32_t ir_after;
    uint32_t code_size;
    uint32_t num_branches;
    uint32_t num_memory_ops;
    uint32_t num_alu_ops;
    uint32_t num_spills;
    double code_density;
    double ir_reduction_pct;
    double code_reduction_pct;
    double estimated_cycles;
} QualityMetrics;

// ============================================================
// 分析机器代码
// ============================================================
static QualityMetrics analyze_code(const char *name,
                                    uint32_t ir_before, uint32_t ir_after,
                                    SilverCompileResult *result) {
    QualityMetrics m;
    memset(&m, 0, sizeof(m));
    m.test_name = name;
    m.ir_before = ir_before;
    m.ir_after = ir_after;
    m.code_size = result ? result->code_size : 0;
    
    if (ir_before > 0) {
        m.ir_reduction_pct = (1.0 - (double)ir_after / ir_before) * 100.0;
    }
    if (m.code_size > 0) {
        m.code_density = (double)ir_after / m.code_size;
    }
    
    if (result && result->code) {
        uint8_t *code = silver_buffer_data(result->code);
        uint32_t size = silver_buffer_length(result->code);
        
        for (uint32_t i = 0; i < size; i++) {
            uint8_t byte = code[i];
            
            // x86-64 分支指令统计
            if (byte == 0xEB || byte == 0xE9 || byte == 0xE8 || byte == 0xC3) {
                m.num_branches++;
            } else if (byte >= 0x70 && byte <= 0x7F) {
                m.num_branches++;
                i++;
            } else if (byte == 0x0F && i + 1 < size) {
                uint8_t next = code[i + 1];
                if (next >= 0x80 && next <= 0x8F) {
                    m.num_branches++;
                    i += 5;
                }
            }
            
            // 内存操作
            if (byte == 0x8B || byte == 0x89 || byte == 0xFF) {
                m.num_memory_ops++;
            }
            
            // ALU操作
            if ((byte >= 0x01 && byte <= 0x3D) || byte == 0xF7 || byte == 0xFF) {
                m.num_alu_ops++;
            }
        }
        
        // 估算周期（简化模型）
        m.estimated_cycles = m.num_branches * 3.0 +
                             m.num_memory_ops * 4.0 +
                             m.num_alu_ops * 1.0 +
                             (size - m.num_branches - m.num_memory_ops - m.num_alu_ops) * 0.5;
    }
    
    return m;
}

// ============================================================
// 测试用例生成
// ============================================================

typedef IRModule *(*TestGenerator)(IRBuilder **out_builder);

// 常量折叠测试
static IRModule *gen_constant_fold(IRBuilder **out_builder) {
    IRModule *m = ir_module_create("cf", "x86_64-unknown-none");
    IRBuilder *b = ir_builder_create(m);
    IRFunction *f = ir_module_add_function(m, "cf", 0);
    ir_builder_set_position(b, f, NULL);
    IRBlock *e = ir_builder_create_block(b, "e");
    ir_builder_set_insert_point(b, e);
    
    // 大量可折叠表达式
    IRValueId a = ir_builder_add(b,
        ir_builder_int_const(b, b->i64_type, 1),
        ir_builder_int_const(b, b->i64_type, 2));
    IRValueId c = ir_builder_add(b,
        ir_builder_int_const(b, b->i64_type, 3),
        ir_builder_int_const(b, b->i64_type, 4));
    IRValueId m1 = ir_builder_mul(b, a, c);
    IRValueId s = ir_builder_sub(b, m1,
        ir_builder_int_const(b, b->i64_type, 10));
    IRValueId d = ir_builder_div(b, s,
        ir_builder_int_const(b, b->i64_type, 2), true);
    ir_builder_ret(b, d);
    
    *out_builder = b;
    return m;
}

// 代数简化测试
static IRModule *gen_algebraic(IRBuilder **out_builder) {
    IRModule *m = ir_module_create("alg", "x86_64-unknown-none");
    IRBuilder *b = ir_builder_create(m);
    IRFunction *f = ir_module_add_function(m, "alg", 0);
    ir_builder_set_position(b, f, NULL);
    IRBlock *e = ir_builder_create_block(b, "e");
    ir_builder_set_insert_point(b, e);
    
    IRValueId x = ir_builder_int_const(b, b->i64_type, 42);
    IRValueId zero = ir_builder_int_const(b, b->i64_type, 0);
    IRValueId one = ir_builder_int_const(b, b->i64_type, 1);
    
    IRValueId r1 = ir_builder_add(b, x, zero);      // x+0 → x
    IRValueId r2 = ir_builder_mul(b, r1, one);       // x*1 → x
    IRValueId r3 = ir_builder_and(b, r2, r2);        // x&x → x
    IRValueId r4 = ir_builder_or(b, r3, zero);       // x|0 → x
    ir_builder_ret(b, r4);
    
    *out_builder = b;
    return m;
}

// 强度削弱测试
static IRModule *gen_strength(IRBuilder **out_builder) {
    IRModule *m = ir_module_create("str", "x86_64-unknown-none");
    IRBuilder *b = ir_builder_create(m);
    IRFunction *f = ir_module_add_function(m, "str", 0);
    ir_builder_set_position(b, f, NULL);
    IRBlock *e = ir_builder_create_block(b, "e");
    ir_builder_set_insert_point(b, e);
    
    IRValueId x = ir_builder_int_const(b, b->i64_type, 100);
    IRValueId r1 = ir_builder_mul(b, x, ir_builder_int_const(b, b->i64_type, 8));   // x*8 → x<<3
    IRValueId r2 = ir_builder_div(b, r1, ir_builder_int_const(b, b->i64_type, 4), true); // /4 → >>2
    ir_builder_ret(b, r2);
    
    *out_builder = b;
    return m;
}

// 死代码消除测试
static IRModule *gen_dce(IRBuilder **out_builder) {
    IRModule *m = ir_module_create("dce", "x86_64-unknown-none");
    IRBuilder *b = ir_builder_create(m);
    IRFunction *f = ir_module_add_function(m, "dce", 0);
    ir_builder_set_position(b, f, NULL);
    IRBlock *e = ir_builder_create_block(b, "e");
    ir_builder_set_insert_point(b, e);
    
    IRValueId live = ir_builder_int_const(b, b->i64_type, 0);
    for (int i = 0; i < 50; i++) {
        IRValueId d1 = ir_builder_add(b,
            ir_builder_int_const(b, b->i64_type, i),
            ir_builder_int_const(b, b->i64_type, i * 2));
        IRValueId d2 = ir_builder_mul(b, d1, ir_builder_int_const(b, b->i64_type, 3));
        (void)d2;
    }
    ir_builder_ret(b, live);
    
    *out_builder = b;
    return m;
}

// 复写传播测试
static IRModule *gen_copy_prop(IRBuilder **out_builder) {
    IRModule *m = ir_module_create("cp", "x86_64-unknown-none");
    IRBuilder *b = ir_builder_create(m);
    IRFunction *f = ir_module_add_function(m, "cp", 0);
    ir_builder_set_position(b, f, NULL);
    IRBlock *e = ir_builder_create_block(b, "e");
    ir_builder_set_insert_point(b, e);
    
    IRValueId orig = ir_builder_int_const(b, b->i64_type, 42);
    IRValueId c1 = ir_builder_copy(b, orig);
    IRValueId c2 = ir_builder_copy(b, c1);
    IRValueId result = ir_builder_add(b, c2, ir_builder_int_const(b, b->i64_type, 1));
    ir_builder_ret(b, result);
    
    *out_builder = b;
    return m;
}

// ============================================================
// 格式化输出
// ============================================================
static void print_separator(int w) { for (int i = 0; i < w; i++) putchar('='); putchar('\n'); }
static void print_dash(int w) { for (int i = 0; i < w; i++) putchar('-'); putchar('\n'); }

// ============================================================
// 主函数
// ============================================================
int main(int argc, char **argv) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║       Silver Compiler Backend — Code Quality Benchmarks          ║\n");
    printf("║                      v%s                          ║\n", SILVER_VERSION_STRING);
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    
    TestGenerator generators[] = {
        gen_constant_fold, gen_algebraic, gen_strength, gen_dce, gen_copy_prop
    };
    const char *names[] = {
        "Constant Folding",
        "Algebraic Simplification",
        "Strength Reduction",
        "Dead Code Elimination",
        "Copy Propagation"
    };
    const char *descriptions[] = {
        "Arithmetic on constants: (1+2)*(3+4)-10)/2 = 5",
        "Identity ops: x+0, x*1, x&x, x|0 → x",
        "x*8 /4 → x<<3 >>2 → x<<1",
        "50 unused computations removed, only 'return 0' remains",
        "Copy chain: orig→c1→c2→c3 replaced by direct use of orig"
    };
    
    int num_tests = 5;
    
    QualityMetrics noopt[5], opt[5];
    uint32_t ir_before[5];
    
    // ================================================================
    // 第1节：逐测试详细对比
    // ================================================================
    printf("\n");
    printf("┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│  Section 1: Per-Pass Detailed Comparison                         │\n");
    printf("│  No optimization vs Full optimization                            │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n");
    
    for (int i = 0; i < num_tests; i++) {
        printf("\n");
        printf("  ▸ Pass %d: %s\n", i + 1, names[i]);
        printf("    Description: %s\n", descriptions[i]);
        printf("\n");
        
        // 创建模块
        IRBuilder *b1 = NULL;
        IRModule *m1 = generators[i](&b1);
        ir_before[i] = m1->stats.total_instructions;
        
        // 不优化编译
        SilverContext *ctx1 = silver_context_create();
        SilverCompileOptions opts1 = silver_options_default();
        opts1.optimize = false;
        opts1.target_arch = SILVER_TARGET_X86_64;
        SilverCompileResult *r1 = silver_compile(ctx1, m1, &opts1);
        noopt[i] = analyze_code(names[i], ir_before[i], ir_before[i], r1);
        
        // 优化编译
        IRBuilder *b2 = NULL;
        IRModule *m2 = generators[i](&b2);
        OptContext *opt_ctx = opt_context_create(m2);
        opt_run_all(opt_ctx);
        uint32_t ir_after = m2->stats.total_instructions;
        opt_context_destroy(opt_ctx);
        
        SilverContext *ctx2 = silver_context_create();
        SilverCompileOptions opts2 = silver_options_default();
        opts2.optimize = false;  // 已经手动优化了
        opts2.target_arch = SILVER_TARGET_X86_64;
        SilverCompileResult *r2 = silver_compile(ctx2, m2, &opts2);
        opt[i] = analyze_code(names[i], ir_before[i], ir_after, r2);
        
        // 计算变化
        double ir_change = ir_before[i] > 0 ? 
            (1.0 - (double)ir_after / ir_before[i]) * 100.0 : 0;
        double code_change = noopt[i].code_size > 0 ?
            (1.0 - (double)opt[i].code_size / noopt[i].code_size) * 100.0 : 0;
        double cycle_change = noopt[i].estimated_cycles > 0 ?
            noopt[i].estimated_cycles / opt[i].estimated_cycles : 1.0;
        
        // 打印对比表
        printf("    ┌──────────────────┬──────────────┬──────────────┬──────────┐\n");
        printf("    │ Metric           │  No Opt      │  Optimized   │ Change   │\n");
        printf("    ├──────────────────┼──────────────┼──────────────┼──────────┤\n");
        printf("    │ IR Instructions  │ %8u    │ %8u    │ %+7.1f%% │\n",
               ir_before[i], ir_after, ir_change);
        printf("    │ Code Size (B)    │ %8u    │ %8u    │ %+7.1f%% │\n",
               noopt[i].code_size, opt[i].code_size, code_change);
        printf("    │ Branches         │ %8u    │ %8u    │ %+8d │\n",
               noopt[i].num_branches, opt[i].num_branches,
               (int)opt[i].num_branches - (int)noopt[i].num_branches);
        printf("    │ Memory Ops       │ %8u    │ %8u    │ %+8d │\n",
               noopt[i].num_memory_ops, opt[i].num_memory_ops,
               (int)opt[i].num_memory_ops - (int)noopt[i].num_memory_ops);
        printf("    │ Est. Cycles      │ %8.0f    │ %8.0f    │ %7.2f× │\n",
               noopt[i].estimated_cycles, opt[i].estimated_cycles, cycle_change);
        printf("    │ Code Density     │ %7.2f/IR   │ %7.2f/IR   │          │\n",
               noopt[i].code_density, opt[i].code_density);
        printf("    └──────────────────┴──────────────┴──────────────┴──────────┘\n");
        
        // 清理
        if (r1) silver_compile_result_destroy(r1);
        if (r2) silver_compile_result_destroy(r2);
        silver_context_destroy(ctx1);
        silver_context_destroy(ctx2);
        ir_module_destroy(m1);
        ir_module_destroy(m2);
        if (b1) ir_builder_destroy(b1);
        if (b2) ir_builder_destroy(b2);
    }
    
    // ================================================================
    // 第2节：整体效果总结
    // ================================================================
    printf("\n");
    printf("┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│  Section 2: Aggregate Optimization Effectiveness                 │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n");
    printf("\n");
    
    printf("  %-28s │ %8s │ %8s │ %8s │ %8s │ %8s\n",
           "Pass", "IR Δ%", "Code Δ%", "BranchΔ", "MemΔ", "Speed×");
    print_dash(85);
    
    double total_ir_before = 0, total_ir_after = 0;
    double total_code_before = 0, total_code_after = 0;
    double total_cycles_before = 0, total_cycles_after = 0;
    
    for (int i = 0; i < num_tests; i++) {
        double ir_d = ir_before[i] > 0 ? (1.0 - (double)opt[i].ir_after / ir_before[i]) * 100.0 : 0;
        double cd = noopt[i].code_size > 0 ? (1.0 - (double)opt[i].code_size / noopt[i].code_size) * 100.0 : 0;
        double sp = noopt[i].estimated_cycles > 0 ? noopt[i].estimated_cycles / opt[i].estimated_cycles : 1.0;
        
        printf("  %-28s │ %+7.1f%% │ %+7.1f%% │ %+7d │ %+7d │ %7.2f×\n",
               names[i], ir_d, cd,
               (int)opt[i].num_branches - (int)noopt[i].num_branches,
               (int)opt[i].num_memory_ops - (int)noopt[i].num_memory_ops,
               sp);
        
        total_ir_before += ir_before[i];
        total_ir_after += opt[i].ir_after;
        total_code_before += noopt[i].code_size;
        total_code_after += opt[i].code_size;
        total_cycles_before += noopt[i].estimated_cycles;
        total_cycles_after += opt[i].estimated_cycles;
    }
    
    print_dash(85);
    
    double total_ir_d = total_ir_before > 0 ? (1.0 - total_ir_after / total_ir_before) * 100.0 : 0;
    double total_cd = total_code_before > 0 ? (1.0 - total_code_after / total_code_before) * 100.0 : 0;
    double total_sp = total_cycles_after > 0 ? total_cycles_before / total_cycles_after : 1.0;
    
    printf("  %-28s │ %+7.1f%% │ %+7.1f%% │ %7s │ %7s │ %7.2f×\n",
           "TOTAL / AVERAGE", total_ir_d, total_cd, "—", "—", total_sp);
    
    // ================================================================
    // 第3节：代码密度排名
    // ================================================================
    printf("\n");
    printf("┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│  Section 3: Code Density Ranking (bytes per IR instruction)      │\n");
    printf("│  Lower is better — less machine code per unit of IR              │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n");
    printf("\n");
    
    printf("  %-28s │ %12s │ %12s │ %12s\n",
           "Pass", "Bytes/IR", "IR/Bytes", "Efficiency");
    print_dash(75);
    
    for (int i = 0; i < num_tests; i++) {
        double bpi = opt[i].ir_after > 0 ? (double)opt[i].code_size / opt[i].ir_after : 0;
        double ipb = opt[i].code_size > 0 ? (double)opt[i].ir_after / opt[i].code_size : 0;
        double eff = bpi > 0 ? (4.0 / bpi) * 100.0 : 0;  // 4=理想RISC
        
        printf("  %-28s │ %9.2f B  │ %9.4f    │ %8.1f%%\n",
               names[i], bpi, ipb, eff);
    }
    
    // ================================================================
    // 第4节：总结
    // ================================================================
    printf("\n");
    printf("┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│  Summary                                                         │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n");
    printf("\n");
    printf("  Overall optimization impact:\n");
    printf("    IR reduction:      %+.1f%%  (%d → %d instructions)\n",
           total_ir_d, (int)total_ir_before, (int)total_ir_after);
    printf("    Code reduction:    %+.1f%%  (%d → %d bytes)\n",
           total_cd, (int)total_code_before, (int)total_code_after);
    printf("    Speedup estimate:  %.2f×\n", total_sp);
    printf("\n");
    printf("  Most effective passes:\n");
    
    // 找最有效的三个
    typedef struct { const char *name; double score; } Rank;
    Rank ranks[5];
    for (int i = 0; i < num_tests; i++) {
        double cd = noopt[i].code_size > 0 ? (1.0 - (double)opt[i].code_size / noopt[i].code_size) * 100.0 : 0;
        ranks[i].name = names[i];
        ranks[i].score = cd;
    }
    // 排序
    for (int i = 0; i < num_tests - 1; i++) {
        for (int j = i + 1; j < num_tests; j++) {
            if (ranks[j].score > ranks[i].score) {
                Rank tmp = ranks[i]; ranks[i] = ranks[j]; ranks[j] = tmp;
            }
        }
    }
    for (int i = 0; i < 3; i++) {
        printf("    %d. %-30s %+.1f%% code reduction\n", i + 1, ranks[i].name, ranks[i].score);
    }
    
    printf("\n");
    printf("  ═══════════════════════════════════════════════════════════════\n");
    printf("  Code Quality Benchmark Complete\n");
    printf("  ═══════════════════════════════════════════════════════════════\n\n");
    
    return 0;
}