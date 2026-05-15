#include "silver/silver.h"
#include "silver/ir/ir_builder.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// 打印使用说明
static void print_usage(const char *program) {
    printf("Silver Compiler Backend v%s\n", SILVER_VERSION_STRING);
    printf("Usage: %s [options] <input>\n\n", program);
    printf("Options:\n");
    printf("  -o <file>          Output file (default: output.o)\n");
    printf("  -target <arch>     Target architecture:\n");
    printf("                       x86-64, arm64, riscv64\n");
    printf("  -triple <triple>   Target triple\n");
    printf("  -S                 Generate assembly (not yet implemented)\n");
    printf("  -g                 Generate debug info\n");
    printf("  -fpic              Generate position-independent code\n");
    printf("  -v, --verbose      Verbose output\n");
    printf("  -h, --help         Print this help\n");
    printf("  --version          Print version\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s -target x86-64 -o output.o input.ir\n", program);
    printf("  %s -target arm64 -g -o output.o input.ir\n", program);
}

// 解析命令行参数
static bool parse_options(int argc, char **argv, SilverCompileOptions *options) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            options->output_file = argv[++i];
        } else if (strcmp(argv[i], "-target") == 0 && i + 1 < argc) {
            const char *arch = argv[++i];
            if (strcmp(arch, "x86-64") == 0) {
                options->target_arch = SILVER_TARGET_X86_64;
            } else if (strcmp(arch, "arm64") == 0) {
                options->target_arch = SILVER_TARGET_ARM64;
            } else if (strcmp(arch, "riscv64") == 0) {
                options->target_arch = SILVER_TARGET_RISCV64;
            } else {
                fprintf(stderr, "Unknown target architecture: %s\n", arch);
                return false;
            }
        } else if (strcmp(argv[i], "-triple") == 0 && i + 1 < argc) {
            options->target_triple = argv[++i];
            options->target_arch = silver_target_from_triple(options->target_triple);
        } else if (strcmp(argv[i], "-S") == 0) {
            options->output_format = SILVER_OUTPUT_ASM;
        } else if (strcmp(argv[i], "-g") == 0) {
            options->debug_info = true;
        } else if (strcmp(argv[i], "-fpic") == 0) {
            options->pic = true;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            options->verbose = true;
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(0);
        } else if (strcmp(argv[i], "--version") == 0) {
            printf("Silver Compiler Backend v%s\n", SILVER_VERSION_STRING);
            exit(0);
        } else if (argv[i][0] == '-') {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return false;
        } else {
            options->input_file = argv[i];
        }
    }
    
    return true;
}

// 简单的IR构建示例（用于测试）
static IRModule *create_test_module(SilverContext *ctx) {
    IRModule *module = ir_module_create("test", "x86_64-unknown-none");
    if (!module) return NULL;
    
    IRBuilder *builder = ir_builder_create(module);
    if (!builder) {
        ir_module_destroy(module);
        return NULL;
    }
    
    // 创建一个简单的函数
    IRTypeId i64_type = 0;  // 在实际初始化后会是有效的类型ID
    IRTypeId func_type = 0;
    
    // 创建main函数
    IRFunction *func = ir_module_add_function(module, "main", func_type);
    if (func) {
        ir_builder_set_position(builder, func, NULL);
        
        // 创建入口块
        IRBlock *entry = ir_builder_create_block(builder, "entry");
        ir_builder_set_insert_point(builder, entry);
        
        // 生成一些IR指令
        IRValueId const_42 = ir_builder_int_const(builder, i64_type, 42);
        IRValueId const_10 = ir_builder_int_const(builder, i64_type, 10);
        IRValueId sum = ir_builder_add(builder, const_42, const_10);
        ir_builder_ret(builder, sum);
    }
    
    ir_builder_destroy(builder);
    return module;
}

int main(int argc, char **argv) {
    // 默认选项
    SilverCompileOptions options = silver_options_default();
    
    // 解析命令行参数
    if (!parse_options(argc, argv, &options)) {
        print_usage(argv[0]);
        return 1;
    }
    
    // 创建编译上下文
    SilverContext *ctx = silver_context_create();
    if (!ctx) {
        fprintf(stderr, "Failed to create compilation context\n");
        return 1;
    }
    
    // 如果有错误上下文，设置详细输出
    if (options.verbose) {
        SilverErrorContext *error_ctx = silver_get_error_context(ctx);
        if (error_ctx) {
            // 错误将在编译过程中收集
        }
    }
    
    // 创建IR模块（这里使用测试模块，实际应该从输入文件解析）
    IRModule *module = create_test_module(ctx);
    if (!module) {
        fprintf(stderr, "Failed to create IR module\n");
        silver_context_destroy(ctx);
        return 1;
    }
    
    if (options.verbose) {
        printf("Target: %s (%s)\n", 
               silver_target_arch_name(options.target_arch),
               options.target_triple);
        printf("Output: %s\n", options.output_file);
        printf("Optimizing...\n");
        
        // 打印IR
        printf("\nOriginal IR:\n");
        ir_module_dump(module, stdout);
    }
    
    // 编译
    bool success = silver_compile_to_file(ctx, module, &options);
    
    // 清理
    ir_module_destroy(module);
    
    // 打印错误（如果有）
    SilverErrorContext *error_ctx = silver_get_error_context(ctx);
    if (error_ctx && silver_error_has_errors(error_ctx)) {
        silver_error_print_all(error_ctx, stderr);
    }
    
    silver_context_destroy(ctx);
    
    if (success) {
        if (options.verbose) {
            printf("Compilation successful!\n");
        }
        return 0;
    } else {
        fprintf(stderr, "Compilation failed.\n");
        return 1;
    }
}