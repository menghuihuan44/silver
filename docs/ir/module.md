# IR 模块

IR 模块是编译的基本单元，包含函数、全局变量和类型信息。每个模块对应一个 `.o` 目标文件。

## 创建和销毁

```c
IRModule *module = ir_module_create("my_module", "x86_64-unknown-none");

// ... 使用模块 ...

ir_module_destroy(module);
```

`ir_module_create` 参数：
- `name`：模块名称，用于调试信息和符号表
- `target_triple`：目标三元组，如 `"x86_64-unknown-none"`, `"aarch64-unknown-none"`, `"riscv64-unknown-none"`

## 模块结构

```
IRModule
├── arena          # Arena 分配器（模块级生命周期）
├── error_ctx      # 错误上下文
├── type_table     # 类型表（15个预定义类型 + 动态创建）
├── value_pool     # 值池（常量、指令结果、参数等）
├── functions[]    # 函数数组
├── globals[]      # 全局变量数组
├── module_name    # 模块名称
├── target_triple  # 目标三元组
├── data_layout    # 数据布局字符串
└── stats          # 编译统计（指令总数、编译时间等）
```

## 函数管理

### 添加函数

```c
IRFunction *func = ir_module_add_function(module, "my_func", func_type_id);
```

- `name`：函数名（会出现在符号表中）
- `func_type_id`：函数类型ID，`0` 表示最简单的函数签名（无参数、无返回值类型）

### 查找函数

```c
IRFunction *f1 = ir_module_get_function(module, 0);          // 按ID
IRFunction *f2 = ir_module_find_function(module, "my_func"); // 按名称
```

### 函数结构

```c
typedef struct IRFunction {
    uint32_t id;
    const char *name;
    IRTypeId func_type_id;
    uint32_t num_args;
    IRValueId *arg_values;       // 参数值ID数组
    IRBlock *blocks;             // 基本块数组
    uint32_t num_blocks;
    IRInst *instructions;        // 指令数组（所有块连续存储）
    uint32_t num_insts;
    IRValueId *inst_to_value;    // 指令索引→值ID映射
    uint32_t stack_size;
    uint32_t flags;
} IRFunction;
```

## 全局变量管理

### 添加全局变量

```c
IRValueId init_val = ir_value_create_int_const(&module->value_pool, i64_type, 0);
IRGlobal *global = ir_module_add_global(module, "my_global", i64_type, init_val);
```

### 查找全局变量

```c
IRGlobal *g1 = ir_module_get_global(module, 0);           // 按ID
IRGlobal *g2 = ir_module_find_global(module, "my_global"); // 按名称
```

### 全局变量结构

```c
typedef struct IRGlobal {
    uint32_t id;
    const char *name;
    IRTypeId type_id;
    IRValueId init_value;   // IR_VALUE_ID_INVALID 表示未初始化
    uint32_t alignment;
    uint32_t flags;
    uint64_t size;
} IRGlobal;
```

## 基本块管理

### 添加基本块

```c
IRBlock *block = ir_function_add_block(func, "block_name");
```

- `name`：块名称，用于调试，可为 `NULL`

### 块结构

```c
typedef struct IRBlock {
    uint32_t id;
    uint32_t first_inst;         // 第一条指令索引
    uint32_t last_inst;          // 最后一条指令索引
    uint32_t num_predecessors;   // 前驱块数量
    uint32_t *predecessors;      // 前驱块ID数组
    uint32_t num_successors;     // 后继块数量
    uint32_t *successors;        // 后继块ID数组
    const char *name;
    uint16_t flags;
    bool is_sealed;              // 是否已封闭（不再添加指令）
} IRBlock;
```

块的 `first_inst` 和 `last_inst` 由 IRBuilder 自动维护。`IR_VALUE_ID_INVALID` 表示空块。

### 块标志

| 标志 | 说明 |
|------|------|
| `IR_BLOCK_ENTRY` | 函数入口块 |
| `IR_BLOCK_EXIT` | 函数出口块 |
| `IR_BLOCK_LOOP_HEADER` | 循环头块 |

## 指令管理

### 添加指令（底层API）

```c
IRInst *inst = ir_function_add_inst(func, IR_OP_ADD, i64_type, 0,
                                     lhs_id, rhs_id, IR_VALUE_ID_INVALID, 0);
```

通常不直接使用，而是通过 IRBuilder 的高级接口。

### 指令数组

所有基本块的指令**连续存储**在一个数组中：

```
instructions[0..2]   ← block 0 的指令
instructions[3..5]   ← block 1 的指令
instructions[6..8]   ← block 2 的指令
```

块的 `first_inst` 和 `last_inst` 索引到这个数组中。

### 容量扩展

指令数组初始容量 8192，按需 2× 扩展。扩展时使用 Arena 分配新数组并复制旧数据。

## 模块验证

```c
bool valid = ir_module_verify(module);
```

检查内容：
- 所有基本块的指令范围是否有效
- 终结指令是否正确（每个封闭块的最后一条指令必须是终结指令）
- 全局变量的类型ID是否有效
- `first_inst <= last_inst`

## 模块转储

```c
ir_module_dump(module, stdout);
```

以人类可读格式输出：
- 目标三元组和数据布局
- 所有类型定义
- 所有全局变量
- 所有函数的完整 IR（基本块和指令）
- 编译统计

## 统计信息

```c
char buf[512];
ir_module_get_stats(module, buf, sizeof(buf));
printf("%s\n", buf);
```

输出：
```
Module: my_module
  Functions: 2
  Globals: 1
  Total Instructions: 1042
  Total Values: 2083
  Total Types: 16
  Arena: 128.5 KB used / 256.0 KB allocated (50.2% utilization)
  Errors: 0, Warnings: 0
```

## 内存管理

模块使用 Arena 分配器：
- 所有数据结构（类型、值、指令、基本块）都在 Arena 中分配
- `ir_module_destroy` 一次性释放所有内存
- 编译过程中不调用 `malloc`/`free`（除了 Arena 块扩展）

## 完整示例

```c
IRModule *module = ir_module_create("example", "x86_64-unknown-none");

// 添加函数
IRFunction *func = ir_module_add_function(module, "main", 0);
IRBlock *block = ir_function_add_block(func, "entry");
IRInst *ret_inst = ir_function_add_inst(func, IR_OP_RET, 0, 0,
    ir_value_create_int_const(&module->value_pool, module->type_table.i64_type, 0),
    IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
ir_block_update_range(block, 0, 0);

// 添加全局变量
ir_module_add_global(module, "counter", module->type_table.i64_type,
    ir_value_create_int_const(&module->value_pool, module->type_table.i64_type, 0));

// 验证
if (ir_module_verify(module)) {
    printf("Valid!\n");
}

// 查看
ir_module_dump(module, stdout);

ir_module_destroy(module);
```