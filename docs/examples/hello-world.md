# Hello World 示例

演示如何使用 Silver API 创建一个返回固定值的函数，并编译为 x86-64 ELF 目标文件。

## 完整代码

```c
#include "silver/silver.h"
#include "silver/ir/ir_builder.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    printf("Silver Compiler Backend v%s\n", SILVER_VERSION_STRING);
    printf("Hello World Example\n\n");

    // 1. 创建编译上下文
    SilverContext *ctx = silver_context_create();
    if (!ctx) {
        fprintf(stderr, "Failed to create compilation context\n");
        return 1;
    }

    // 2. 创建 IR 模块
    IRModule *module = ir_module_create("hello", "x86_64-unknown-none");
    if (!module) {
        fprintf(stderr, "Failed to create IR module\n");
        silver_context_destroy(ctx);
        return 1;
    }

    // 3. 创建 IR Builder
    IRBuilder *builder = ir_builder_create(module);
    if (!builder) {
        fprintf(stderr, "Failed to create IR builder\n");
        ir_module_destroy(module);
        silver_context_destroy(ctx);
        return 1;
    }

    // 4. 创建函数: int hello(void) { return 42; }
    IRFunction *func = ir_module_add_function(module, "hello", 0);
    ir_builder_set_position(builder, func, NULL);

    // 5. 创建基本块
    IRBlock *entry = ir_builder_create_block(builder, "entry");
    ir_builder_set_insert_point(builder, entry);

    // 6. 生成 IR 指令
    IRValueId const_42 = ir_builder_int_const(builder, builder->i64_type, 42);
    ir_builder_ret(builder, const_42);

    // 7. 打印生成的 IR（调试用）
    printf("Generated IR:\n");
    ir_module_dump(module, stdout);

    // 8. 编译到目标文件
    SilverCompileOptions options = silver_options_default();
    options.target_arch = SILVER_TARGET_X86_64;
    options.output_file = "hello.o";
    options.object_format = SILVER_FORMAT_ELF;
    options.verbose = true;

    printf("\nCompiling to %s...\n", options.output_file);
    bool success = silver_compile_to_file(ctx, module, &options);

    if (success) {
        printf("\n✓ Generated hello.o (x86-64 ELF object file)\n");
        printf("  Link with: ld hello.o -o hello\n");
    } else {
        printf("\n✗ Compilation failed.\n");
    }

    // 9. 清理
    ir_builder_destroy(builder);
    ir_module_destroy(module);
    silver_context_destroy(ctx);

    return success ? 0 : 1;
}
```

## 编译和运行

```bash
# 编译示例
gcc -I/usr/local/include -o hello_example hello_example.c -lsilver

# 运行
./hello_example

# 输出:
# Silver Compiler Backend v0.1.0
# Hello World Example
#
# Generated IR:
# ;; Silver IR Module: hello
# ;; Target: x86_64-unknown-none
# ...
# define void @hello() {
# BB0:
#   ret 42
# }
#
# Compiling to hello.o...
# Compilation completed in 0.02 ms
# Code size: 572 bytes
#
# ✓ Generated hello.o (x86-64 ELF object file)

# 验证目标文件
file hello.o
# 输出: hello.o: ELF 64-bit LSB relocatable, x86-64

# 反汇编
objdump -d hello.o
# 输出:
# 0000000000000000 <hello>:
#    0: 55           push %rbp
#    1: 48 89 e5     mov  %rsp,%rbp
#    4: 48 c7 c0 2a  mov $0x2a,%rax
#    b: 00 00 00
#    e: 5d           pop  %rbp
#    f: c3           ret
```

## 代码解析

### 第1步：创建上下文

`SilverContext` 管理编译会话。它缓存目标平台信息，避免重复创建。

### 第2步：创建模块

`ir_module_create` 的第一个参数是模块名称（用于调试信息），第二个参数是目标三元组。

### 第3步：创建 Builder

`IRBuilder` 是构建 IR 的高级接口。它维护指令到值 ID 的映射。

### 第4-6步：构建函数

- `ir_module_add_function` 创建函数。第三个参数是函数类型 ID，`0` 表示最简单的函数签名。
- `ir_builder_create_block` 创建基本块。
- `ir_builder_int_const` 创建整数常量。
- `ir_builder_ret` 生成返回指令。

### 第7步：编译

`silver_compile_to_file` 运行完整的编译流水线（优化 → 代码生成 → ELF 输出）。

## 生成的机器码分析

```
push %rbp          ; 保存帧指针
mov  %rsp, %rbp    ; 设置新帧指针
mov  $0x2a, %rax   ; 将 42 (0x2a) 放入返回值寄存器 RAX
pop  %rbp          ; 恢复帧指针
ret                ; 返回
```

5 条指令，572 字节（包含 ELF 头和节头）。实际机器码只有 11 字节。