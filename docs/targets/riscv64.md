# RISC-V64 目标平台

## 寄存器映射

| Silver | RISC-V | 用途 |
|--------|--------|------|
| REG_RAX-RCX | A0-A2 (X10-X12) | 参数/返回值 |
| REG_RBX-RDI | A3-A5 (X13-X15) | 参数 |
| REG_R8-R9 | A6-A7 (X16-X17) | 参数 |
| REG_R10-R12 | T0-T2 (X5-X7) | 临时 |
| REG_R13-R14 | S0-S1 (X8-X9) | 被调用者保存 |
| REG_R15 | S2 (X18) | 被调用者保存 |
| REG_RSP | SP (X2) | 栈指针 |
| REG_RBP | FP (X8) | 帧指针 |
| REG_XMM0-XMM15 | F0-F15 | 浮点 |

## 调用约定

**RISC-V Calling Convention**：

- 前8个整数参数：A0-A7
- 前8个浮点参数：F0-F7
- 返回值：A0（A1 用于 128 位）
- 栈对齐：16 字节
- 无红区

## 编码特点

- 定长 4 字节指令
- R/I/S/B/U/J 六种指令格式
- 32 个通用寄存器（X0=零寄存器, X1=RA, X2=SP, X8=FP）
- 伪指令：MV, LI, CALL, RET, J, NOP
- 条件分支：BEQ, BNE, BLT, BGE, BLTU, BGEU（需反转条件的用相反操作数）

## 支持指令

- ADD/SUB/SLT/SLTU（整数算术）
- MUL/DIV/DIVU/REM/REMU（M 扩展）
- AND/OR/XOR（位操作）
- SLL/SRL/SRA（移位）
- ADDI/SLTI/ANDI/ORI/XORI（立即数）
- LD/SD（加载/存储 64 位）
- BEQ/BNE/BLT/BGE/BLTU/BGEU（分支）
- JAL/JALR（跳转）
- FADD.D/FSUB.D/FMUL.D/FDIV.D（双精度浮点，D 扩展）
- FCVT.D.L/FCVT.L.D（浮点转换）

## 实现文件

- `src/target/riscv/riscv.c`
- `src/target/riscv/riscv_encode.c`
- `src/target/riscv/riscv_isel.c`