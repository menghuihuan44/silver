#ifndef SILVER_TARGET_X86_H
#define SILVER_TARGET_X86_H

#include "silver/codegen/machine.h"
#include "silver/codegen/isel.h"
#include "silver/ir/ir_module.h"

#ifdef __cplusplus
extern "C" {
#endif

// x86-64特定寄存器定义（与通用寄存器枚举对应）
// 32位子寄存器
typedef enum {
    X86_REG_EAX = REG_RAX, X86_REG_ECX = REG_RCX, X86_REG_EDX = REG_RDX,
    X86_REG_EBX = REG_RBX, X86_REG_ESI = REG_RSI, X86_REG_EDI = REG_RDI,
    X86_REG_ESP = REG_RSP, X86_REG_EBP = REG_RBP,
    X86_REG_R8D = REG_R8, X86_REG_R9D = REG_R9, X86_REG_R10D = REG_R10,
    X86_REG_R11D = REG_R11, X86_REG_R12D = REG_R12, X86_REG_R13D = REG_R13,
    X86_REG_R14D = REG_R14, X86_REG_R15D = REG_R15,
} X86Reg32;

// x86-64 REX前缀
#define X86_REX_W      0x48  // 64位操作数大小
#define X86_REX_R      0x44  // 扩展ModRM.reg
#define X86_REX_X      0x42  // 扩展SIB.index
#define X86_REX_B      0x41  // 扩展ModRM.r/m, SIB.base, 或opcode reg
#define X86_REX_BASE   0x40

// ModRM字节
typedef struct {
    uint8_t mod : 2;    // 寻址模式
    uint8_t reg : 3;    // 寄存器/操作码扩展
    uint8_t rm  : 3;    // 寄存器/内存
} __attribute__((packed)) X86ModRM;

// ModRM mod字段
#define X86_MOD_DISP0   0x00  // [reg] 或 [sib]
#define X86_MOD_DISP8   0x01  // [reg + disp8]
#define X86_MOD_DISP32  0x02  // [reg + disp32]
#define X86_MOD_REG     0x03  // 寄存器直接寻址

// SIB字节
typedef struct {
    uint8_t scale : 2;  // 缩放因子 (1,2,4,8)
    uint8_t index : 3;  // 索引寄存器
    uint8_t base  : 3;  // 基址寄存器
} __attribute__((packed)) X86SIB;

// x86-64操作码
typedef enum {
    // 通用操作
    X86_NOP          = 0x90,
    X86_INT3         = 0xCC,
    
    // MOV指令
    X86_MOV_RR       = 0x89,  // mov r/m, r
    X86_MOV_MR       = 0x8B,  // mov r, r/m
    X86_MOV_RI       = 0xB8,  // mov r, imm (基础操作码)
    X86_MOV_AI       = 0xA1,  // mov eax/rax, [imm]
    
    // 算术指令
    X86_ADD_RM       = 0x03,  // add r, r/m
    X86_ADD_MR       = 0x01,  // add r/m, r
    X86_ADD_RI       = 0x81,  // add r/m, imm (group 1, /0)
    X86_SUB_RM       = 0x2B,  // sub r, r/m
    X86_SUB_MR       = 0x29,  // sub r/m, r
    X86_SUB_RI       = 0x81,  // sub r/m, imm (group 1, /5)
    X86_MUL_R        = 0xF7,  // mul r/m (group 3, /4)
    X86_IMUL_R       = 0xF7,  // imul r/m (group 3, /5)
    X86_IMUL_RR      = 0x0F,  // imul r, r/m (0x0F 0xAF)
    X86_DIV_R        = 0xF7,  // div r/m (group 3, /6)
    X86_IDIV_R       = 0xF7,  // idiv r/m (group 3, /7)
    X86_NEG_R        = 0xF7,  // neg r/m (group 3, /3)
    
    // 逻辑指令
    X86_AND_RM       = 0x23,  // and r, r/m
    X86_AND_RI       = 0x81,  // and r/m, imm (group 1, /4)
    X86_OR_RM        = 0x0B,  // or r, r/m
    X86_OR_RI        = 0x81,  // or r/m, imm (group 1, /1)
    X86_XOR_RM       = 0x33,  // xor r, r/m
    X86_XOR_RI       = 0x81,  // xor r/m, imm (group 1, /6)
    X86_NOT_R        = 0xF7,  // not r/m (group 3, /2)
    
    // 移位指令
    X86_SHL_RI       = 0xC1,  // shl r/m, imm8 (group 2, /4)
    X86_SHR_RI       = 0xC1,  // shr r/m, imm8 (group 2, /5)
    X86_SAR_RI       = 0xC1,  // sar r/m, imm8 (group 2, /7)
    X86_SHL_CL       = 0xD3,  // shl r/m, cl (group 2, /4)
    X86_SHR_CL       = 0xD3,  // shr r/m, cl (group 2, /5)
    X86_SAR_CL       = 0xD3,  // sar r/m, cl (group 2, /7)
    
    // 比较指令
    X86_CMP_RM       = 0x3B,  // cmp r, r/m
    X86_CMP_RI       = 0x81,  // cmp r/m, imm (group 1, /7)
    X86_TEST_RM      = 0x85,  // test r/m, r
    X86_TEST_RI      = 0xF7,  // test r/m, imm (group 3, /0)
    
    // 设置条件字节
    X86_SETCC        = 0x0F,  // setcc r/m8 (0x0F 0x9x)
    
    // 扩展指令前缀
    X86_ESCAPE       = 0x0F,  // 两字节操作码前缀
    X86_ESCAPE_38    = 0x38,  // 三字节操作码 (0x0F 0x38)
    X86_ESCAPE_3A    = 0x3A,  // 三字节操作码 (0x0F 0x3A)
    
    // 类型转换
    X86_MOVSX_RR     = 0x0F,  // movsx r, r/m (0x0F 0xBE/0xBF)
    X86_MOVSXD       = 0x63,  // movsxd r, r/m (64位符号扩展)
    X86_MOVZX_RR     = 0x0F,  // movzx r, r/m (0x0F 0xB6/0xB7)
    
    // 栈操作
    X86_PUSH_R       = 0x50,  // push r (基础操作码)
    X86_POP_R        = 0x58,  // pop r (基础操作码)
    X86_PUSH_IMM     = 0x68,  // push imm32
    X86_RET          = 0xC3,  // ret
    X86_RET_IMM      = 0xC2,  // ret imm16
    
    // 跳转指令
    X86_JMP_REL8     = 0xEB,  // jmp rel8
    X86_JMP_REL32    = 0xE9,  // jmp rel32
    X86_JMP_RM       = 0xFF,  // jmp r/m (group 5, /4)
    X86_JCC_REL8     = 0x70,  // jcc rel8 (基础操作码)
    X86_JCC_REL32    = 0x0F,  // jcc rel32 (0x0F 0x8x)
    
    // 调用指令
    X86_CALL_REL32   = 0xE8,  // call rel32
    X86_CALL_RM      = 0xFF,  // call r/m (group 5, /2)
    
    // LEA指令
    X86_LEA          = 0x8D,  // lea r, m
    
    // MOVSXD (符号扩展双字到四字)
    X86_MOVSXD_OP    = 0x63,
    
    // 条件移动
    X86_CMOVCC       = 0x0F,  // cmovcc r, r/m (0x0F 0x4x)
} X86Opcode;

// 创建x86-64目标
SilverTarget *x86_target_create(void);

// 销毁x86-64目标
void x86_target_destroy(SilverTarget *target);

// x86-64指令编码
bool x86_encode_instruction(const SilverTarget *target,
                             const MachineInstExt *inst,
                             uint8_t *buffer, uint32_t *length);

// x86-64函数序言
uint32_t x86_emit_prologue(const SilverTarget *target,
                            IRFunction *func, uint8_t *buffer);

// x86-64函数尾声
uint32_t x86_emit_epilogue(const SilverTarget *target,
                            IRFunction *func, uint8_t *buffer);

// x86-64匹配表（由ISel使用）
extern const MatchEntry x86_match_table[];
extern const uint32_t x86_match_table_size;

#ifdef __cplusplus
}
#endif

#endif // SILVER_TARGET_X86_H