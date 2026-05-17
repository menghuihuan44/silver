# Fibonacci 示例

演示如何构建包含控制流（循环、条件分支）和多个基本块的函数，并编译到三个目标平台。

## 函数逻辑

```c
int fib(int n) {
    int a = 0, b = 1;
    for (int i = 0; i < n; i++) {
        int tmp = a + b;
        a = b;
        b = tmp;
    }
    return a;
}
```

## IR 基本块结构

```
entry:
    a = 0; b = 1; i = 0; br loop

loop:
    cond = i < n
    brcond cond → body, exit

body:
    tmp = a + b
    a = b; b = tmp; i = i + 1
    br loop

exit:
    ret a
```

## 完整代码

```c
#include "silver/silver.h"
#include "silver/ir/ir_builder.h"
#include <stdio.h>
#include <stdlib.h>

// 创建计算第 n 个斐波那契数的函数
static IRFunction *create_fib_function(IRModule *module, IRBuilder *builder) {
    // 创建函数类型: i64(i64)
    IRTypeId i64 = builder->i64_type;
    IRTypeId param_types[] = { i64 };
    IRTypeId func_type = ir_type_create_func(&module->type_table, i64, 1, param_types, false);

    IRFunction *func = ir_module_add_function(module, "fib", func_type);
    ir_builder_set_position(builder, func, NULL);

    // 获取参数 n
    IRValueId n = func->arg_values[0];

    // 创建基本块
    IRBlock *entry  = ir_builder_create_block(builder, "entry");
    IRBlock *loop   = ir_builder_create_block(builder, "loop");
    IRBlock *body   = ir_builder_create_block(builder, "body");
    IRBlock *exit_b = ir_builder_create_block(builder, "exit");

    // === entry 块 ===
    ir_builder_set_insert_point(builder, entry);
    IRValueId zero  = ir_builder_int_const(builder, i64, 0);
    IRValueId one   = ir_builder_int_const(builder, i64, 1);
    IRValueId a     = zero;                    // a = 0
    IRValueId b     = one;                     // b = 1
    IRValueId i     = zero;                    // i = 0
    ir_builder_br(builder, loop);

    // === loop 块 ===
    ir_builder_set_insert_point(builder, loop);
    IRValueId cond = ir_builder_icmp(builder, IR_OP_CMPLT, i, n);
    ir_builder_condbr(builder, cond, body, exit_b);

    // === body 块 ===
    ir_builder_set_insert_point(builder, body);
    IRValueId tmp  = ir_builder_add(builder, a, b);
    IRValueId new_a = b;                        // a = b
    IRValueId new_b = tmp;                      // b = tmp
    IRValueId new_i = ir_builder_add(builder, i, one);
    // 更新循环变量（SSA 中用新值替代旧值）
    a = new_a; b = new_b; i = new_i;
    ir_builder_br(builder, loop);

    // === exit 块 ===
    ir_builder_set_insert_point(builder, exit_b);
    ir_builder_ret(builder, a);

    return func;
}

// 为指定目标编译
static bool compile_for_target(const char *name, SilverTargetArch arch,
                                const char *triple, const char *output) {
    SilverContext *ctx = silver_context_create();
    IRModule *module = ir_module_create("fib", triple);
    IRBuilder *builder = ir_builder_create(module);
    if (!ctx || !module || !builder) return false;

    create_fib_function(module, builder);

    printf("  %s IR:\n", name);
    ir_module_dump(module, stdout);

    SilverCompileOptions opts = silver_options_default();
    opts.target_arch = arch;
    opts.target_triple = triple;
    opts.output_file = output;
    opts.verbose = false;

    bool ok = silver_compile_to_file(ctx, module, &opts);
    printf("  → %s: %s\n\n", name, ok ? "OK" : "FAILED");

    ir_builder_destroy(builder);
    ir_module_destroy(module);
    silver_context_destroy(ctx);
    if (ok) remove(output);
    return ok;
}

int main(void) {
    printf("Silver Compiler Backend v%s\n", SILVER_VERSION_STRING);
    printf("Fibonacci Example\n\n");

    bool all_ok = true;
    all_ok &= compile_for_target("x86-64", SILVER_TARGET_X86_64,
                                  "x86_64-unknown-none", "fib_x86.o");
    all_ok &= compile_for_target("ARM64", SILVER_TARGET_ARM64,
                                  "aarch64-unknown-none", "fib_arm64.o");
    all_ok &= compile_for_target("RISC-V64", SILVER_TARGET_RISCV64,
                                  "riscv64-unknown-none", "fib_riscv64.o");

    printf("%s\n", all_ok ? "All targets compiled successfully!" : "Some targets failed.");
    return all_ok ? 0 : 1;
}
```

## 关键知识点

### SSA 中的变量更新

在 SSA 形式中，变量不能被重新赋值。循环中的变量更新需要用**新值替代旧值**：

```c
// ❌ 错误（SSA 不允许）
a = b;

// ✅ 正确
IRValueId new_a = b;
a = new_a;  // 后续代码使用新的 a
```

实际的 PHI 节点在循环头部合并不同来源的值，但 Silver 当前仅支持两路分支 PHI。

### 控制流构建顺序

基本块必须按**支配树顺序**创建：先创建所有块，再填充指令。不能在创建块 B 之前引用块 B。

### 跨平台编译

同一 IR 可以编译到不同目标平台，只需改变 `target_arch`。ARM64 和 RISC-V64 的代码大小与 x86-64 相近（都在 ~590 字节左右），因为函数框架占主要部分。

## 生成的机器码（x86-64）

```
fib:
    push   %rbp
    mov    %rsp, %rbp
    mov    $0, %rax          ; a = 0
    mov    $1, %rcx          ; b = 1
    mov    $0, %rdx          ; i = 0
loop:
    cmp    %rdi, %rdx        ; i < n ?
    jge    exit
    mov    %rcx, %rsi        ; tmp = b
    add    %rax, %rsi        ; tmp = a + b
    mov    %rcx, %rax        ; a = b
    mov    %rsi, %rcx        ; b = tmp
    inc    %rdx              ; i++
    jmp    loop
exit:
    pop    %rbp
    ret
```

优化后约 40 字节机器码。