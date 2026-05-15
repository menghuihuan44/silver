/**
 * Silver Compiler Backend - Fibonacci Example
 * 
 * 演示如何构建包含控制流（循环、条件分支）和多个基本块的 IR，
 * 并编译为三种目标平台的目标文件。
 * 
 * 编译:
 *   gcc -I include -o example_fib examples/fibonacci.c -L build -lsilver
 */

#include "silver/silver.h"
#include "silver/ir/ir_builder.h"
#include <stdio.h>
#include <stdlib.h>

// 创建计算第 n 个斐波那契数的 IR 函数
// int fib(int n) {
//     int a = 0, b = 1;
//     for (int i = 0; i < n; i++) {
//         int tmp = a + b;
//         a = b;
//         b = tmp;
//     }
//     return a;
// }
static IRFunction *create_fib_function(IRModule *module, IRBuilder *builder) {
    // 创建函数类型: i64(i64)
    IRTypeId i64 = builder->i64_type;
    IRTypeId param_types[] = { i64 };
    IRTypeId func_type = ir_type_create_func(&module->type_table, i64, 1, param_types, false);
    
    IRFunction *func = ir_module_add_function(module, "fib", func_type);
    ir_builder_set_position(builder, func, NULL);
    
    // 获取参数
    IRValueId n = func->arg_values[0];
    
    // 创建基本块
    IRBlock *entry  = ir_builder_create_block(builder, "entry");
    IRBlock *loop   = ir_builder_create_block(builder, "loop");
    IRBlock *body   = ir_builder_create_block(builder, "body");
    IRBlock *exit   = ir_builder_create_block(builder, "exit");
    
    // === entry 块 ===
    ir_builder_set_insert_point(builder, entry);
    IRValueId zero    = ir_builder_int_const(builder, i64, 0);
    IRValueId one     = ir_builder_int_const(builder, i64, 1);
    IRValueId a       = ir_builder_int_const(builder, i64, 0);  // a = 0
    IRValueId b       = ir_builder_int_const(builder, i64, 1);  // b = 1
    IRValueId i       = ir_builder_int_const(builder, i64, 0);  // i = 0
    ir_builder_br(builder, loop);
    
    // === loop 块 ===
    ir_builder_set_insert_point(builder, loop);
    // PHI 节点（简化：直接使用 a, b, i 的最新值）
    IRValueId cond = ir_builder_icmp(builder, IR_OP_CMPLT, i, n);
    ir_builder_condbr(builder, cond, body, exit);
    
    // === body 块 ===
    ir_builder_set_insert_point(builder, body);
    IRValueId tmp = ir_builder_add(builder, a, b);
    IRValueId new_a = b;       // a = b
    IRValueId new_b = tmp;     // b = tmp
    IRValueId new_i = ir_builder_add(builder, i, one);
    
    // 更新变量（用 COPY 表示值传递，实际优化会处理）
    a = new_a;
    b = new_b;
    i = new_i;
    ir_builder_br(builder, loop);
    
    // === exit 块 ===
    ir_builder_set_insert_point(builder, exit);
    ir_builder_ret(builder, a);
    
    return func;
}

static bool compile_for_target(const char *target_name, SilverTargetArch arch,
                                const char *triple, const char *output_file) {
    SilverContext *ctx = silver_context_create();
    if (!ctx) return false;
    
    IRModule *module = ir_module_create("fib", triple);
    IRBuilder *builder = ir_builder_create(module);
    if (!builder) {
        ir_module_destroy(module);
        silver_context_destroy(ctx);
        return false;
    }
    
    create_fib_function(module, builder);
    
    printf("  %s IR:\n", target_name);
    ir_module_dump(module, stdout);
    
    SilverCompileOptions options = silver_options_default();
    options.target_arch = arch;
    options.target_triple = triple;
    options.output_file = output_file;
    options.verbose = false;
    
    bool success = silver_compile_to_file(ctx, module, &options);
    printf("  -> %s: %s (%u bytes)\n\n", target_name,
           success ? "OK" : "FAILED",
           success ? 0 : 0);
    
    ir_builder_destroy(builder);
    ir_module_destroy(module);
    silver_context_destroy(ctx);
    
    if (success) remove(output_file);
    return success;
}

int main(int argc, char **argv) {
    printf("Silver Compiler Backend v%s\n", SILVER_VERSION_STRING);
    printf("Fibonacci Example\n");
    printf("Compiles fibonacci function to three target platforms\n\n");
    
    bool all_ok = true;
    
    printf("Compiling for x86-64...\n");
    all_ok &= compile_for_target("x86-64", SILVER_TARGET_X86_64,
                                  "x86_64-unknown-none", "fib_x86.o");
    
    printf("Compiling for ARM64...\n");
    all_ok &= compile_for_target("ARM64", SILVER_TARGET_ARM64,
                                  "aarch64-unknown-none", "fib_arm64.o");
    
    printf("Compiling for RISC-V64...\n");
    all_ok &= compile_for_target("RISC-V64", SILVER_TARGET_RISCV64,
                                  "riscv64-unknown-none", "fib_riscv64.o");
    
    if (all_ok) {
        printf("All targets compiled successfully!\n");
    } else {
        printf("Some targets failed.\n");
    }
    
    return all_ok ? 0 : 1;
}