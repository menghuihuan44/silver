# 快速上手

本章通过一个简单示例，展示如何使用 Silver API 创建 IR 模块并将其编译为目标文件。

## 完整示例

```c
#include "silver/silver.h"
#include "silver/ir/ir_builder.h"
#include <stdio.h>

int main() {
    // 1. 创建编译上下文
    SilverContext *ctx = silver_context_create();
    if (!ctx) { fprintf(stderr, "Failed to create context\n"); return 1; }

    // 2. 创建 IR 模块
    IRModule *module = ir_module_create("hello", "x86_64-unknown-none");
    IRBuilder *builder = ir_builder_create(module);
    if (!module || !builder) { goto cleanup; }

    // 3. 创建函数
    IRFunction *func = ir_module_add_function(module, "hello", 0);
    ir_builder_set_position(builder, func, NULL);

    // 4. 创建基本块
    IRBlock *entry = ir_builder_create_block(builder, "entry");
    ir_builder_set_insert_point(builder, entry);

    // 5. 生成 IR 指令
    IRValueId const_42 = ir_builder_int_const(builder, builder->i64_type, 42);
    ir_builder_ret(builder, const_42);

    // 6. 编译到目标文件
    SilverCompileOptions opts = silver_options_default();
    opts.target_arch = SILVER_TARGET_X86_64;
    opts.output_file = "hello.o";

    bool ok = silver_compile_to_file(ctx, module, &opts);
    printf("Compilation %s\n", ok ? "succeeded" : "failed");

cleanup:
    ir_builder_destroy(builder);
    ir_module_destroy(module);
    silver_context_destroy(ctx);
    return ok ? 0 : 1;
}
```

## 编译和运行

```bash
gcc -I/usr/local/include -o hello_example hello_example.c -lsilver
./hello_example

# 验证输出文件
file hello.o        # ELF 64-bit LSB relocatable, x86-64
objdump -d hello.o  # 反汇编查看生成的机器码
```

## 核心步骤解析

### 1. 创建编译上下文

`SilverContext` 管理整个编译会话，包括目标平台选择和缓存。

### 2. 创建 IR 模块

IR 模块是编译的基本单元，包含函数、全局变量和类型信息。

### 3. 构建 IR

使用 `IRBuilder` 创建函数、基本块和指令。Silver 的 IR 是 SSA 形式 —— 每个值只能被定义一次。

### 4. 编译

`silver_compile` 返回编译结果，`silver_compile_to_file` 直接写入文件。

## 下一步

- 了解 [命令行工具](command-line.md) 的用法
- 深入学习 [IR 系统](../ir/overview.md)
- 查看 [更多示例](../examples/hello-world.md)