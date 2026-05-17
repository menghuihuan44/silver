# 代码生成概述

Silver 的代码生成将优化后的 IR 转换为目标机器码。

## 流水线

```
优化后的 IR
    ↓
PHI Elimination（如果存在 PHI 节点）
    ↓
活跃区间计算
    ↓
线性扫描寄存器分配
    ↓
指令选择（表驱动，集成 RA）
    ↓
机器码编码（三平台编码器）
    ↓
输出格式包装（ELF/PE/Mach-O）
```

## 关键设计

### 集成寄存器分配

指令选择时直接分配机器寄存器，不需要单独的 RA Pass：

```
ISel: 选择机器指令 → 分配结果寄存器 → 查找操作数寄存器 → 生成溢出代码
```

### 表驱动指令选择

三平台共享同一个 ISel 引擎：

```
isel_select_instruction()
    ├── x86_match_table[]   → x86_encode_instruction()
    ├── arm64_match_table[] → arm64_encode_instruction()
    └── riscv64_match_table[] → riscv64_encode_instruction()
```

### 快速编码路径

- x86-64: 使用预计算 ModRM/SIB 查找表，内联 REX 前缀生成
- ARM64/RISC-V: 直接计算 32 位指令字，一次写入 4 字节

## 性能

| 指标 | 数值 |
|------|------|
| 代码生成占比 | ~17% 总编译时间 |
| 寄存器分配 | 线性扫描，O(n log n) |
| 指令选择 | O(1) 哈希索引 + ISel 缓存 |
| 编码速度 | x86: ~5-8 ns/字节，ARM64/RISC-V: ~2 ns/字节 |

## 进一步阅读

- [指令选择详解](isel.md)
- [寄存器分配详解](regalloc.md)
- [代码发射器](emitter.md)