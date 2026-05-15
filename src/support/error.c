#include "silver/support/error.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

#ifdef _WIN32
    #include <io.h>           // Windows: _isatty, _fileno
    #define isatty _isatty
    #define fileno _fileno
#else
    #include <unistd.h>       // POSIX: isatty, fileno
#endif

// 错误代码对应的描述字符串
static const char *error_strings[] = {
    [SILVER_ERR_NONE] = "no error",
    [SILVER_ERR_OUT_OF_MEMORY] = "out of memory",
    [SILVER_ERR_INVALID_ARGUMENT] = "invalid argument",
    [SILVER_ERR_INTERNAL] = "internal compiler error",
    
    [SILVER_ERR_IR_INVALID_TYPE] = "invalid IR type",
    [SILVER_ERR_IR_INVALID_OPCODE] = "invalid IR opcode",
    [SILVER_ERR_IR_INVALID_OPERAND] = "invalid IR operand",
    [SILVER_ERR_IR_TYPE_MISMATCH] = "IR type mismatch",
    [SILVER_ERR_IR_VALUE_NOT_FOUND] = "IR value not found",
    [SILVER_ERR_IR_BLOCK_NOT_FOUND] = "IR basic block not found",
    [SILVER_ERR_IR_INSTRUCTION_LIMIT] = "IR instruction limit exceeded",
    
    [SILVER_ERR_OPT_INVALID_PASS] = "invalid optimization pass",
    [SILVER_ERR_OPT_INVALID_IR] = "invalid IR for optimization",
    
    [SILVER_ERR_CODEGEN_UNSUPPORTED_OP] = "unsupported operation in code generation",
    [SILVER_ERR_CODEGEN_REGISTER_ALLOC_FAILED] = "register allocation failed",
    [SILVER_ERR_CODEGEN_INSTRUCTION_SELECTION_FAILED] = "instruction selection failed",
    [SILVER_ERR_CODEGEN_SPILL_FAILED] = "register spilling failed",
    
    [SILVER_ERR_TARGET_UNSUPPORTED] = "unsupported target platform",
    [SILVER_ERR_TARGET_ENCODING_FAILED] = "target encoding failed",
    [SILVER_ERR_TARGET_ABI_VIOLATION] = "ABI violation",
    
    [SILVER_ERR_LINK_UNRESOLVED_SYMBOL] = "unresolved symbol",
    [SILVER_ERR_LINK_FORMAT_ERROR] = "link format error",
    [SILVER_ERR_LINK_RELOCATION_ERROR] = "relocation error",
    
    [SILVER_ERR_IO_FILE_NOT_FOUND] = "file not found",
    [SILVER_ERR_IO_READ_ERROR] = "read error",
    [SILVER_ERR_IO_WRITE_ERROR] = "write error",
};

// 错误级别对应的颜色和前缀
static const char *level_colors[] = {
    [SILVER_ERROR_INFO] = "\033[1;34m",     // 蓝色
    [SILVER_ERROR_WARNING] = "\033[1;33m",  // 黄色
    [SILVER_ERROR_ERROR] = "\033[1;31m",    // 红色
    [SILVER_ERROR_FATAL] = "\033[1;35m",    // 紫色
    [SILVER_ERROR_INTERNAL] = "\033[1;31m", // 红色
};

static const char *level_prefixes[] = {
    [SILVER_ERROR_INFO] = "info",
    [SILVER_ERROR_WARNING] = "warning",
    [SILVER_ERROR_ERROR] = "error",
    [SILVER_ERROR_FATAL] = "fatal error",
    [SILVER_ERROR_INTERNAL] = "internal error",
};

SilverErrorContext *silver_error_context_create(void) {
    SilverErrorContext *ctx = (SilverErrorContext *)calloc(1, sizeof(SilverErrorContext));
    if (!ctx) return NULL;
    
    ctx->max_diagnostics = 100;  // 默认最多100条诊断
    ctx->fatal_encountered = false;
    
    return ctx;
}

void silver_error_context_destroy(SilverErrorContext *ctx) {
    if (!ctx) return;
    
    // 释放所有诊断链表
    SilverDiagnostic *diag = ctx->first;
    while (diag) {
        SilverDiagnostic *next = diag->next;
        free(diag->help_text);
        free(diag);
        diag = next;
    }
    
    free(ctx);
}

void silver_error_context_reset(SilverErrorContext *ctx) {
    if (!ctx) return;
    
    SilverDiagnostic *diag = ctx->first;
    while (diag) {
        SilverDiagnostic *next = diag->next;
        free(diag->help_text);
        free(diag);
        diag = next;
    }
    
    ctx->first = NULL;
    ctx->last = NULL;
    ctx->count = 0;
    ctx->error_count = 0;
    ctx->warning_count = 0;
    ctx->fatal_encountered = false;
}

SilverDiagnostic *silver_error_addv(
    SilverErrorContext *ctx,
    SilverErrorLevel level,
    SilverErrorCode code,
    const SilverSourceLocation *location,
    const char *fmt,
    va_list args
) {
    if (!ctx) return NULL;
    
    // 检查是否达到最大诊断数量
    if (ctx->count >= ctx->max_diagnostics) {
        if (ctx->count == ctx->max_diagnostics) {
            // 添加一条"诊断数量限制"消息
            ctx->count++;
            fprintf(stderr, "silver: too many diagnostics (limit %u), suppressing further output\n",
                    ctx->max_diagnostics);
        }
        return NULL;
    }
    
    // 遇到致命错误后，只接受致命错误
    if (ctx->fatal_encountered && level < SILVER_ERROR_FATAL) {
        return NULL;
    }
    
    SilverDiagnostic *diag = (SilverDiagnostic *)calloc(1, sizeof(SilverDiagnostic));
    if (!diag) return NULL;
    
    diag->level = level;
    diag->code = code;
    if (location) {
        diag->location = *location;
    }
    
    vsnprintf(diag->message, sizeof(diag->message), fmt, args);
    diag->message[sizeof(diag->message) - 1] = '\0';
    
    // 添加到链表
    if (!ctx->first) {
        ctx->first = diag;
        ctx->last = diag;
    } else {
        ctx->last->next = diag;
        ctx->last = diag;
    }
    
    ctx->count++;
    
    // 统计
    switch (level) {
        case SILVER_ERROR_ERROR:
        case SILVER_ERROR_FATAL:
            ctx->error_count++;
            if (level == SILVER_ERROR_FATAL) {
                ctx->fatal_encountered = true;
            }
            break;
        case SILVER_ERROR_WARNING:
            ctx->warning_count++;
            break;
        default:
            break;
    }
    
    // 调用报告回调
    if (ctx->report_callback) {
        ctx->report_callback(diag, ctx->user_data);
    }
    
    return diag;
}

SilverDiagnostic *silver_error_add(
    SilverErrorContext *ctx,
    SilverErrorLevel level,
    SilverErrorCode code,
    const SilverSourceLocation *location,
    const char *fmt, ...
) {
    va_list args;
    va_start(args, fmt);
    SilverDiagnostic *diag = silver_error_addv(ctx, level, code, location, fmt, args);
    va_end(args);
    return diag;
}

void silver_error_add_help(SilverDiagnostic *diag, const char *help_fmt, ...) {
    if (!diag) return;
    
    va_list args;
    va_start(args, help_fmt);
    
    // 计算需要的长度
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, help_fmt, args_copy);
    va_end(args_copy);
    
    if (len > 0) {
        free(diag->help_text);
        diag->help_text = (char *)malloc(len + 1);
        if (diag->help_text) {
            vsnprintf(diag->help_text, len + 1, help_fmt, args);
        }
    }
    
    va_end(args);
}

void silver_error_add_note(
    SilverErrorContext *ctx,
    const SilverSourceLocation *location,
    const char *fmt, ...
) {
    if (!ctx) return;
    
    va_list args;
    va_start(args, fmt);
    
    SilverDiagnostic *note = silver_error_addv(ctx, SILVER_ERROR_INFO, 
                                                SILVER_ERR_NONE, location, fmt, args);
    if (note) {
        // 重写消息，添加"note: "前缀
        char buffer[sizeof(note->message)];
        snprintf(buffer, sizeof(buffer), "note: %s", note->message);
        strncpy(note->message, buffer, sizeof(note->message));
        note->message[sizeof(note->message) - 1] = '\0';
    }
    
    va_end(args);
}

bool silver_error_has_errors(const SilverErrorContext *ctx) {
    return ctx && ctx->error_count > 0;
}

bool silver_error_has_fatal(const SilverErrorContext *ctx) {
    return ctx && ctx->fatal_encountered;
}

const char *silver_error_code_string(SilverErrorCode code) {
    if (code >= 0 && code < SILVER_ERR_COUNT && error_strings[code]) {
        return error_strings[code];
    }
    return "unknown error";
}

void silver_error_print_diagnostic(const SilverDiagnostic *diag, FILE *out) {
    if (!diag || !out) return;
    
    bool use_color = (out == stderr || out == stdout) && isatty(fileno(out));
    
    // 打印位置信息
    if (diag->location.filename) {
        fprintf(out, "%s", diag->location.filename);
        if (diag->location.line > 0) {
            fprintf(out, ":%u", diag->location.line);
            if (diag->location.column > 0) {
                fprintf(out, ":%u", diag->location.column);
            }
        }
        fprintf(out, ": ");
    }
    
    // 打印级别和消息
    if (use_color) {
        fprintf(out, "%s%s:\033[0m ", level_colors[diag->level], 
                level_prefixes[diag->level]);
    } else {
        fprintf(out, "%s: ", level_prefixes[diag->level]);
    }
    
    fprintf(out, "%s [%s]\n", diag->message, silver_error_code_string(diag->code));
    
    // 打印源代码上下文
    if (diag->location.source_text && diag->location.line > 0) {
        fprintf(out, "    %s\n", diag->location.source_text);
        if (diag->location.column > 0) {
            fprintf(out, "    %*s^\n", diag->location.column - 1, "");
        }
    }
    
    // 打印帮助信息
    if (diag->help_text) {
        fprintf(out, "help: %s\n", diag->help_text);
    }
    
    fprintf(out, "\n");
}

void silver_error_print_all(const SilverErrorContext *ctx, FILE *out) {
    if (!ctx || !out) return;
    
    const SilverDiagnostic *diag = ctx->first;
    while (diag) {
        silver_error_print_diagnostic(diag, out);
        diag = diag->next;
    }
    
    // 打印统计信息
    if (ctx->error_count > 0 || ctx->warning_count > 0) {
        fprintf(out, "%u error(s), %u warning(s) generated.\n",
                ctx->error_count, ctx->warning_count);
    }
}

void silver_error_set_callback(
    SilverErrorContext *ctx,
    void (*callback)(const SilverDiagnostic *diag, void *user_data),
    void *user_data
) {
    if (!ctx) return;
    ctx->report_callback = callback;
    ctx->user_data = user_data;
}

void silver_error_set_max_diagnostics(SilverErrorContext *ctx, uint32_t max) {
    if (!ctx) return;
    ctx->max_diagnostics = max;
}