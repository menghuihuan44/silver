#include "silver/silver.h"
#include "silver/ir/ir_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#ifdef _WIN32
    #include <windows.h>
    static double get_time_ns(void) {
        LARGE_INTEGER freq, count;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&count);
        return (double)count.QuadPart * 1e9 / (double)freq.QuadPart;
    }
#else
    #include <sys/time.h>
    static double get_time_ns(void) {
        struct timespec ts;
        clock_gettime(CLOCK_MONOTONIC, &ts);
        return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
    }
#endif

// ============================================================
// 计时辅助
// ============================================================
static const char *time_format(double ns) {
    static char buf[32];
    if (ns < 1000.0)       snprintf(buf, sizeof(buf), "%6.0f ns", ns);
    else if (ns < 1e6)     snprintf(buf, sizeof(buf), "%6.2f µs", ns / 1000.0);
    else if (ns < 1e9)     snprintf(buf, sizeof(buf), "%6.2f ms", ns / 1e6);
    else                   snprintf(buf, sizeof(buf), "%6.2f  s", ns / 1e9);
    return buf;
}

static const char *rate_format(double per_second) {
    static char buf[32];
    if (per_second >= 1e9)       snprintf(buf, sizeof(buf), "%.2f G/s", per_second / 1e9);
    else if (per_second >= 1e6)  snprintf(buf, sizeof(buf), "%.1f M/s", per_second / 1e6);
    else if (per_second >= 1e3)  snprintf(buf, sizeof(buf), "%.1f K/s", per_second / 1e3);
    else                         snprintf(buf, sizeof(buf), "%.0f /s", per_second);
    return buf;
}

static const char *byte_rate_format(double bytes_per_second) {
    static char buf[32];
    if (bytes_per_second >= 1e9)       snprintf(buf, sizeof(buf), "%.2f GB/s", bytes_per_second / 1e9);
    else if (bytes_per_second >= 1e6)  snprintf(buf, sizeof(buf), "%.1f MB/s", bytes_per_second / 1e6);
    else if (bytes_per_second >= 1e3)  snprintf(buf, sizeof(buf), "%.1f KB/s", bytes_per_second / 1e3);
    else                               snprintf(buf, sizeof(buf), "%.0f B/s", bytes_per_second);
    return buf;
}

static void print_dash(int w) { for (int i = 0; i < w; i++) putchar('-'); putchar('\n'); }

// ============================================================
// 基准上下文（各阶段时间累积）
// ============================================================
typedef struct {
    IRModule *module;
    IRBuilder *builder;
    SilverContext *ctx;
    SilverCompileOptions options;
    double ir_build_ns;
    double opt_ns;
    double codegen_ns;
    uint32_t ir_count;
    uint32_t code_size;
} BenchContext;

static BenchContext *bench_ctx_create(SilverTargetArch arch) {
    BenchContext *bc = (BenchContext *)calloc(1, sizeof(BenchContext));
    if (!bc) return NULL;
    bc->ctx = silver_context_create();
    bc->options = silver_options_default();
    bc->options.target_arch = arch;
    bc->options.verbose = false;
    return bc;
}

static void bench_ctx_reset(BenchContext *bc) {
    if (bc->module) { ir_module_destroy(bc->module); bc->module = NULL; }
    if (bc->builder) { ir_builder_destroy(bc->builder); bc->builder = NULL; }
    bc->ir_build_ns = 0;
    bc->opt_ns = 0;
    bc->codegen_ns = 0;
}

static void bench_ctx_destroy(BenchContext *bc) {
    if (!bc) return;
    bench_ctx_reset(bc);
    if (bc->ctx) silver_context_destroy(bc->ctx);
    free(bc);
}

// ============================================================
// 测试模块生成器
// ============================================================
static IRModule *gen_arithmetic(BenchContext *bc, int num_ops) {
    bc->module = ir_module_create("bench_arith", "x86_64-unknown-none");
    if (!bc->module) return NULL;
    bc->builder = ir_builder_create(bc->module);
    if (!bc->builder) return NULL;
    
    IRFunction *func = ir_module_add_function(bc->module, "arith_test", 0);
    ir_builder_set_position(bc->builder, func, NULL);
    IRBlock *entry = ir_builder_create_block(bc->builder, "entry");
    ir_builder_set_insert_point(bc->builder, entry);
    
    IRValueId result = ir_builder_int_const(bc->builder, bc->builder->i64_type, 0);
    for (int i = 0; i < num_ops; i++) {
        IRValueId val = ir_builder_int_const(bc->builder, bc->builder->i64_type, i);
        switch (i % 6) {
            case 0: result = ir_builder_add(bc->builder, result, val); break;
            case 1: result = ir_builder_sub(bc->builder, result, val); break;
            case 2: result = ir_builder_mul(bc->builder, result, val); break;
            case 3: result = ir_builder_and(bc->builder, result, val); break;
            case 4: result = ir_builder_or(bc->builder, result, val); break;
            case 5: result = ir_builder_xor(bc->builder, result, val); break;
        }
    }
    ir_builder_ret(bc->builder, result);
    bc->ir_count = bc->module->stats.total_instructions;
    return bc->module;
}

// ============================================================
// 高精度测量：返回总时间，同时累积各阶段时间到 bc
// ============================================================
static double bench_full_pipeline(int complexity, void *ctx) {
    BenchContext *bc = (BenchContext *)ctx;
    bench_ctx_reset(bc);
    
    // IR 构建
    double t0 = get_time_ns();
    gen_arithmetic(bc, complexity);
    double t1 = get_time_ns();
    bc->ir_build_ns = t1 - t0;
    
    // 优化
    double t2 = get_time_ns();
    if (bc->options.optimize) {
        OptContext *opt_ctx = opt_context_create(bc->module);
        opt_run_all(opt_ctx);
        opt_context_destroy(opt_ctx);
    }
    double t3 = get_time_ns();
    bc->opt_ns = t3 - t2;
    
    // 代码生成
    double t4 = get_time_ns();
    SilverCompileResult *result = silver_compile(bc->ctx, bc->module, &bc->options);
    double t5 = get_time_ns();
    bc->codegen_ns = t5 - t4;
    
    if (result) {
        bc->code_size = result->code_size;
        silver_compile_result_destroy(result);
    }
    
    return t5 - t0;
}

// ============================================================
// 测量：预热 + 多次运行，累积各阶段平均时间
// ============================================================
typedef struct {
    double total_ns;
    double ir_build_ns;
    double opt_ns;
    double codegen_ns;
    int runs;
} MeasureResult;

static MeasureResult measure_with_warmup(int complexity, BenchContext *bc,
                                          int warmup, int min_runs, double min_time_ms) {
    MeasureResult mr;
    memset(&mr, 0, sizeof(mr));
    
    // 预热
    for (int i = 0; i < warmup; i++) {
        bench_full_pipeline(complexity, bc);
    }
    
    // 正式测量
    double start = get_time_ns();
    while (mr.runs < min_runs || (get_time_ns() - start) / 1e6 < min_time_ms) {
        if (mr.runs >= 1000) break;  // 安全上限
        
        double single = bench_full_pipeline(complexity, bc);
        mr.total_ns += single;
        mr.ir_build_ns += bc->ir_build_ns;
        mr.opt_ns += bc->opt_ns;
        mr.codegen_ns += bc->codegen_ns;
        mr.runs++;
    }
    
    // 平均
    if (mr.runs > 0) {
        mr.total_ns /= mr.runs;
        mr.ir_build_ns /= mr.runs;
        mr.opt_ns /= mr.runs;
        mr.codegen_ns /= mr.runs;
    }
    
    return mr;
}

// ============================================================
// 主函数
// ============================================================
int main(int argc, char **argv) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════════╗\n");
    printf("║        Silver Compiler Backend — Compile Speed Benchmarks        ║\n");
    printf("║                      v%s                          ║\n", SILVER_VERSION_STRING);
    printf("╚══════════════════════════════════════════════════════════════════╝\n");
    
    // ================================================================
    // Section 1: 各阶段耗时分解
    // ================================================================
    printf("\n┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│  Section 1: Pipeline Stage Breakdown                             │\n");
    printf("│  Average time per stage at various IR sizes (x86-64, with opt)   │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n\n");
    
    int sizes[] = {10, 100, 500, 1000, 2000, 5000, 10000};
    int num_sizes = sizeof(sizes) / sizeof(sizes[0]);
    
    printf("  %8s │ %10s │ %10s │ %10s │ %10s │ %10s │ %12s\n",
           "IR Inst", "IR Build", "Optimize", "Codegen", "Total", "Inst/ms", "Code Rate");
    print_dash(95);
    
    for (int i = 0; i < num_sizes; i++) {
        BenchContext *bc = bench_ctx_create(SILVER_TARGET_X86_64);
        bc->options.optimize = true;
        
        MeasureResult mr = measure_with_warmup(sizes[i], bc, 2, 5, 50.0);
        
        double total_ms = mr.total_ns / 1e6;
        double inst_per_ms = total_ms > 0 ? bc->ir_count / total_ms : 0;
        double bytes_per_s = mr.total_ns > 0 ? bc->code_size / (mr.total_ns / 1e9) : 0;
        
        printf("  %8u │ %10s │ %10s │ %10s │ %10s │ %10.0f │ %12s\n",
               bc->ir_count,
               time_format(mr.ir_build_ns), time_format(mr.opt_ns),
               time_format(mr.codegen_ns), time_format(mr.total_ns),
               inst_per_ms, byte_rate_format(bytes_per_s));
        
        bench_ctx_destroy(bc);
    }
    
    // ================================================================
    // Section 2: 扩展性
    // ================================================================
    printf("\n┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│  Section 2: Scalability — Throughput vs IR Size                  │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n\n");
    
    int scale_sizes[] = {50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000};
    int num_scale = sizeof(scale_sizes) / sizeof(scale_sizes[0]);
    
    printf("  %8s │ %10s │ %15s │ %15s │ %10s\n",
           "IR Size", "Total Time", "IR Inst/sec", "Code Bytes/sec", "Efficiency");
    print_dash(85);
    
    double baseline_rate = 0;
    
    for (int i = 0; i < num_scale; i++) {
        BenchContext *bc = bench_ctx_create(SILVER_TARGET_X86_64);
        bc->options.optimize = true;
        
        MeasureResult mr = measure_with_warmup(scale_sizes[i], bc, 2, 5, 100.0);
        
        double total_s = mr.total_ns / 1e9;
        double ir_per_s = total_s > 0 ? bc->ir_count / total_s : 0;
        double bytes_per_s = total_s > 0 ? bc->code_size / total_s : 0;
        
        if (i == 0) baseline_rate = ir_per_s;
        double efficiency = baseline_rate > 0 ? (ir_per_s / baseline_rate) * 100.0 : 100.0;
        
        printf("  %8u │ %10s │ %12s IR/s │ %12s │ %9.1f%%\n",
               bc->ir_count, time_format(mr.total_ns),
               rate_format(ir_per_s), byte_rate_format(bytes_per_s), efficiency);
        
        bench_ctx_destroy(bc);
    }
    
    // ================================================================
    // Section 3: 跨平台对比
    // ================================================================
    printf("\n┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│  Section 3: Cross-Platform — Same IR, Three Targets              │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n\n");
    
    SilverTargetArch archs[] = {SILVER_TARGET_X86_64, SILVER_TARGET_ARM64, SILVER_TARGET_RISCV64};
    const char *arch_names[] = {"x86-64", "ARM64 (AArch64)", "RISC-V64 (RV64GC)"};
    int cross_sizes[] = {100, 1000};
    
    for (int s = 0; s < 2; s++) {
        printf("  Module size: %d IR instructions\n\n", cross_sizes[s]);
        printf("  %-22s │ %10s │ %10s │ %10s │ %10s │ %8s\n",
               "Target", "Total", "IR Build", "Optimize", "Codegen", "Code Size");
        print_dash(95);
        
        for (int a = 0; a < 3; a++) {
            BenchContext *bc = bench_ctx_create(archs[a]);
            bc->options.target_arch = archs[a];
            bc->options.optimize = true;
            
            MeasureResult mr = measure_with_warmup(cross_sizes[s], bc, 2, 5, 100.0);
            
            printf("  %-22s │ %10s │ %10s │ %10s │ %10s │ %8u B\n",
                   arch_names[a],
                   time_format(mr.total_ns), time_format(mr.ir_build_ns),
                   time_format(mr.opt_ns), time_format(mr.codegen_ns),
                   bc->code_size);
            
            bench_ctx_destroy(bc);
        }
        printf("\n");
    }
    
    // ================================================================
    // Section 4: 优化开销
    // ================================================================
    printf("┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│  Section 4: Optimization Impact — Opt On vs Off                  │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n\n");
    
    int opt_sizes[] = {100, 500, 1000, 2000, 5000, 10000};
    int num_opt = sizeof(opt_sizes) / sizeof(opt_sizes[0]);
    
    printf("  %8s │ %12s │ %12s │ %10s │ %10s │ %15s\n",
           "IR Size", "No Opt", "With Opt", "Δ Time", "Δ Size", "Opt Throughput");
    print_dash(95);
    
    for (int i = 0; i < num_opt; i++) {
        // 不优化
        BenchContext *bc1 = bench_ctx_create(SILVER_TARGET_X86_64);
        bc1->options.optimize = false;
        MeasureResult mr1 = measure_with_warmup(opt_sizes[i], bc1, 2, 5, 50.0);
        uint32_t noopt_size = bc1->code_size;
        bench_ctx_destroy(bc1);
        
        // 优化
        BenchContext *bc2 = bench_ctx_create(SILVER_TARGET_X86_64);
        bc2->options.optimize = true;
        MeasureResult mr2 = measure_with_warmup(opt_sizes[i], bc2, 2, 5, 50.0);
        uint32_t opt_size = bc2->code_size;
        uint32_t ir_count = bc2->ir_count;
        bench_ctx_destroy(bc2);
        
        double time_diff = mr1.total_ns > 0 ? ((mr2.total_ns - mr1.total_ns) / mr1.total_ns) * 100.0 : 0;
        double size_diff = noopt_size > 0 ? (1.0 - (double)opt_size / noopt_size) * 100.0 : 0;
        double opt_ir_per_s = mr2.total_ns > 0 ? ir_count / (mr2.total_ns / 1e9) : 0;
        
        printf("  %8u │ %12s │ %12s │ %+9.1f%% │ %+9.1f%% │ %12s IR/s\n",
               ir_count,
               time_format(mr1.total_ns), time_format(mr2.total_ns),
               time_diff, size_diff, rate_format(opt_ir_per_s));
    }
    
    // ================================================================
    // Section 5: 总结
    // ================================================================
    printf("\n┌──────────────────────────────────────────────────────────────────┐\n");
    printf("│  Summary                                                         │\n");
    printf("└──────────────────────────────────────────────────────────────────┘\n\n");
    
    BenchContext *bc = bench_ctx_create(SILVER_TARGET_X86_64);
    bc->options.optimize = true;
    MeasureResult mr = measure_with_warmup(1000, bc, 5, 10, 200.0);
    
    double ir_per_s = bc->ir_count / (mr.total_ns / 1e9);
    double bytes_per_s = bc->code_size / (mr.total_ns / 1e9);
    
    printf("  Configuration: x86-64, %u IR insts, optimizer enabled\n", bc->ir_count);
    printf("  Average of %d runs:\n", mr.runs);
    printf("    Total:         %s\n", time_format(mr.total_ns));
    printf("    IR Build:      %s\n", time_format(mr.ir_build_ns));
    printf("    Optimize:      %s\n", time_format(mr.opt_ns));
    printf("    Codegen:       %s\n", time_format(mr.codegen_ns));
    printf("    Throughput:    %s IR/s\n", rate_format(ir_per_s));
    printf("    Output:        %s, %s/s\n",
           byte_rate_format(bc->code_size), byte_rate_format(bytes_per_s));
    
    bench_ctx_destroy(bc);
    
    printf("\n  ═══════════════════════════════════════════════════════════════\n");
    printf("  Compile Speed Benchmark Complete\n");
    printf("  ═══════════════════════════════════════════════════════════════\n\n");
    
    return 0;
}