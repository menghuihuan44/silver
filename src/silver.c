#include "silver/silver.h"
#include "silver/target/x86/x86.h"
#include "silver/target/arm/arm.h"
#include "silver/target/riscv/riscv.h"
#include "silver/link/elf.h"
#include "silver/link/pe.h"
#include "silver/link/macho.h"
#include "silver/debug/dwarf.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

// 编译上下文结构
struct SilverContext {
    SilverTarget *target;               // 当前目标平台
    SilverErrorContext *error_ctx;      // 错误上下文
    uint32_t compile_count;             // 编译次数统计
    double total_compile_time;          // 总编译时间
};

SilverContext *silver_context_create(void) {
    SilverContext *ctx = (SilverContext *)calloc(1, sizeof(SilverContext));
    if (!ctx) return NULL;
    
    ctx->error_ctx = silver_error_context_create();
    ctx->compile_count = 0;
    ctx->total_compile_time = 0.0;
    
    return ctx;
}

void silver_context_destroy(SilverContext *ctx) {
    if (!ctx) return;
    
    if (ctx->target) {
        silver_target_destroy(ctx->target);
    }
    
    if (ctx->error_ctx) {
        silver_error_context_destroy(ctx->error_ctx);
    }
    
    free(ctx);
}

SilverCompileOptions silver_options_default(void) {
    SilverCompileOptions opts;
    memset(&opts, 0, sizeof(opts));
    
    opts.target_arch = SILVER_TARGET_X86_64;
    opts.target_triple = "x86_64-unknown-none";
    opts.output_file = "output.o";
    opts.optimize = true;
    opts.debug_info = false;
    opts.pic = false;
    opts.verbose = false;
    opts.output_format = SILVER_OUTPUT_OBJECT;
    opts.object_format = SILVER_FORMAT_ELF;
    
    return opts;
}

const char *silver_version_string(void) {
    return SILVER_VERSION_STRING;
}

const char *silver_target_arch_name(SilverTargetArch arch) {
    switch (arch) {
        case SILVER_TARGET_X86_64:  return "x86-64";
        case SILVER_TARGET_X86_32:  return "x86-32";
        case SILVER_TARGET_ARM64:   return "ARM64";
        case SILVER_TARGET_ARM32:   return "ARM32";
        case SILVER_TARGET_RISCV64: return "RISC-V64";
        case SILVER_TARGET_RISCV32: return "RISC-V32";
        default: return "unknown";
    }
}

SilverTargetArch silver_target_from_triple(const char *triple) {
    if (!triple) return SILVER_TARGET_X86_64;
    
    if (strncmp(triple, "x86_64", 6) == 0) return SILVER_TARGET_X86_64;
    if (strncmp(triple, "i386", 4) == 0 || strncmp(triple, "i686", 4) == 0)
        return SILVER_TARGET_X86_32;
    if (strncmp(triple, "aarch64", 7) == 0 || strncmp(triple, "arm64", 5) == 0)
        return SILVER_TARGET_ARM64;
    if (strncmp(triple, "arm", 3) == 0) return SILVER_TARGET_ARM32;
    if (strncmp(triple, "riscv64", 7) == 0) return SILVER_TARGET_RISCV64;
    if (strncmp(triple, "riscv32", 7) == 0) return SILVER_TARGET_RISCV32;
    
    return SILVER_TARGET_X86_64;  // 默认
}

SilverTarget *silver_target_create(SilverTargetArch arch) {
    switch (arch) {
        case SILVER_TARGET_X86_64:
            return x86_target_create();
        case SILVER_TARGET_ARM64:
            return arm64_target_create();
        case SILVER_TARGET_RISCV64:
            return riscv64_target_create();
        default:
            return NULL;
    }
}

void silver_target_destroy(SilverTarget *target) {
    if (!target) return;
    
    switch (target->arch) {
        case SILVER_TARGET_X86_64:
            x86_target_destroy(target);
            break;
        case SILVER_TARGET_ARM64:
            arm64_target_destroy(target);
            break;
        case SILVER_TARGET_RISCV64:
            riscv64_target_destroy(target);
            break;
        default:
            free(target);
            break;
    }
}

void silver_set_error_context(SilverContext *ctx, SilverErrorContext *error_ctx) {
    if (!ctx) return;
    ctx->error_ctx = error_ctx;
}

SilverErrorContext *silver_get_error_context(SilverContext *ctx) {
    return ctx ? ctx->error_ctx : NULL;
}

// 主编译流程
SilverCompileResult *silver_compile(SilverContext *ctx,
                                     IRModule *module,
                                     const SilverCompileOptions *options) {
    if (!ctx || !module || !options) return NULL;
    
    clock_t start_time = clock();
    
    // 创建编译结果
    SilverCompileResult *result = (SilverCompileResult *)calloc(1, sizeof(SilverCompileResult));
    if (!result) return NULL;
    
    result->code = silver_buffer_create(65536);
    result->data = silver_buffer_create(4096);
    result->debug_info = silver_buffer_create(4096);
    result->unwind_info = silver_buffer_create(4096);
    result->error_ctx = ctx->error_ctx;
    
    // 创建或使用现有的目标平台
    SilverTarget *target = ctx->target;
    if (!target || target->arch != options->target_arch) {
        if (target) {
            silver_target_destroy(target);
        }
        target = silver_target_create(options->target_arch);
        if (!target) {
            silver_error_add(ctx->error_ctx, SILVER_ERROR_ERROR,
                SILVER_ERR_TARGET_UNSUPPORTED, NULL,
                "Unsupported target architecture: %s",
                silver_target_arch_name(options->target_arch));
            goto error;
        }
        ctx->target = target;
    }
    
    // 1. 验证IR模块
    if (!ir_module_verify(module)) {
        silver_error_add(ctx->error_ctx, SILVER_ERROR_ERROR,
            SILVER_ERR_OPT_INVALID_IR, NULL,
            "IR module verification failed");
        goto error;
    }
    
    // 2. 运行优化Pass
    if (options->optimize) {
        OptContext *opt_ctx = opt_context_create(module);
        if (opt_ctx) {
            opt_run_all(opt_ctx);
            
            // 获取优化统计
            char stats_buf[256];
            opt_get_stats(opt_ctx, stats_buf, sizeof(stats_buf));
            
            if (options->verbose) {
                printf("Optimization statistics:\n%s\n", stats_buf);
            }
            
            opt_context_destroy(opt_ctx);
        }
    }
    
    // 3. 代码生成
    CodeGenContext *cg_ctx = codegen_create(target, module);
    if (!cg_ctx) {
        silver_error_add(ctx->error_ctx, SILVER_ERROR_ERROR,
            SILVER_ERR_CODEGEN_INSTRUCTION_SELECTION_FAILED, NULL,
            "Failed to create code generation context");
        goto error;
    }

    // 设置匹配表
    switch (target->arch) {
        case SILVER_TARGET_X86_64:
            cg_ctx->match_table = x86_match_table;
            cg_ctx->table_size = x86_match_table_size;
            break;
        case SILVER_TARGET_ARM64:
            cg_ctx->match_table = arm64_match_table;
            cg_ctx->table_size = arm64_match_table_size;
            break;
        case SILVER_TARGET_RISCV64:
            cg_ctx->match_table = riscv64_match_table;
            cg_ctx->table_size = riscv64_match_table_size;
            break;
        default:
            silver_error_add(ctx->error_ctx, SILVER_ERROR_ERROR,
                SILVER_ERR_TARGET_UNSUPPORTED, NULL,
                "No match table for target architecture");
            codegen_destroy(cg_ctx);
            goto error;
    }
    isel_build_opcode_index(cg_ctx);
    
    // 生成代码
    if (!codegen_generate(cg_ctx, result->code)) {
        silver_error_add(ctx->error_ctx, SILVER_ERROR_ERROR,
            SILVER_ERR_CODEGEN_INSTRUCTION_SELECTION_FAILED, NULL,
            "Code generation failed");
        codegen_destroy(cg_ctx);
        goto error;
    }
    
    // 获取代码大小统计
    result->code_size = silver_buffer_length(result->code);
    
    codegen_destroy(cg_ctx);
    
    // 4. 生成调试信息（如果启用）
    if (options->debug_info) {
        // DWARF调试信息生成
        // 这里调用DWARF生成器
        extern bool dwarf_generate_debug_info(IRModule *module, SilverBuffer *output);
        dwarf_generate_debug_info(module, result->debug_info);
    }
    
    // 5. 包装输出格式
    if (options->output_format == SILVER_OUTPUT_OBJECT) {
        SilverBuffer *packaged = silver_buffer_create(
            silver_buffer_length(result->code) + 
            silver_buffer_length(result->data) + 
            silver_buffer_length(result->debug_info) + 4096);
        
        switch (options->object_format) {
            case SILVER_FORMAT_ELF:
                if (!elf_write_object(module, result, target, packaged)) {
                    silver_buffer_destroy(packaged);
                    goto error;
                }
                break;
            case SILVER_FORMAT_PE:
                if (!pe_write_object(module, result, target, packaged)) {
                    silver_buffer_destroy(packaged);
                    goto error;
                }
                break;
            case SILVER_FORMAT_MACHO:
                if (!macho_write_object(module, result, target, packaged)) {
                    silver_buffer_destroy(packaged);
                    goto error;
                }
                break;
        }
        
        // 用包装后的代码替换原始代码
        silver_buffer_destroy(result->code);
        result->code = packaged;
        result->code_size = silver_buffer_length(result->code);
    }
    
    // 编译时间统计
    clock_t end_time = clock();
    double compile_time = (double)(end_time - start_time) / CLOCKS_PER_SEC * 1000.0;
    ctx->total_compile_time += compile_time;
    ctx->compile_count++;
    
    if (options->verbose) {
        printf("Compilation completed in %.2f ms\n", compile_time);
        printf("Code size: %u bytes\n", result->code_size);
    }
    
    return result;
    
error:
    silver_compile_result_destroy(result);
    return NULL;
}

bool silver_compile_to_file(SilverContext *ctx,
                             IRModule *module,
                             const SilverCompileOptions *options) {
    if (!ctx || !module || !options) return false;
    
    SilverCompileResult *result = silver_compile(ctx, module, options);
    if (!result) return false;
    
    // 写入文件
    FILE *fp = fopen(options->output_file, "wb");
    if (!fp) {
        silver_error_add(ctx->error_ctx, SILVER_ERROR_ERROR,
            SILVER_ERR_IO_WRITE_ERROR, NULL,
            "Cannot open output file: %s", options->output_file);
        silver_compile_result_destroy(result);
        return false;
    }
    
    size_t written = fwrite(silver_buffer_data(result->code), 1,
                            silver_buffer_length(result->code), fp);
    fclose(fp);
    
    bool success = (written == silver_buffer_length(result->code));
    silver_compile_result_destroy(result);
    
    return success;
}

void silver_compile_result_destroy(SilverCompileResult *result) {
    if (!result) return;
    
    silver_buffer_destroy(result->code);
    silver_buffer_destroy(result->data);
    silver_buffer_destroy(result->debug_info);
    silver_buffer_destroy(result->unwind_info);
    
    free(result);
}

void silver_get_compile_stats(const SilverCompileResult *result,
                               char *buffer, size_t buffer_size) {
    if (!result || !buffer) return;
    
    snprintf(buffer, buffer_size,
        "Compilation Results:\n"
        "  Code section: %u bytes\n"
        "  Data section: %u bytes\n"
        "  Debug info: %u bytes\n"
        "  Unwind info: %u bytes\n",
        result->code_size,
        result->data_size,
        result->debug_info ? (uint32_t)silver_buffer_length(result->debug_info) : 0,
        result->unwind_info ? (uint32_t)silver_buffer_length(result->unwind_info) : 0);
}