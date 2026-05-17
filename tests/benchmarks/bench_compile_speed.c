/**
 * Silver Compiler Backend — Compile Speed Benchmark
 * 
 * 测试编译流水线各阶段的吞吐量。
 * 覆盖场景：算术、控制流、函数调用、混合、空函数。
 * 测量精度：纳秒级，预热+多次平均。
 */

#include "silver/silver.h"
#include "silver/ir/ir_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef _WIN32
    #include <windows.h>
    static double get_time_ns(void) {
        LARGE_INTEGER f, c;
        QueryPerformanceFrequency(&f);
        QueryPerformanceCounter(&c);
        return (double)c.QuadPart * 1e9 / (double)f.QuadPart;
    }
#else
    #include <time.h>
    static double get_time_ns(void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
    }
#endif

// ============================================================
// 格式化辅助
// ============================================================
static const char *fmt_time(double ns) {
    static char b[32];
    if (ns < 1000.0)      snprintf(b, sizeof(b), "%6.0f ns", ns);
    else if (ns < 1e6)    snprintf(b, sizeof(b), "%6.2f µs", ns / 1e3);
    else if (ns < 1e9)    snprintf(b, sizeof(b), "%6.2f ms", ns / 1e6);
    else                  snprintf(b, sizeof(b), "%6.2f  s", ns / 1e9);
    return b;
}

static const char *fmt_rate(double per_s) {
    static char b[32];
    if (per_s >= 1e9)       snprintf(b, sizeof(b), "%.2f G/s", per_s / 1e9);
    else if (per_s >= 1e6)  snprintf(b, sizeof(b), "%.1f M/s", per_s / 1e6);
    else if (per_s >= 1e3)  snprintf(b, sizeof(b), "%.1f K/s", per_s / 1e3);
    else                    snprintf(b, sizeof(b), "%.0f /s", per_s);
    return b;
}

static const char *fmt_bytes(double bps) {
    static char buf[32];
    if (bps >= 1e9)       snprintf(buf, sizeof(buf), "%.2f GB/s", bps / 1e9);
    else if (bps >= 1e6)  snprintf(buf, sizeof(buf), "%.1f MB/s", bps / 1e6);
    else if (bps >= 1e3)  snprintf(buf, sizeof(buf), "%.1f KB/s", bps / 1e3);
    else                  snprintf(buf, sizeof(buf), "%.0f B/s", bps);
    return buf;
}

static void hr(int w, char c) { for (int i = 0; i < w; i++) putchar(c); putchar('\n'); }

// ============================================================
// 测试模块生成器
// ============================================================
typedef IRModule *(*ModuleGen)(int complexity, IRBuilder **out_builder, int *out_ir_count);

// 场景1: 纯算术链（可优化链）
static IRModule *gen_arithmetic_chain(int n, IRBuilder **b, int *count) {
    IRModule *m = ir_module_create("arith_chain", "x86_64-unknown-none");
    *b = ir_builder_create(m);
    IRFunction *f = ir_module_add_function(m, "f", 0);
    ir_builder_set_position(*b, f, NULL);
    IRBlock *e = ir_builder_create_block(*b, "e");
    ir_builder_set_insert_point(*b, e);
    IRValueId r = ir_builder_int_const(*b, (*b)->i64_type, 0);
    for (int i = 0; i < n; i++) {
        IRValueId v = ir_builder_int_const(*b, (*b)->i64_type, i);
        switch (i % 6) {
            case 0: r = ir_builder_add(*b, r, v); break;
            case 1: r = ir_builder_sub(*b, r, v); break;
            case 2: r = ir_builder_mul(*b, r, v); break;
            case 3: r = ir_builder_and(*b, r, v); break;
            case 4: r = ir_builder_or(*b, r, v);  break;
            case 5: r = ir_builder_xor(*b, r, v); break;
        }
    }
    ir_builder_ret(*b, r);
    *count = m->stats.total_instructions;
    return m;
}

// 场景2: 纯算术独立（每条独立，不可优化链）
static IRModule *gen_arithmetic_independent(int n, IRBuilder **b, int *count) {
    IRModule *m = ir_module_create("arith_indep", "x86_64-unknown-none");
    *b = ir_builder_create(m);
    IRFunction *f = ir_module_add_function(m, "f", 0);
    ir_builder_set_position(*b, f, NULL);
    IRBlock *e = ir_builder_create_block(*b, "e");
    ir_builder_set_insert_point(*b, e);
    IRValueId sum = ir_builder_int_const(*b, (*b)->i64_type, 0);
    for (int i = 0; i < n; i++) {
        IRValueId a0 = ir_builder_int_const(*b, (*b)->i64_type, i);
        IRValueId b0 = ir_builder_int_const(*b, (*b)->i64_type, i * 3 + 7);
        IRValueId x = ir_builder_mul(*b, a0, b0);
        sum = ir_builder_add(*b, sum, x);
    }
    ir_builder_ret(*b, sum);
    *count = m->stats.total_instructions;
    return m;
}

// 场景3: 控制流（分支+基本块）
static IRModule *gen_control_flow(int n, IRBuilder **b, int *count) {
    IRModule *m = ir_module_create("control_flow", "x86_64-unknown-none");
    *b = ir_builder_create(m);
    IRFunction *f = ir_module_add_function(m, "f", 0);
    ir_builder_set_position(*b, f, NULL);
    IRBlock *e = ir_builder_create_block(*b, "e");
    ir_builder_set_insert_point(*b, e);
    
    int nblocks = n < 2 ? 2 : (n > 20 ? 20 : n);
    IRBlock **blks = (IRBlock **)malloc(nblocks * sizeof(IRBlock *));
    for (int i = 0; i < nblocks; i++) {
        char nm[16]; snprintf(nm, sizeof(nm), "b%d", i);
        blks[i] = ir_builder_create_block(*b, nm);
    }
    ir_builder_br(*b, blks[0]);
    
    IRValueId acc = ir_builder_int_const(*b, (*b)->i64_type, 0);
    for (int i = 0; i < nblocks - 1; i++) {
        ir_builder_set_insert_point(*b, blks[i]);
        IRValueId v = ir_builder_int_const(*b, (*b)->i64_type, i);
        acc = ir_builder_add(*b, acc, v);
        IRValueId cond = ir_builder_icmp(*b, IR_OP_CMPLT, acc,
            ir_builder_int_const(*b, (*b)->i64_type, nblocks * 2));
        ir_builder_condbr(*b, cond, blks[i + 1], blks[nblocks - 1]);
    }
    ir_builder_set_insert_point(*b, blks[nblocks - 1]);
    ir_builder_ret(*b, acc);
    free(blks);
    *count = m->stats.total_instructions;
    return m;
}

// 场景4: 函数调用
static IRModule *gen_function_calls(int n, IRBuilder **b, int *count) {
    IRModule *m = ir_module_create("func_calls", "x86_64-unknown-none");
    *b = ir_builder_create(m);
    
    IRFunction *callee = ir_module_add_function(m, "callee", 0);
    ir_builder_set_position(*b, callee, NULL);
    IRBlock *ce = ir_builder_create_block(*b, "e");
    ir_builder_set_insert_point(*b, ce);
    IRValueId p = ir_builder_int_const(*b, (*b)->i64_type, 0);
    ir_builder_ret(*b, ir_builder_add(*b, p, p));
    
    IRFunction *caller = ir_module_add_function(m, "caller", 0);
    ir_builder_set_position(*b, caller, NULL);
    IRBlock *e = ir_builder_create_block(*b, "e");
    ir_builder_set_insert_point(*b, e);
    IRValueId callee_v = ir_value_create_function(&m->value_pool, 0, "callee");
    IRValueId total = ir_builder_int_const(*b, (*b)->i64_type, 0);
    int nc = n < 1 ? 1 : (n > 50 ? 50 : n);
    for (int i = 0; i < nc; i++) {
        IRValueId arg = ir_builder_int_const(*b, (*b)->i64_type, i);
        IRValueId ret = ir_builder_call(*b, (*b)->i64_type, callee_v, 1, arg);
        total = ir_builder_add(*b, total, ret);
    }
    ir_builder_ret(*b, total);
    *count = m->stats.total_instructions;
    return m;
}

// 场景5: 空函数
static IRModule *gen_empty(int n, IRBuilder **b, int *count) {
    (void)n;
    IRModule *m = ir_module_create("empty", "x86_64-unknown-none");
    *b = ir_builder_create(m);
    IRFunction *f = ir_module_add_function(m, "f", 0);
    ir_builder_set_position(*b, f, NULL);
    IRBlock *e = ir_builder_create_block(*b, "e");
    ir_builder_set_insert_point(*b, e);
    ir_builder_ret(*b, ir_builder_int_const(*b, (*b)->i64_type, 42));
    *count = m->stats.total_instructions;
    return m;
}

// ============================================================
// 单次编译计时
// ============================================================
typedef struct {
    double build_ns;
    double opt_ns;
    double codegen_ns;
    double total_ns;
    uint32_t ir_count;
    uint32_t code_size;
} CompileTiming;

static CompileTiming time_compile(ModuleGen gen, int complexity) {
    CompileTiming ct;
    memset(&ct, 0, sizeof(ct));
    
    IRBuilder *builder = NULL;
    int ir_count = 0;
    
    double t0 = get_time_ns();
    IRModule *module = gen(complexity, &builder, &ir_count);
    double t1 = get_time_ns();
    ct.build_ns = t1 - t0;
    ct.ir_count = (uint32_t)ir_count;
    
    double t2 = get_time_ns();
    OptContext *opt = opt_context_create(module);
    opt_run_all(opt);
    opt_context_destroy(opt);
    double t3 = get_time_ns();
    ct.opt_ns = t3 - t2;
    
    SilverContext *ctx = silver_context_create();
    SilverCompileOptions opts = silver_options_default();
    opts.target_arch = SILVER_TARGET_X86_64;
    opts.verbose = false;
    
    double t4 = get_time_ns();
    SilverCompileResult *res = silver_compile(ctx, module, &opts);
    double t5 = get_time_ns();
    ct.codegen_ns = t5 - t4;
    
    if (res) {
        ct.code_size = res->code_size;
        silver_compile_result_destroy(res);
    }
    ct.total_ns = t5 - t0;
    
    silver_context_destroy(ctx);
    ir_builder_destroy(builder);
    ir_module_destroy(module);
    return ct;
}

// ============================================================
// 多次测量取平均
// ============================================================
static CompileTiming bench(ModuleGen gen, int complexity, int warmup, int runs) {
    for (int i = 0; i < warmup; i++) time_compile(gen, complexity);
    
    CompileTiming sum;
    memset(&sum, 0, sizeof(sum));
    for (int i = 0; i < runs; i++) {
        CompileTiming ct = time_compile(gen, complexity);
        sum.build_ns += ct.build_ns;
        sum.opt_ns += ct.opt_ns;
        sum.codegen_ns += ct.codegen_ns;
        sum.total_ns += ct.total_ns;
        sum.ir_count = ct.ir_count;
        sum.code_size = ct.code_size;
    }
    sum.build_ns /= runs;
    sum.opt_ns /= runs;
    sum.codegen_ns /= runs;
    sum.total_ns /= runs;
    return sum;
}

// ============================================================
// 主函数
// ============================================================
int main(void) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════════╗\n");
    printf("║         Silver Compiler Backend — Compile Speed Benchmark            ║\n");
    printf("║                         v%s                              ║\n", SILVER_VERSION_STRING);
    printf("╚══════════════════════════════════════════════════════════════════════╝\n");
    
    // ============================================================
    // Part 1: 各场景固定规模测试
    // ============================================================
    printf("\n┌──────────────────────────────────────────────────────────────────────┐\n");
    printf("│  Part 1: Per-Scenario Breakdown (1000 IR instructions each)          │\n");
    printf("│  5 scenarios × 3 sizes, 10 warmup + 100 runs each                   │\n");
    printf("└──────────────────────────────────────────────────────────────────────┘\n\n");
    
    typedef struct { ModuleGen gen; const char *name; const char *desc; } Scenario;
    Scenario scenarios[] = {
        {gen_arithmetic_chain,      "Arith Chain",    "可优化的常量链"},
        {gen_arithmetic_independent, "Arith Indep",    "独立计算（不可优化链）"},
        {gen_control_flow,          "Control Flow",    "多基本块分支"},
        {gen_function_calls,        "Function Calls",  "跨函数调用"},
        {gen_empty,                 "Empty/Minimal",   "最小函数"},
    };
    int num_scenarios = 5;
    int sizes[] = {100, 500, 1000};
    int num_sizes = 3;
    
    for (int s = 0; s < num_scenarios; s++) {
        printf("  ▸ %s — %s\n", scenarios[s].name, scenarios[s].desc);
        printf("    %8s │ %10s │ %10s │ %10s │ %10s │ %8s │ %12s\n",
               "IR Inst", "IR Build", "Optimize", "Codegen", "Total", "Inst/ms", "Code Rate");
        hr(95, '-');
        
        for (int z = 0; z < num_sizes; z++) {
            CompileTiming ct = bench(scenarios[s].gen, sizes[z], 10, 100);
            double inst_per_ms = ct.total_ns > 0 ? ct.ir_count / (ct.total_ns / 1e6) : 0;
            double bytes_per_s = ct.total_ns > 0 ? ct.code_size / (ct.total_ns / 1e9) : 0;
            
            printf("    %8u │ %10s │ %10s │ %10s │ %10s │ %8.0f │ %12s\n",
                   ct.ir_count, fmt_time(ct.build_ns), fmt_time(ct.opt_ns),
                   fmt_time(ct.codegen_ns), fmt_time(ct.total_ns),
                   inst_per_ms, fmt_bytes(bytes_per_s));
        }
        printf("\n");
    }
    
    // ============================================================
    // Part 2: 吞吐量 vs IR 规模（可优化链）
    // ============================================================
    printf("┌──────────────────────────────────────────────────────────────────────┐\n");
    printf("│  Part 2: Throughput vs IR Size (Arith Chain, 10 warmup + 200 runs)   │\n");
    printf("└──────────────────────────────────────────────────────────────────────┘\n\n");
    
    int scale_sizes[] = {10, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
    int num_scale = sizeof(scale_sizes) / sizeof(scale_sizes[0]);
    
    printf("    %8s │ %10s │ %12s │ %12s │ %12s │ %8s\n",
           "IR Size", "Time", "IR/s", "Code Bytes", "Bytes/s", "Code Sz");
    hr(90, '-');
    
    for (int i = 0; i < num_scale; i++) {
        CompileTiming ct = bench(gen_arithmetic_chain, scale_sizes[i], 10, 200);
        double ir_per_s = ct.total_ns > 0 ? ct.ir_count / (ct.total_ns / 1e9) : 0;
        double bytes_per_s = ct.total_ns > 0 ? ct.code_size / (ct.total_ns / 1e9) : 0;
        
        printf("    %8u │ %10s │ %12s │ %12u │ %12s │ %6u B\n",
               ct.ir_count, fmt_time(ct.total_ns), fmt_rate(ir_per_s),
               ct.ir_count, fmt_bytes(bytes_per_s), ct.code_size);
    }
    
    // ============================================================
    // Part 3: 跨平台对比
    // ============================================================
    printf("\n┌──────────────────────────────────────────────────────────────────────┐\n");
    printf("│  Part 3: Cross-Platform (1000 IR Arith Chain, 10 warmup + 200 runs) │\n");
    printf("└──────────────────────────────────────────────────────────────────────┘\n\n");
    
    SilverTargetArch archs[] = {SILVER_TARGET_X86_64, SILVER_TARGET_ARM64, SILVER_TARGET_RISCV64};
    const char *anames[] = {"x86-64", "ARM64 (AArch64)", "RISC-V64 (RV64GC)"};
    
    printf("    %-22s │ %10s │ %10s │ %10s │ %10s │ %8s\n",
           "Target", "Total", "IR Build", "Optimize", "Codegen", "Code Size");
    hr(90, '-');
    
    for (int a = 0; a < 3; a++) {
        // 为每个目标生成
        IRBuilder *b = NULL; int ic = 0;
        IRModule *m = gen_arithmetic_chain(1000, &b, &ic);
        
        double t0 = get_time_ns();
        OptContext *opt = opt_context_create(m);
        opt_run_all(opt);
        opt_context_destroy(opt);
        
        SilverContext *ctx = silver_context_create();
        SilverCompileOptions opts = silver_options_default();
        opts.target_arch = archs[a];
        
        double t1 = get_time_ns();
        SilverCompileResult *res = silver_compile(ctx, m, &opts);
        double t2 = get_time_ns();
        
        uint32_t cs = res ? res->code_size : 0;
        if (res) silver_compile_result_destroy(res);
        
        printf("    %-22s │ %10s │ %10s │ %10s │ %10s │ %8u B\n",
               anames[a], fmt_time(t2 - t0), fmt_time(t0 - t0),  // 简化：仅总时间
               fmt_time(0.0), fmt_time(t2 - t1), cs);
        
        silver_context_destroy(ctx);
        ir_builder_destroy(b);
        ir_module_destroy(m);
    }
    
    // ============================================================
    // Part 4: 总结
    // ============================================================
    printf("\n┌──────────────────────────────────────────────────────────────────────┐\n");
    printf("│  Summary                                                             │\n");
    printf("└──────────────────────────────────────────────────────────────────────┘\n\n");
    
    CompileTiming avg = bench(gen_arithmetic_chain, 1000, 20, 500);
    double ir_per_s = avg.total_ns > 0 ? avg.ir_count / (avg.total_ns / 1e9) : 0;
    double bytes_per_s = avg.total_ns > 0 ? avg.code_size / (avg.total_ns / 1e9) : 0;
    
    printf("  Benchmark: Arith Chain, 1000 IR instructions\n");
    printf("  Runs: 20 warmup + 500 measured\n");
    printf("  ─────────────────────────────────────────\n");
    printf("  IR Build:     %s  (%4.1f%%)\n", fmt_time(avg.build_ns),
           100.0 * avg.build_ns / avg.total_ns);
    printf("  Optimize:     %s  (%4.1f%%)\n", fmt_time(avg.opt_ns),
           100.0 * avg.opt_ns / avg.total_ns);
    printf("  Codegen:      %s  (%4.1f%%)\n", fmt_time(avg.codegen_ns),
           100.0 * avg.codegen_ns / avg.total_ns);
    printf("  ─────────────────────────────────────────\n");
    printf("  Total:        %s\n", fmt_time(avg.total_ns));
    printf("  Throughput:   %s IR/s\n", fmt_rate(ir_per_s));
    printf("  Code Output:  %s/s\n", fmt_bytes(bytes_per_s));
    printf("  Code Size:    %u bytes\n", avg.code_size);
    printf("\n  IR→Code Ratio: %.2f bytes per IR instruction\n",
           avg.ir_count > 0 ? (double)avg.code_size / avg.ir_count : 0);
    
    printf("\n  ═══════════════════════════════════════════════════════════════════\n");
    printf("  Compile Speed Benchmark Complete\n");
    printf("  ═══════════════════════════════════════════════════════════════════\n\n");
    
    return 0;
}