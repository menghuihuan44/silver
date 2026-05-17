# IR 构建器

`IRBuilder` 是构建 IR 的高级接口，隐藏了指令创建、值 ID 映射等底层细节。

## 基本用法

```c
IRBuilder *builder = ir_builder_create(module);

// 设置当前位置
ir_builder_set_position(builder, func, NULL);

// 创建基本块
IRBlock *entry = ir_builder_create_block(builder, "entry");
ir_builder_set_insert_point(builder, entry);

// 生成指令
IRValueId c1 = ir_builder_int_const(builder, builder->i64_type, 42);
IRValueId c2 = ir_builder_int_const(builder, builder->i64_type, 10);
IRValueId sum = ir_builder_add(builder, c1, c2);
ir_builder_ret(builder, sum);

ir_builder_destroy(builder);
```

## 常量创建

```c
// 整数常量
IRValueId c = ir_builder_int_const(builder, builder->i32_type, 42);

// 浮点常量
IRValueId f = ir_builder_float_const(builder, builder->f64_type, 3.14);

// 特殊常量
IRValueId zero = ir_builder_null(builder, ptr_type);
IRValueId t = ir_builder_true(builder);    // i1 = 1
IRValueId f = ir_builder_false(builder);   // i1 = 0
```

## 算术运算

```c
IRValueId sum = ir_builder_add(builder, lhs, rhs);
IRValueId diff = ir_builder_sub(builder, lhs, rhs);
IRValueId prod = ir_builder_mul(builder, lhs, rhs);
IRValueId quot = ir_builder_div(builder, lhs, rhs, true);  // 有符号
IRValueId rem = ir_builder_rem(builder, lhs, rhs, false);  // 无符号
IRValueId neg = ir_builder_neg(builder, val);
```

## 位操作

```c
IRValueId and_val = ir_builder_and(builder, lhs, rhs);
IRValueId or_val = ir_builder_or(builder, lhs, rhs);
IRValueId xor_val = ir_builder_xor(builder, lhs, rhs);
IRValueId shl_val = ir_builder_shl(builder, lhs, rhs);
IRValueId shr_val = ir_builder_lshr(builder, lhs, rhs);
IRValueId sar_val = ir_builder_ashr(builder, lhs, rhs);
```

## 比较操作

```c
IRValueId cond = ir_builder_icmp(builder, IR_OP_CMPEQ, lhs, rhs);
IRValueId fcond = ir_builder_fcmp(builder, IR_OP_FCMPOLT, lhs, rhs);
```

## 类型转换

```c
IRValueId truncated = ir_builder_trunc(builder, val, i32_type);
IRValueId extended = ir_builder_zext(builder, val, i64_type);
IRValueId fp = ir_builder_sitofp(builder, val, f64_type);
```

## 控制流

```c
// 返回
ir_builder_ret(builder, val);
ir_builder_ret_void(builder);

// 无条件跳转
ir_builder_br(builder, target_block);

// 条件分支
ir_builder_condbr(builder, cond, true_block, false_block);

// PHI 节点（仅支持两路分支）
IRValueId incoming_vals[] = { val_from_block1, val_from_block2 };
IRBlock *incoming_blks[] = { block1, block2 };
IRValueId phi = ir_builder_phi(builder, type, 2, incoming_vals, incoming_blks);
```

## 内存操作

```c
// 栈分配
IRValueId ptr = ir_builder_alloca(builder, i64_type, 8);

// 加载/存储
IRValueId val = ir_builder_load(builder, i64_type, ptr);
ir_builder_store(builder, val, ptr);

// 指针偏移
IRValueId elem_ptr = ir_builder_gep(builder, ptr_type, base, offset);
```

## 函数调用

```c
IRValueId func_ref = ir_value_create_function(&module->value_pool, 0, "target_func");
IRValueId result = ir_builder_call(builder, i64_type, func_ref, 2, arg1, arg2);
```

## 内部机制

IR 构建器维护一个 `inst_to_value` 映射表，记录每条指令对应的结果值 ID。所有指令创建函数返回的值 ID 都是通过这个映射表获取的，确保同一指令始终返回相同的值 ID。