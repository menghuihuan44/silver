# 值系统

所有操作数和结果都以 **值 ID**（`uint32_t`）的形式存在。值统一在 `IRValuePool` 中分配。

## 值的种类

| 种类 | 枚举 | 说明 |
|------|------|------|
| 常量 | `IR_VALUE_CONSTANT` | 整数或浮点字面量 |
| 指令结果 | `IR_VALUE_INSTRUCTION` | IR 指令的计算结果 |
| 函数参数 | `IR_VALUE_ARGUMENT` | 函数形式参数 |
| 基本块 | `IR_VALUE_BLOCK` | 基本块引用（用于跳转） |
| 全局变量 | `IR_VALUE_GLOBAL` | 模块级变量 |
| 函数 | `IR_VALUE_FUNCTION` | 函数引用（用于调用） |

## 创建值

### 常量

```c
IRValueId c1 = ir_value_create_int_const(&module->value_pool, i64_type, 42);
IRValueId c2 = ir_value_create_float_const(&module->value_pool, f64_type, 3.14);
IRValueId null_ptr = ir_value_create_null_ptr(&module->value_pool, ptr_type);
```

常量有去重缓存（256 项哈希表），相同值的整数常量只创建一次。

### 指令结果

通常不直接调用，由 `IRBuilder` 在生成指令时自动创建：

```c
IRValueId result = ir_builder_add(builder, lhs, rhs);
// result 对应值池中一条 IR_VALUE_INSTRUCTION，关联到 add 指令
```

### 参数

在创建函数时自动生成：

```c
IRFunction *func = ir_module_add_function(module, "my_func", func_type_id);
// func->arg_values[0], arg_values[1], ... 即参数值
```

### 基本块

```c
IRValueId block_id = ir_value_create_block(&module->value_pool, block->id);
```

## 值查询

```c
IRValue *val = ir_value_get(&module->value_pool, value_id);
bool is_const = ir_value_is_constant(&module->value_pool, value_id);
bool is_zero = ir_value_is_zero(&module->value_pool, value_id);
int64_t int_val;
bool ok = ir_value_get_int_constant(&module->value_pool, value_id, &int_val);
```

## 无效值

`IR_VALUE_ID_INVALID`（0xFFFF）表示无效或缺失的操作数。

## 内部实现

值池使用 Arena 分配，初始容量 8192。值池扩展时分配新数组并复制旧数据。每个值结构体 32 字节，包含类型 ID、值种类、具体数据和引用计数。