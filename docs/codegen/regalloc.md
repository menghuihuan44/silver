# 寄存器分配

## 算法：线性扫描

### 步骤

```
1. 计算活跃区间
2. 按起始位置排序
3. 线性扫描分配：
   - 释放已过期区间占用的寄存器
   - 尝试分配空闲寄存器
   - 若无空闲 → 选择最远结束的活跃区间溢出
```

### 活跃区间

```
指令:  0    1    2    3    4    5    6    7
值A:   [定义──────使用]
值B:        [定义──────────使用]
值C:             [定义──────使用]
值D:                         [定义──使用]
```

值 A 在指令 3 之后不再活跃，寄存器可以复用给值 D。

### 溢出

当所有寄存器都被占用时：
- 选择**结束位置最远**的活跃区间
- 生成 spill 指令（store 到栈）
- 该区间后续使用前生成 reload 指令（load 从栈）

### 寄存器分类

| 类别 | x86-64 | ARM64 | RISC-V64 |
|------|--------|-------|----------|
| 调用者保存 | RAX,RCX,RDX,RSI,RDI,R8-R11 | X0-X18 | A0-A7,T0-T6 |
| 被调用者保存 | RBX,R12-R15 | X19-X28 | S0-S11 |
| 浮点 | XMM0-XMM15 | V0-V31 | F0-F31 |
| 特殊 | RSP,RBP | SP(X31),FP(X29) | SP(X2),FP(X8) |

## PHI 节点处理

PHI 节点在寄存器分配前被**消除**（PHI Elimination）：

```
转换前:
  BB1: x = 1; br BB3
  BB2: x = 2; br BB3
  BB3: z = PHI(x_BB1, x_BB2); return z

转换后:
  BB1: x = 1; copy z, x; br BB3
  BB2: x = 2; copy z, x; br BB3
  BB3: return z
```

> 仅支持两路分支的 PHI。超过两个前驱会返回错误。

## 实现文件

- `src/codegen/isel.c` — `compute_live_intervals`, `linear_scan_allocate`