#ifndef SILVER_ERROR_H
#define SILVER_ERROR_H

#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// 错误严重级别
typedef enum {
    SILVER_ERROR_INFO,
    SILVER_ERROR_WARNING,
    SILVER_ERROR_ERROR,
    SILVER_ERROR_FATAL,
    SILVER_ERROR_INTERNAL
} SilverErrorLevel;

// 错误代码定义
typedef enum {
    // 通用错误
    SILVER_ERR_NONE = 0,
    SILVER_ERR_OUT_OF_MEMORY,
    SILVER_ERR_INVALID_ARGUMENT,
    SILVER_ERR_INTERNAL,
    
    // IR错误 (1000-1999)
    SILVER_ERR_IR_INVALID_TYPE = 1000,
    SILVER_ERR_IR_INVALID_OPCODE,
    SILVER_ERR_IR_INVALID_OPERAND,
    SILVER_ERR_IR_TYPE_MISMATCH,
    SILVER_ERR_IR_VALUE_NOT_FOUND,
    SILVER_ERR_IR_BLOCK_NOT_FOUND,
    SILVER_ERR_IR_INSTRUCTION_LIMIT,
    
    // 优化错误 (2000-2999)
    SILVER_ERR_OPT_INVALID_PASS = 2000,
    SILVER_ERR_OPT_INVALID_IR,
    
    // 代码生成错误 (3000-3999)
    SILVER_ERR_CODEGEN_UNSUPPORTED_OP = 3000,
    SILVER_ERR_CODEGEN_REGISTER_ALLOC_FAILED,
    SILVER_ERR_CODEGEN_INSTRUCTION_SELECTION_FAILED,
    SILVER_ERR_CODEGEN_SPILL_FAILED,
    
    // 目标平台错误 (4000-4999)
    SILVER_ERR_TARGET_UNSUPPORTED = 4000,
    SILVER_ERR_TARGET_ENCODING_FAILED,
    SILVER_ERR_TARGET_ABI_VIOLATION,
    
    // 链接错误 (5000-5999)
    SILVER_ERR_LINK_UNRESOLVED_SYMBOL = 5000,
    SILVER_ERR_LINK_FORMAT_ERROR,
    SILVER_ERR_LINK_RELOCATION_ERROR,
    
    // 输入输出错误 (6000-6999)
    SILVER_ERR_IO_FILE_NOT_FOUND = 6000,
    SILVER_ERR_IO_READ_ERROR,
    SILVER_ERR_IO_WRITE_ERROR,
    
    // 用户错误数量
    SILVER_ERR_COUNT
} SilverErrorCode;

// 错误上下文 - 包含源位置信息
typedef struct {
    const char *filename;       // 源文件名
    uint32_t line;             // 行号
    uint32_t column;           // 列号
    const char *function;      // 函数名
    const char *source_text;   // 源代码文本（可选）
} SilverSourceLocation;

// 错误诊断信息
typedef struct SilverDiagnostic {
    SilverErrorLevel level;
    SilverErrorCode code;
    SilverSourceLocation location;
    char message[512];         // 错误消息
    char *help_text;           // 帮助文本（可选）
    struct SilverDiagnostic *next;  // 链表
} SilverDiagnostic;

// 错误上下文管理器
typedef struct {
    SilverDiagnostic *first;        // 第一条诊断
    SilverDiagnostic *last;         // 最后一条诊断
    uint32_t count;                 // 诊断总数
    uint32_t error_count;           // 错误数量（不包括警告和信息）
    uint32_t warning_count;         // 警告数量
    uint32_t max_diagnostics;       // 最大诊断数量
    bool fatal_encountered;         // 是否遇到致命错误
    void (*report_callback)(const SilverDiagnostic *diag, void *user_data);
    void *user_data;
} SilverErrorContext;

// 创建错误上下文
SilverErrorContext *silver_error_context_create(void);

// 销毁错误上下文
void silver_error_context_destroy(SilverErrorContext *ctx);

// 重置错误上下文
void silver_error_context_reset(SilverErrorContext *ctx);

// 添加诊断信息
SilverDiagnostic *silver_error_add(
    SilverErrorContext *ctx,
    SilverErrorLevel level,
    SilverErrorCode code,
    const SilverSourceLocation *location,
    const char *fmt, ...
) __attribute__((format(printf, 5, 6)));

// 添加诊断信息（va_list版本）
SilverDiagnostic *silver_error_addv(
    SilverErrorContext *ctx,
    SilverErrorLevel level,
    SilverErrorCode code,
    const SilverSourceLocation *location,
    const char *fmt,
    va_list args
);

// 为诊断添加帮助信息
void silver_error_add_help(SilverDiagnostic *diag, const char *help_fmt, ...)
    __attribute__((format(printf, 2, 3)));

// 添加注释（附加信息）
void silver_error_add_note(
    SilverErrorContext *ctx,
    const SilverSourceLocation *location,
    const char *fmt, ...
) __attribute__((format(printf, 3, 4)));

// 检查是否有错误
bool silver_error_has_errors(const SilverErrorContext *ctx);

// 检查是否有致命错误
bool silver_error_has_fatal(const SilverErrorContext *ctx);

// 打印所有诊断到文件
void silver_error_print_all(const SilverErrorContext *ctx, FILE *out);

// 打印单个诊断到文件
void silver_error_print_diagnostic(const SilverDiagnostic *diag, FILE *out);

// 获取错误描述字符串
const char *silver_error_code_string(SilverErrorCode code);

// 设置诊断报告回调
void silver_error_set_callback(
    SilverErrorContext *ctx,
    void (*callback)(const SilverDiagnostic *diag, void *user_data),
    void *user_data
);

// 限制最大诊断数量
void silver_error_set_max_diagnostics(SilverErrorContext *ctx, uint32_t max);

#ifdef __cplusplus
}
#endif

#endif // SILVER_ERROR_H