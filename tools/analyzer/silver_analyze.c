// tools/analyzer/silver_analyze.c - 性能分析工具
#include "silver/silver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
    #include <windows.h>
    static double get_time_ms(void) {
        return (double)GetTickCount64();
    }
#else
    #include <sys/time.h>
    static double get_time_ms(void) {
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
    }
#endif

// 分析编译结果中的指令分布
static void analyze_instruction_distribution(IRModule *module) {
    printf("\n=== IR Instruction Distribution ===\n");
    
    uint32_t op_counts[IR_OP_COUNT];
    memset(op_counts, 0, sizeof(op_counts));
    uint32_t total = 0;
    
    for (uint32_t f = 0; f < module->num_functions; f++) {
        IRFunction *func = &module->functions[f];
        for (uint32_t i = 0; i < func->num_insts; i++) {
            IROpcode op = func->instructions[i].opcode;
            if (op < IR_OP_COUNT) {
                op_counts[op]++;
                total++;
            }
        }
    }
    
    // 按频率排序打印
    printf("Total instructions: %u\n\n", total);
    printf("  %-20s  %8s  %8s\n", "Opcode", "Count", "Percent");
    printf("  %-20s  %8s  %8s\n", "------", "-----", "-------");
    
    // 简单排序打印前10
    for (int rank = 0; rank < 10; rank++) {
        uint32_t max_count = 0;
        IROpcode max_op = 0;
        
        for (int op = 0; op < IR_OP_COUNT; op++) {
            if (op_counts[op] > max_count) {
                max_count = op_counts[op];
                max_op = (IROpcode)op;
            }
        }
        
        if (max_count == 0) break;
        
        printf("  %-20s  %8u  %7.1f%%\n",
               ir_opcode_name(max_op), max_count,
               total > 0 ? (double)max_count / total * 100.0 : 0);
        
        op_counts[max_op] = 0;  // 清除以便找下一个
    }
}

// 分析优化效果
static void analyze_optimization_effect(IRModule *before, IRModule *after) {
    printf("\n=== Optimization Effect Analysis ===\n");
    
    uint32_t before_total = 0, after_total = 0;
    
    for (uint32_t f = 0; f < before->num_functions; f++) {
        before_total += before->functions[f].num_insts;
    }
    for (uint32_t f = 0; f < after->num_functions; f++) {
        after_total += after->functions[f].num_insts;
    }
    
    printf("  Before optimization: %u instructions\n", before_total);
    printf("  After optimization:  %u instructions\n", after_total);
    
    if (before_total > 0) {
        double reduction = (double)(before_total - after_total) / before_total * 100.0;
        printf("  Reduction:           %.1f%%\n", reduction);
    }
    
    // 按操作码分析变化
    printf("\n  Per-opcode changes:\n");
    printf("  %-20s  %8s  %8s  %8s\n", "Opcode", "Before", "After", "Change");
    printf("  %-20s  %8s  %8s  %8s\n", "------", "------", "-----", "------");
    
    for (int op = 0; op < IR_OP_COUNT; op++) {
        uint32_t before_count = 0, after_count = 0;
        
        for (uint32_t f = 0; f < before->num_functions; f++) {
            IRFunction *func = &before->functions[f];
            for (uint32_t i = 0; i < func->num_insts; i++) {
                if (func->instructions[i].opcode == (IROpcode)op) before_count++;
            }
        }
        for (uint32_t f = 0; f < after->num_functions; f++) {
            IRFunction *func = &after->functions[f];
            for (uint32_t i = 0; i < func->num_insts; i++) {
                if (func->instructions[i].opcode == (IROpcode)op) after_count++;
            }
        }
        
        if (before_count > 0 || after_count > 0) {
            int32_t change = (int32_t)after_count - (int32_t)before_count;
            printf("  %-20s  %8u  %8u  %+8d\n",
                   ir_opcode_name((IROpcode)op), before_count, after_count, change);
        }
    }
}

// 分析代码密度
static void analyze_code_density(const SilverCompileResult *result, IRModule *module) {
    printf("\n=== Code Density Analysis ===\n");
    
    uint32_t ir_count = module->stats.total_instructions;
    uint32_t code_size = result->code_size;
    
    printf("  IR instructions:     %u\n", ir_count);
    printf("  Machine code bytes:  %u\n", code_size);
    
    if (ir_count > 0) {
        printf("  Bytes per IR inst:   %.2f\n", (double)code_size / ir_count);
    }
    if (code_size > 0) {
        printf("  IR insts per byte:   %.4f\n", (double)ir_count / code_size);
    }
    
    // 估算与理想情况的比例
    // x86-64平均指令约3-5字节，RISC架构固定4字节
    printf("  Efficiency ratio:    %.1f%%\n",
           ir_count > 0 ? (4.0 / ((double)code_size / ir_count)) * 100.0 : 0);
}

int main(int argc, char **argv) {
    printf("Silver Performance Analyzer v%s\n\n", SILVER_VERSION_STRING);
    
    if (argc < 2) {
        printf("Usage: %s <ir_file>\n", argv[0]);
        printf("Analyzes IR and compilation performance\n");
        return 1;
    }
    
    // 创建IR模块（这里需要从文件加载，简化示例）
    IRModule *module = ir_module_create("analysis", "x86_64-unknown-none");
    if (!module) {
        fprintf(stderr, "Failed to create module\n");
        return 1;
    }
    
    // 分析优化前的指令分布
    analyze_instruction_distribution(module);
    
    // 复制模块用于对比
    IRModule *optimized = ir_module_create("optimized", "x86_64-unknown-none");
    // 这里应该复制IR，简化处理
    
    // 运行优化
    OptContext *opt_ctx = opt_context_create(module);
    opt_run_all(opt_ctx);
    opt_context_destroy(opt_ctx);
    
    // 分析优化后的指令分布
    printf("\n--- After Optimization ---");
    analyze_instruction_distribution(module);
    
    // 编译并分析代码密度
    SilverContext *ctx = silver_context_create();
    SilverCompileOptions options = silver_options_default();
    
    double start = get_time_ms();
    SilverCompileResult *result = silver_compile(ctx, module, &options);
    double elapsed = get_time_ms() - start;
    
    if (result) {
        printf("\n=== Compilation Performance ===\n");
        printf("  Compile time: %.2f ms\n", elapsed);
        printf("  Compile speed: %.0f IR insts/ms\n",
               module->stats.total_instructions / elapsed);
        
        analyze_code_density(result, module);
        
        silver_compile_result_destroy(result);
    }
    
    silver_context_destroy(ctx);
    ir_module_destroy(module);
    ir_module_destroy(optimized);
    
    return 0;
}