#include "silver/silver.h"
#include "silver/ir/ir_builder.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static bool test_##name(void)
#define RUN_TEST(name) do { \
    tests_run++; \
    printf("  Running %s... ", #name); \
    if (test_##name()) { tests_passed++; printf("PASSED\n"); } \
    else { tests_failed++; printf("FAILED\n"); } \
} while(0)

// 创建包含多种指令的测试模块
static IRModule *create_complex_module(const char *triple) {
    IRModule *module = ir_module_create("complex_test", triple);
    if (!module) return NULL;
    
    IRBuilder *builder = ir_builder_create(module);
    if (!builder) { ir_module_destroy(module); return NULL; }
    
    IRFunction *func = ir_module_add_function(module, "complex_func", 0);
    if (!func) { ir_builder_destroy(builder); ir_module_destroy(module); return NULL; }
    
    ir_builder_set_position(builder, func, NULL);
    IRBlock *entry = ir_builder_create_block(builder, "entry");
    ir_builder_set_insert_point(builder, entry);
    
    IRValueId a = ir_builder_int_const(builder, builder->i64_type, 100);
    IRValueId b = ir_builder_int_const(builder, builder->i64_type, 200);
    IRValueId c = ir_builder_int_const(builder, builder->i64_type, 3);
    
    IRValueId add = ir_builder_add(builder, a, b);
    IRValueId sub = ir_builder_sub(builder, add, c);
    IRValueId mul = ir_builder_mul(builder, sub, c);
    IRValueId div = ir_builder_div(builder, mul, c, true);
    IRValueId and_val = ir_builder_and(builder, div, a);
    IRValueId or_val  = ir_builder_or(builder, and_val, b);
    IRValueId xor_val = ir_builder_xor(builder, or_val, c);
    IRValueId shl_val = ir_builder_shl(builder, xor_val, 
        ir_builder_int_const(builder, builder->i64_type, 2));
    
    ir_builder_ret(builder, shl_val);
    
    ir_builder_destroy(builder);
    return module;
}

// ============================================================
// 测试 x86-64 完整编译
// ============================================================
static bool test_x86_64_full(void) {
    SilverContext *ctx = silver_context_create();
    if (!ctx) return false;
    
    IRModule *module = create_complex_module("x86_64-unknown-none");
    if (!module) { silver_context_destroy(ctx); return false; }
    
    SilverCompileOptions options = silver_options_default();
    options.target_arch = SILVER_TARGET_X86_64;
    options.output_file = "test_x86_full.o";
    options.debug_info = true;
    
    bool success = silver_compile_to_file(ctx, module, &options);
    
    ir_module_destroy(module);
    silver_context_destroy(ctx);
    if (success) remove("test_x86_full.o");
    return success;
}

// ============================================================
// 测试 ARM64 完整编译
// ============================================================
static bool test_arm64_full(void) {
    SilverContext *ctx = silver_context_create();
    if (!ctx) return false;
    
    IRModule *module = create_complex_module("aarch64-unknown-none");
    if (!module) { silver_context_destroy(ctx); return false; }
    
    SilverCompileOptions options = silver_options_default();
    options.target_arch = SILVER_TARGET_ARM64;
    options.output_file = "test_arm64_full.o";
    
    bool success = silver_compile_to_file(ctx, module, &options);
    
    ir_module_destroy(module);
    silver_context_destroy(ctx);
    if (success) remove("test_arm64_full.o");
    return success;
}

// ============================================================
// 测试 RISC-V64 完整编译
// ============================================================
static bool test_riscv64_full(void) {
    SilverContext *ctx = silver_context_create();
    if (!ctx) return false;
    
    IRModule *module = create_complex_module("riscv64-unknown-none");
    if (!module) { silver_context_destroy(ctx); return false; }
    
    SilverCompileOptions options = silver_options_default();
    options.target_arch = SILVER_TARGET_RISCV64;
    options.output_file = "test_riscv64_full.o";
    
    bool success = silver_compile_to_file(ctx, module, &options);
    
    ir_module_destroy(module);
    silver_context_destroy(ctx);
    if (success) remove("test_riscv64_full.o");
    return success;
}

// ============================================================
// 测试 ELF 输出格式
// ============================================================
static bool test_elf_output(void) {
    SilverContext *ctx = silver_context_create();
    if (!ctx) return false;
    
    IRModule *module = create_complex_module("x86_64-unknown-none");
    if (!module) { silver_context_destroy(ctx); return false; }
    
    SilverCompileOptions options = silver_options_default();
    options.target_arch = SILVER_TARGET_X86_64;
    options.object_format = SILVER_FORMAT_ELF;
    options.output_file = "test_elf.o";
    
    SilverCompileResult *result = silver_compile(ctx, module, &options);
    if (!result) { ir_module_destroy(module); silver_context_destroy(ctx); return false; }
    
    bool success = false;
    if (silver_buffer_length(result->code) >= 4) {
        uint8_t *data = silver_buffer_data(result->code);
        success = (data[0] == 0x7F && data[1] == 'E' && data[2] == 'L' && data[3] == 'F');
    }
    
    silver_compile_result_destroy(result);
    ir_module_destroy(module);
    silver_context_destroy(ctx);
    if (success) remove("test_elf.o");
    return success;
}

// ============================================================
// 测试 PE 输出格式
// ============================================================
static bool test_pe_output(void) {
    SilverContext *ctx = silver_context_create();
    if (!ctx) return false;
    
    IRModule *module = create_complex_module("x86_64-unknown-none");
    if (!module) { silver_context_destroy(ctx); return false; }
    
    SilverCompileOptions options = silver_options_default();
    options.target_arch = SILVER_TARGET_X86_64;
    options.object_format = SILVER_FORMAT_PE;
    options.output_file = "test_pe.obj";
    
    SilverCompileResult *result = silver_compile(ctx, module, &options);
    bool success = (result != NULL && silver_buffer_length(result->code) > 0);
    
    if (result) silver_compile_result_destroy(result);
    ir_module_destroy(module);
    silver_context_destroy(ctx);
    if (success) remove("test_pe.obj");
    return success;
}

// ============================================================
// 测试 Mach-O 输出格式
// ============================================================
static bool test_macho_output(void) {
    SilverContext *ctx = silver_context_create();
    if (!ctx) return false;
    
    IRModule *module = create_complex_module("x86_64-unknown-none");
    if (!module) { silver_context_destroy(ctx); return false; }
    
    SilverCompileOptions options = silver_options_default();
    options.target_arch = SILVER_TARGET_X86_64;
    options.object_format = SILVER_FORMAT_MACHO;
    options.output_file = "test_macho.o";
    
    SilverCompileResult *result = silver_compile(ctx, module, &options);
    bool success = false;
    if (result && silver_buffer_length(result->code) >= 4) {
        uint32_t magic = *(uint32_t *)silver_buffer_data(result->code);
        success = (magic == 0xFEEDFACF || magic == 0xCFFAEDFE);
    }
    
    if (result) silver_compile_result_destroy(result);
    ir_module_destroy(module);
    silver_context_destroy(ctx);
    if (success) remove("test_macho.o");
    return success;
}

// ============================================================
// 测试带调试信息的编译
// ============================================================
static bool test_debug_info(void) {
    SilverContext *ctx = silver_context_create();
    if (!ctx) return false;
    
    IRModule *module = create_complex_module("x86_64-unknown-none");
    if (!module) { silver_context_destroy(ctx); return false; }
    
    SilverCompileOptions options = silver_options_default();
    options.target_arch = SILVER_TARGET_X86_64;
    options.debug_info = true;
    options.output_file = "test_debug.o";
    
    SilverCompileResult *result = silver_compile(ctx, module, &options);
    bool success = (result != NULL && result->debug_info != NULL && 
                    silver_buffer_length(result->debug_info) > 0);
    
    if (result) silver_compile_result_destroy(result);
    ir_module_destroy(module);
    silver_context_destroy(ctx);
    if (success) remove("test_debug.o");
    return success;
}

int main(void) {
    printf("=== Silver Platform Integration Tests ===\n\n");
    
    RUN_TEST(x86_64_full);
    RUN_TEST(arm64_full);
    RUN_TEST(riscv64_full);
    RUN_TEST(elf_output);
    RUN_TEST(pe_output);
    RUN_TEST(macho_output);
    RUN_TEST(debug_info);
    
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}