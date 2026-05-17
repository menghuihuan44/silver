# ARM64 (AArch64) 目标平台

## 寄存器映射

| Silver | ARM64 | 用途 |
|--------|-------|------|
| REG_RAX-RCX | X0-X2 | 参数/返回值 |
| REG_RDX-RDI | X3-X5 | 参数 |
| REG_R8-R9 | X6-X7 | 参数 |
| REG_R10-R11 | X16-X17 | 临时 |
| REG_R12-R13 | X9-X10 | 被调用者保存 |
| REG_R14-R15 | X11-X12 | 被调用者保存 |
| REG_RSP | X31 (SP) | 栈指针 |
| REG_RBP | X29 (FP) | 帧指针 |
| REG_XMM0-XMM15 | V0-V15 | 浮点 |

## 调用约定

**AAPCS64**：

- 前8个整数参数：X0-X7
- 前8个浮点参数：V0-V7
- 返回值：X0（X1 用于 128 位）
- 栈对齐：16 字节
- 无红区

## 编码特点

- 定长 4 字节指令
- 统一编码格式：R-type, I-type, D-type, B-type, CB-type
- 立即数编码：12 位 + 移位
- MOVZ/MOVK 组合加载 64 位立即数
- 无标志寄存器：比较用 SUBS 设置条件码

## 支持指令

- ADD/SUB（寄存器+立即数）
- MADD（乘法）、SDIV/UDIV
- AND/ORR/EOR（位操作）
- LSL/LSR/ASR（移位）
- LDR/STR（加载/存储）
- B/BL/BR/BLR（分支/调用）
- RET
- FADD/FSUB/FMUL/FDIV（浮点）
- SCVTF/FCVTZS（浮点转换）
- NOP

## 实现文件

- `src/target/arm/arm.c`
- `src/target/arm/arm_encode.c`
- `src/target/arm/arm_isel.c`