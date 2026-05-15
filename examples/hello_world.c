/**
 * Silver Compiler Backend - Hello World Example
 * 
 * 演示如何使用 Silver API 创建 IR 模块并编译为 x86-64 ELF 目标文件。
 * 
 * 编译:
 *   gcc -I include -o example_hello examples/hello_world.c -L build -lsilver
 * 
 * 运行:
 *   ./example_hello
 * 
 * 输出:
 *   生成 hello.o (x86-64 ELF 目标文件)
 */

#include "silver/silver.h"
#include "silver/ir/ir_builder.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    printf("Silver Compiler Backend v%s\n", SILVER_VERSION_STRING);
    printf("Hello World Example\n\n");
    
    // ================================================================
    // 1. 创建编译上下文
    // ================================================================
    SilverContext *ctx = silver_context_create();
    if (!ctx) {
        fprintf(stderr, "Failed to create compilation context\n");
        return 1;
    }
    
    // ================================================================
    // 2. 创建 IR 模块
    // ================================================================
    IRModule *module = ir_module_create("hello", "x86_64-unknown-none");
    if (!module) {
        fprintf(stderr, "Failed to create IR module\n");
        silver_context_destroy(ctx);
        return 1;
    }
    
    // ================================================================
    // 3. 使用 IR Builder 构建函数
    // ================================================================
    IRBuilder *builder = ir_builder_create(module);
    if (!builder) {
        fprintf(stderr, "Failed to create IR builder\n");
        ir_module_destroy(module);
        silver_context_destroy(ctx);
        return 1;
    }
    
    // 创建一个简单的函数: int hello(void) { return 42; }
    IRFunction *func = ir_module_add_function(module, "hello", 0);
    ir_builder_set_position(builder, func, NULL);
    
    // 创建基本块
    IRBlock *entry = ir_builder_create_block(builder, "entry");
    ir_builder_set_insert_point(builder, entry);
    
    // 生成 IR 指令: return 42
    IRValueId const_42 = ir_builder_int_const(builder, builder->i64_type, 42);
    ir_builder_ret(builder, const_42);
    
    printf("IR Module created:\n");
    printf("  Function: hello\n");
    printf("  Returns: 42\n\n");
    
    // 打印 IR（调试用）
    ir_module_dump(module, stdout);
    
    // ================================================================
    // 4. 编译到目标文件
    // ================================================================
    SilverCompileOptions options = silver_options_default();
    options.target_arch = SILVER_TARGET_X86_64;
    options.output_file = "hello.o";
    options.object_format = SILVER_FORMAT_ELF;
    options.verbose = true;
    
    printf("\nCompiling to %s...\n", options.output_file);
    
    bool success = silver_compile_to_file(ctx, module, &options);
    
    if (success) {
        printf("\nSuccess! Generated hello.o (x86-64 ELF object file)\n");
        printf("You can link it with:\n");
        printf("  ld hello.o -o hello\n");
        printf("  gcc hello.o -o hello\n");
    } else {
        printf("\nCompilation failed.\n");
    }
    
    // ================================================================
    // 5. 清理
    // ================================================================
    ir_builder_destroy(builder);
    ir_module_destroy(module);
    silver_context_destroy(ctx);
    
    return success ? 0 : 1;
}