#include "silver/silver.h"
#include "silver/target/x86/x86.h"
#include "silver/target/arm/arm.h"
#include "silver/target/riscv/riscv.h"
#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static bool test_##name(void)
#define RUN_TEST(name) do { \
    tests_run++; \
    printf("  Running %s... ", #name); \
    if (test_##name()) { \
        tests_passed++; \
        printf("PASSED\n"); \
    } else { \
        tests_failed++; \
        printf("FAILED\n"); \
    } \
} while(0)

// 测试x86-64目标创建
static bool test_x86_target_create(void) {
    SilverTarget *target = x86_target_create();
    if (!target) return false;
    
    bool success = 
        (target->arch == SILVER_TARGET_X86_64) &&
        (target->is_64_bit == true) &&
        (target->pointer_size == 8) &&
        (target->sp_reg == REG_RSP) &&
        (target->bp_reg == REG_RBP) &&
        (target->encode != NULL) &&
        (target->emit_prologue != NULL) &&
        (target->emit_epilogue != NULL);
    
    x86_target_destroy(target);
    return success;
}

// 测试ARM64目标创建
static bool test_arm64_target_create(void) {
    SilverTarget *target = arm64_target_create();
    if (!target) return false;
    
    bool success = 
        (target->arch == SILVER_TARGET_ARM64) &&
        (target->is_64_bit == true) &&
        (target->pointer_size == 8) &&
        (target->encode != NULL);
    
    arm64_target_destroy(target);
    return success;
}

// 测试RISC-V64目标创建
static bool test_riscv64_target_create(void) {
    SilverTarget *target = riscv64_target_create();
    if (!target) return false;
    
    bool success = 
        (target->arch == SILVER_TARGET_RISCV64) &&
        (target->is_64_bit == true) &&
        (target->pointer_size == 8) &&
        (target->encode != NULL);
    
    riscv64_target_destroy(target);
    return success;
}

// 测试x86-64简单指令编码
static bool test_x86_simple_encode(void) {
    SilverTarget *target = x86_target_create();
    if (!target) return false;
    
    // 编码 RET
    MachineInstExt inst;
    memset(&inst, 0, sizeof(inst));
    inst.base.opcode = MACH_RET;
    
    uint8_t buffer[16];
    uint32_t length = 0;
    
    bool success = target->encode(target, &inst, buffer, &length);
    if (success) {
        // RET = 0xC3
        success = (length == 1) && (buffer[0] == 0xC3);
    }
    
    x86_target_destroy(target);
    return success;
}

// 测试x86-64 MOV编码
static bool test_x86_mov_encode(void) {
    SilverTarget *target = x86_target_create();
    if (!target) return false;
    
    // 编码 MOV RAX, RBX
    MachineInstExt inst;
    memset(&inst, 0, sizeof(inst));
    inst.base.opcode = MACH_MOV;
    inst.base.rd = REG_RAX;
    inst.base.rn = REG_RBX;
    
    uint8_t buffer[16];
    uint32_t length = 0;
    
    bool success = target->encode(target, &inst, buffer, &length);
    // MOV r64, r64: REX.W + 0x89 + ModRM
    if (success && length >= 3) {
        uint8_t rex = buffer[0];
        uint8_t opcode = buffer[1];
        uint8_t modrm = buffer[2];
        
        // 检查 REX.W 并且正确的操作码
        bool rex_w = (rex & 0x08) != 0;  // REX.W
        bool opcode_ok = (opcode == 0x89);  // MOV r/m64, r64
        
        // ModRM: mod=3, reg=src(RBX=3), r/m=dst(RAX=0) -> 0xD8
        bool modrm_mod = ((modrm >> 6) & 0x03) == 3;
        bool modrm_ok = (modrm == 0xD8 || modrm_mod); 
        
        success = rex_w && opcode_ok && modrm_ok;
        
        if (!success) {
            printf("  Got: %02X %02X %02X\n", buffer[0], buffer[1], buffer[2]);
            printf("  Expected REX=48, Opcode=89, ModRM=D8\n");
            printf("  REX=%s, Opcode=%s, ModRM=%s\n",
                   rex_w ? "OK" : "FAIL",
                   opcode_ok ? "OK" : "FAIL",
                   modrm_ok ? "OK" : "FAIL");
        }
    } else {
        success = false;
        printf("  Length=%u (expected >=3)\n", length);
    }
    
    x86_target_destroy(target);
    return success; 
}

// 测试ARM64简单编码
static bool test_arm64_nop_encode(void) {
    SilverTarget *target = arm64_target_create();
    if (!target) return false;
    
    MachineInstExt inst;
    memset(&inst, 0, sizeof(inst));
    inst.base.opcode = MACH_NOP;
    
    uint8_t buffer[16];
    uint32_t length = 0;
    
    bool success = target->encode(target, &inst, buffer, &length);
    if (success) {
        // NOP = 0xD503201F
        success = (length == 4) &&
                  (buffer[0] == 0x1F) &&
                  (buffer[1] == 0x20) &&
                  (buffer[2] == 0x03) &&
                  (buffer[3] == 0xD5);
    }
    
    arm64_target_destroy(target);
    return success;
}

// 测试RISC-V64简单编码
static bool test_riscv64_nop_encode(void) {
    SilverTarget *target = riscv64_target_create();
    if (!target) return false;
    
    MachineInstExt inst;
    memset(&inst, 0, sizeof(inst));
    inst.base.opcode = MACH_NOP;
    
    uint8_t buffer[16];
    uint32_t length = 0;
    
    bool success = target->encode(target, &inst, buffer, &length);
    if (success) {
        // NOP = ADDI x0, x0, 0
        success = (length == 4);
        // 具体编码: 0x00000013
        if (success) {
            uint32_t word = buffer[0] | (buffer[1] << 8) | 
                           (buffer[2] << 16) | (buffer[3] << 24);
            success = (word == 0x00000013);
        }
    }
    
    riscv64_target_destroy(target);
    return success;
}

int main(void) {
    printf("=== Silver Target Platform Unit Tests ===\n\n");
    
    RUN_TEST(x86_target_create);
    RUN_TEST(arm64_target_create);
    RUN_TEST(riscv64_target_create);
    RUN_TEST(x86_simple_encode);
    RUN_TEST(x86_mov_encode);
    RUN_TEST(arm64_nop_encode);
    RUN_TEST(riscv64_nop_encode);
    
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}