# IR 系统概述

Silver 使用 **SSA（Static Single Assignment）** 形式的中间表示。所有值只能被定义一次，每个变量在使用前都有唯一的定义点。

## 设计原则

- **定长指令**：每条 IR 指令 16 字节，数组存储，缓存友好
- **值池管理**：所有 SSA 值和常量统一在值池中分配
- **Arena 分配**：整个模块使用 Arena 分配器，编译完成一次性释放

## IR 模块结构

```
IRModule
├── TypeTable       # 类型系统（15个预定义类型 + 动态创建）
├── ValuePool       # 值池（常量、指令结果、参数、基本块）
├── IRFunction[]    # 函数数组
│   ├── IRBlock[]   # 基本块数组
│   │   └── 指令范围（first_inst .. last_inst）
│   └── IRInst[]    # 指令数组（所有块的指令连续存储）
└── IRGlobal[]      # 全局变量
```

## 核心概念

### 类型系统

Silver 有 15 个预定义类型（void, i1, i8, i16, i32, i64, u8, u16, u32, u64, f32, f64, ptr, label, token），并按需支持动态创建（函数类型、结构体类型、数组类型等）。详见 [类型系统](types.md)。

### 值系统

所有操作数和结果都是值 ID（`uint32_t`），在值池中统一管理。值的种类包括：

- 常量（整数、浮点）
- 指令结果
- 函数参数
- 基本块引用
- 全局变量引用
- 函数引用

详见 [值系统](values.md)。

### 指令

50+ 种 IR 操作码，涵盖：
- 终结指令（ret, br, brcond, switch）
- 算术指令（add, sub, mul, div, rem）
- 位操作（and, or, xor, shl, lshr, ashr）
- 比较指令（整数比较、浮点比较）
- 内存指令（load, store, alloca, gep）
- 类型转换（trunc, zext, sext, fptosi, sitofp 等）
- 原子指令、PHI 节点、函数调用

详见 [指令定义](instructions.md)。

### SSA 构建

IR 构建器（`IRBuilder`）负责创建指令并管理值 ID 映射。每条产生结果的指令会自动在值池中创建对应的 `IR_VALUE_INSTRUCTION` 值。

## 进一步阅读

- [类型系统详解](types.md)
- [值系统详解](values.md)
- [指令定义](instructions.md)
- [IR 构建器使用](builder.md)
- [模块管理](module.md)