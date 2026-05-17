# IR 指令

## 指令格式

每条 IR 指令 **16 字节** 定长：

```
[0-1]   opcode      操作码（IROpcode）
[2-3]   type_id     结果类型（IRTypeId）
[4-5]   flags       指令标志（volatile, exact 等）
[6-7]   operand0_id 操作数0（值ID）
[8-9]   operand1_id 操作数1（值ID）
[10-11] operand2_id 操作数2（值ID）
[12-13] extra       额外数据（PHI 前驱数、CALL 参数数等）
[14-15] original_order 原始顺序编号
```

## 操作码分类

### 终结指令

| 操作码 | 操作数 | 说明 |
|--------|--------|------|
| `ret` | 返回值 | 函数返回 |
| `br` | 目标块 | 无条件分支 |
| `brcond` | 条件, 真块, 假块 | 条件分支 |
| `switch` | 值, 默认块 | 多路分支（extra=case数） |
| `unreachable` | — | 不可达代码标记 |

### 算术指令

| 操作码 | 说明 |
|--------|------|
| `add` | 整数加法 |
| `sub` | 整数减法 |
| `mul` | 整数乘法 |
| `div` | 有符号除法 |
| `udiv` | 无符号除法 |
| `rem` | 有符号取余 |
| `urem` | 无符号取余 |

### 位操作

| 操作码 | 说明 |
|--------|------|
| `and` | 按位与 |
| `or` | 按位或 |
| `xor` | 按位异或 |
| `shl` | 左移 |
| `lshr` | 逻辑右移 |
| `ashr` | 算术右移 |

### 比较指令

| 操作码 | 说明 |
|--------|------|
| `cmpeq` / `cmpne` | 等于 / 不等于 |
| `cmplt` / `cmple` | 有符号小于 / 小于等于 |
| `cmpgt` / `cmpge` | 有符号大于 / 大于等于 |
| `cmpult` / `cmpule` | 无符号小于 / 小于等于 |
| `cmpugt` / `cmpuge` | 无符号大于 / 大于等于 |

### 浮点指令

| 操作码 | 说明 |
|--------|------|
| `fadd` / `fsub` / `fmul` / `fdiv` | 浮点算术 |
| `fneg` | 浮点取负 |
| `fcmpoeq` - `fcmpuge` | 有序/无序浮点比较 |

### 内存指令

| 操作码 | 说明 |
|--------|------|
| `load` | 从内存加载 |
| `store` | 存储到内存 |
| `alloca` | 栈上分配 |
| `gep` | 指针偏移（GetElementPtr） |

### 类型转换

| 操作码 | 说明 |
|--------|------|
| `trunc` | 整数截断 |
| `zext` | 零扩展 |
| `sext` | 符号扩展 |
| `fptosi` / `fptoui` | 浮点→整数 |
| `sitofp` / `uitofp` | 整数→浮点 |
| `fptrunc` / `fpext` | 浮点精度转换 |
| `bitcast` | 位模式重解释 |

### 特殊指令

| 操作码 | 说明 |
|--------|------|
| `phi` | SSA PHI 节点（支持两路分支） |
| `call` | 函数调用 |
| `select` | 条件选择 |
| `copy` | 值复制（用于寄存器分配） |

## 标志位

| 标志 | 说明 |
|------|------|
| `IR_FLAG_NNSW` | 无有符号溢出 |
| `IR_FLAG_NUW` | 无无符号溢出 |
| `IR_FLAG_EXACT` | 精确除法 |
| `IR_FLAG_VOLATILE` | 易变内存操作 |
| `IR_FLAG_ATOMIC` | 原子操作 |
| `IR_FLAG_ALIGNED` | 对齐访问 |

## 副作用

以下指令被认为有副作用（不会被 DCE 消除）：

- `store`, `call`, `ret`, `br`, `brcond`, `switch`, `unreachable`
- `div`, `udiv`, `rem`, `urem`（除零可能异常）
- 所有原子指令