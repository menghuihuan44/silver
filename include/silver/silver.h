#ifndef SILVER_H
#define SILVER_H

#include "silver/ir/ir_module.h"
#include "silver/ir/ir_builder.h"
#include "silver/opt/passes.h"
#include "silver/codegen/isel.h"
#include "silver/codegen/emitter.h"
#include "silver/codegen/machine.h"
#include "silver/support/arena.h"
#include "silver/support/error.h"
#include "silver/support/buffer.h"
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Silver编译器后端版本
#define SILVER_VERSION_MAJOR 0
#define SILVER_VERSION_MINOR 1
#define SILVER_VERSION_PATCH 0
#define SILVER_VERSION_STRING "0.1.0"

// 编译选项
typedef struct {
    SilverTargetArch target_arch;       // 目标架构
    const char *target_triple;          // 目标三元组
    const char *output_file;            // 输出文件名
    const char *input_file;             // 输入文件名（用于调试信息）
    
    bool optimize;                      // 是否优化（Silver总是优化，此选项保留）
    bool debug_info;                    // 是否生成调试信息
    bool pic;                           // 是否生成位置无关代码
    bool verbose;                       // 详细输出
    
    // 输出格式
    enum {
        SILVER_OUTPUT_OBJECT,           // 目标文件
        SILVER_OUTPUT_ASM,              // 汇编文件
        SILVER_OUTPUT_RAW,              // 原始二进制
    } output_format;
    
    // 目标文件格式
    enum {
        SILVER_FORMAT_ELF,
        SILVER_FORMAT_PE,
        SILVER_FORMAT_MACHO,
    } object_format;
} SilverCompileOptions;

// 编译结果
typedef struct {
    SilverBuffer *code;                 // 生成的代码
    SilverBuffer *data;                 // 数据段
    SilverBuffer *debug_info;           // 调试信息
    SilverBuffer *unwind_info;          // 展开信息
    
    uint64_t entry_point;               // 入口点地址
    uint32_t code_size;                 // 代码段大小
    uint32_t data_size;                 // 数据段大小
    
    SilverErrorContext *error_ctx;      // 错误上下文
} SilverCompileResult;

// 编译上下文
typedef struct SilverContext SilverContext;

// 创建编译上下文
SilverContext *silver_context_create(void);

// 销毁编译上下文
void silver_context_destroy(SilverContext *ctx);

// 创建编译选项（默认值）
SilverCompileOptions silver_options_default(void);

// 编译IR模块到目标代码
SilverCompileResult *silver_compile(SilverContext *ctx, 
                                     IRModule *module,
                                     const SilverCompileOptions *options);

// 编译并输出到文件
bool silver_compile_to_file(SilverContext *ctx,
                             IRModule *module,
                             const SilverCompileOptions *options);

// 释放编译结果
void silver_compile_result_destroy(SilverCompileResult *result);

// 获取版本字符串
const char *silver_version_string(void);

// 获取目标平台名称
const char *silver_target_arch_name(SilverTargetArch arch);

// 从三元组解析目标架构
SilverTargetArch silver_target_from_triple(const char *triple);

// 创建目标平台实例
SilverTarget *silver_target_create(SilverTargetArch arch);

// 销毁目标平台实例
void silver_target_destroy(SilverTarget *target);

// 错误处理辅助
void silver_set_error_context(SilverContext *ctx, SilverErrorContext *error_ctx);
SilverErrorContext *silver_get_error_context(SilverContext *ctx);

// 获取Silver编译输出的统计信息
void silver_get_compile_stats(const SilverCompileResult *result,
                               char *buffer, size_t buffer_size);

#ifdef __cplusplus
}
#endif

#endif // SILVER_H