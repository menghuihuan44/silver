# 类型系统

## 预定义类型

| ID | 名称 | C 等价 | 大小 |
|----|------|--------|------|
| 0 | void | void | 0 |
| 1 | i1 | _Bool | 1 |
| 2 | i8 | int8_t | 1 |
| 3 | i16 | int16_t | 2 |
| 4 | i32 | int32_t | 4 |
| 5 | i64 | int64_t | 8 |
| 6 | u8 | uint8_t | 1 |
| 7 | u16 | uint16_t | 2 |
| 8 | u32 | uint32_t | 4 |
| 9 | u64 | uint64_t | 8 |
| 10 | f32 | float | 4 |
| 11 | f64 | double | 8 |
| 12 | ptr | void* | 8 |
| 13 | label | — | 0 |
| 14 | token | — | 0 |

## 动态创建类型

### 指针类型

```c
IRTypeId ptr_type = ir_type_create_ptr(&module->type_table, pointee_type);
```

### 数组类型

```c
IRTypeId arr_type = ir_type_create_array(&module->type_table, elem_type, count);
```

### 函数类型

```c
IRTypeId param_types[] = { i64_type, i64_type };
IRTypeId func_type = ir_type_create_func(&module->type_table, return_type, 2, param_types, false);
```

### 结构体类型

```c
IRTypeId field_types[] = { i32_type, i64_type, f64_type };
IRTypeId struct_type = ir_type_create_struct(&module->type_table, "Point", 3, field_types);
```

## 类型查询

```c
IRType *type = ir_type_get(&module->type_table, type_id);
uint32_t size = ir_type_size(&module->type_table, type_id);
uint32_t align = ir_type_alignment(&module->type_table, type_id);
bool is_int = ir_type_is_integer(&module->type_table, type_id);
bool is_float = ir_type_is_float(&module->type_table, type_id);
IRTypeKind kind = ir_type_get_kind(&module->type_table, type_id);
```

## 内部实现

类型存储在 `IRTypeTable` 中，使用 Arena 分配。初始容量 128，按需扩展（2× 增长）。类型 ID 从 0 开始自增。