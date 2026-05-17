# 指令选择

## 工作原理

指令选择将每条 IR 指令映射为一条或多条机器指令。

### 匹配表

每个平台有一个 `MatchEntry` 数组，定义 IR→机器指令的映射：

```c
typedef struct {
    IROpcode ir_opcode;              // IR 操作码
    uint8_t operand_constraints[3];  // 操作数约束（REG/IMM/MEM/LABEL）
    uint8_t operand_reg_class[3];    // 寄存器类别（GPR/FPR）
    MachineOpcode mach_opcode;       // 机器操作码
    int8_t operand_mapping[3];       // IR操作数→机器操作数映射
    uint8_t result_reg_class;        // 结果寄存器类别
    uint16_t flags;                  // COMMUTATIVE, HAS_IMM_VARIANT 等
    uint32_t cost;                   // 代价
} MatchEntry;
```

### 查找过程

```
1. 检查 ISel 缓存（同类型指令直接命中）
2. 使用 opcode 索引数组（O(1) 定位到匹配项列表）
3. 遍历同 opcode 的匹配项，选择第一个满足操作数约束的
4. 缓存此次匹配结果
```

### 缓存

ISel 缓存最多 32 项，按 (opcode, type_id, flags) 索引。编译一个函数时，重复的指令模式（如多条 ret）直接从缓存获取。

### 常量处理

如果操作数是常量且匹配项有立即数变体（`MATCH_HAS_IMM_VARIANT`），ISel 自动选择立即数版本，将常量值存入 `extended_imm` 字段。

## 操作数分配

```
对每个操作数:
  1. 如果是常量 → 选择立即数变体，存入 extended_imm
  2. 如果已有寄存器映射 → 直接使用
  3. 否则 → 分配新寄存器（可能触发溢出）
```

## 结果寄存器分配

指令产生结果时，通过 `inst_to_value` 映射找到结果值 ID，分配寄存器并记录映射。

## 多指令展开

一条 IR 指令可能展开为多条机器指令。例如 x86 上 `idiv` 需要先 `cdq` 再 `idiv`。

## 实现文件

- `src/codegen/isel.c`
- `src/target/x86/x86_isel.c` — x86-64 匹配表
- `src/target/arm/arm_isel.c` — ARM64 匹配表
- `src/target/riscv/riscv_isel.c` — RISC-V64 匹配表