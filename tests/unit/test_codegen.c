#include "silver/silver.h"
#include "silver/codegen/isel.h"
#include "silver/codegen/machine.h"
#include "silver/target/x86/x86.h"
#include "silver/target/arm/arm.h"
#include "silver/target/riscv/riscv.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static bool test_##name(void)
#define RUN_TEST(name) do { \
    tests_run++; \
    printf("  Running %s... ", #name); \
    if (test_##name()) { tests_passed++; printf("PASSED\n"); } \
    else { tests_failed++; printf("FAILED\n"); } \
} while(0)

#define ASSERT(cond) do { if (!(cond)) { printf("  -> %s:%d: %s\n", __FILE__, __LINE__, #cond); return false; } } while(0)

// ============================================================
// 测试 codegen_create / codegen_destroy
// ============================================================
static bool test_codegen_create_destroy(void) {
    SilverTarget *target = x86_target_create();
    ASSERT(target != NULL);
    
    IRModule *module = ir_module_create("test_cg", "x86_64-unknown-none");
    ASSERT(module != NULL);
    
    CodeGenContext *cg = codegen_create(target, module);
    ASSERT(cg != NULL);
    ASSERT(cg->target == target);
    ASSERT(cg->module == module);
    ASSERT(cg->code_emitter != NULL);
    ASSERT(cg->data_buffer != NULL);
    ASSERT(cg->reloc_buffer != NULL);
    ASSERT(cg->reg_alloc.reg_map != NULL);
    ASSERT(cg->reg_alloc.reg_values != NULL);
    ASSERT(cg->reg_alloc.map_size > 0);
    ASSERT(cg->reg_alloc.used_gpr == 0);
    ASSERT(cg->reg_alloc.used_fpr == 0);
    
    codegen_destroy(cg);
    ir_module_destroy(module);
    x86_target_destroy(target);
    return true;
}

// ============================================================
// 测试 opcode 索引构建
// ============================================================
static bool test_opcode_index(void) {
    SilverTarget *target = x86_target_create();
    ASSERT(target != NULL);
    
    IRModule *module = ir_module_create("test_idx", "x86_64-unknown-none");
    ASSERT(module != NULL);
    
    CodeGenContext *cg = codegen_create(target, module);
    ASSERT(cg != NULL);
    
    // 设置匹配表并构建索引
    cg->match_table = x86_match_table;
    cg->table_size = x86_match_table_size;
    isel_build_opcode_index(cg);
    
    // 验证常见opcode有匹配项
    ASSERT(cg->opcode_lookup[IR_OP_ADD] != NULL);
    ASSERT(cg->opcode_count[IR_OP_ADD] >= 2);  // reg+imm变体
    
    ASSERT(cg->opcode_lookup[IR_OP_RET] != NULL);
    ASSERT(cg->opcode_count[IR_OP_RET] >= 1);
    
    ASSERT(cg->opcode_lookup[IR_OP_LOAD] != NULL);
    ASSERT(cg->opcode_count[IR_OP_LOAD] >= 1);
    
    ASSERT(cg->opcode_lookup[IR_OP_STORE] != NULL);
    ASSERT(cg->opcode_count[IR_OP_STORE] >= 1);
    
    ASSERT(cg->opcode_lookup[IR_OP_CALL] != NULL);
    ASSERT(cg->opcode_count[IR_OP_CALL] >= 1);
    
    codegen_destroy(cg);
    ir_module_destroy(module);
    x86_target_destroy(target);
    return true;
}

// ============================================================
// 测试寄存器分配
// ============================================================
static bool test_register_alloc(void) {
    SilverTarget *target = x86_target_create();
    ASSERT(target != NULL);
    
    IRModule *module = ir_module_create("test_ra", "x86_64-unknown-none");
    ASSERT(module != NULL);
    
    CodeGenContext *cg = codegen_create(target, module);
    ASSERT(cg != NULL);
    
    // 分配几个寄存器
    MachineRegister r1 = isel_allocate_register(cg, REG_CLASS_GPR, 100);
    ASSERT(r1 != REG_NONE);
    ASSERT(cg->reg_alloc.used_gpr != 0);
    ASSERT(cg->reg_alloc.reg_map[100] == r1);
    
    MachineRegister r2 = isel_allocate_register(cg, REG_CLASS_GPR, 200);
    ASSERT(r2 != REG_NONE);
    ASSERT(r2 != r1);  // 应该分配不同的寄存器
    
    // 重复分配应该返回相同的寄存器
    MachineRegister r1_again = isel_allocate_register(cg, REG_CLASS_GPR, 100);
    ASSERT(r1_again == r1);
    
    // 获取已分配的寄存器
    ASSERT(isel_get_value_reg(cg, 100) == r1);
    ASSERT(isel_get_value_reg(cg, 200) == r2);
    ASSERT(isel_get_value_reg(cg, 999) == REG_NONE);  // 未分配
    
    // 释放寄存器
    isel_release_register(cg, r1);
    ASSERT(cg->reg_alloc.reg_map[100] == REG_NONE);
    ASSERT(isel_get_value_reg(cg, 100) == REG_NONE);
    
    codegen_destroy(cg);
    ir_module_destroy(module);
    x86_target_destroy(target);
    return true;
}

// ============================================================
// 测试浮点寄存器分配
// ============================================================
static bool test_fp_register_alloc(void) {
    SilverTarget *target = x86_target_create();
    ASSERT(target != NULL);
    
    IRModule *module = ir_module_create("test_fpra", "x86_64-unknown-none");
    ASSERT(module != NULL);
    
    CodeGenContext *cg = codegen_create(target, module);
    ASSERT(cg != NULL);
    
    MachineRegister fr1 = isel_allocate_register(cg, REG_CLASS_FPR, 300);
    ASSERT(fr1 != REG_NONE);
    ASSERT(cg->reg_alloc.used_fpr != 0);
    
    MachineRegister fr2 = isel_allocate_register(cg, REG_CLASS_FPR, 400);
    ASSERT(fr2 != REG_NONE);
    ASSERT(fr2 != fr1);
    
    // GPR和FPR应该互不影响
    MachineRegister gr1 = isel_allocate_register(cg, REG_CLASS_GPR, 500);
    ASSERT(gr1 != REG_NONE);
    
    codegen_destroy(cg);
    ir_module_destroy(module);
    x86_target_destroy(target);
    return true;
}

// ============================================================
// 测试条件码转换
// ============================================================
static bool test_condition_codes(void) {
    ASSERT(isel_ir_cmp_to_mach_cond(IR_OP_CMPEQ) == COND_E);
    ASSERT(isel_ir_cmp_to_mach_cond(IR_OP_CMPNE) == COND_NE);
    ASSERT(isel_ir_cmp_to_mach_cond(IR_OP_CMPLT) == COND_L);
    ASSERT(isel_ir_cmp_to_mach_cond(IR_OP_CMPLE) == COND_LE);
    ASSERT(isel_ir_cmp_to_mach_cond(IR_OP_CMPGT) == COND_G);
    ASSERT(isel_ir_cmp_to_mach_cond(IR_OP_CMPGE) == COND_GE);
    ASSERT(isel_ir_cmp_to_mach_cond(IR_OP_CMPULT) == COND_B);
    ASSERT(isel_ir_cmp_to_mach_cond(IR_OP_CMPULE) == COND_BE);
    ASSERT(isel_ir_cmp_to_mach_cond(IR_OP_CMPUGT) == COND_A);
    ASSERT(isel_ir_cmp_to_mach_cond(IR_OP_CMPUGE) == COND_AE);
    return true;
}

// ============================================================
// 测试跨平台 codegen_create
// ============================================================
static bool test_cross_platform_codegen(void) {
    // x86-64
    SilverTarget *x86 = x86_target_create();
    IRModule *m1 = ir_module_create("t1", "x86_64-unknown-none");
    CodeGenContext *c1 = codegen_create(x86, m1);
    ASSERT(c1 != NULL);
    codegen_destroy(c1);
    ir_module_destroy(m1);
    x86_target_destroy(x86);
    
    // ARM64
    SilverTarget *arm = arm64_target_create();
    IRModule *m2 = ir_module_create("t2", "aarch64-unknown-none");
    CodeGenContext *c2 = codegen_create(arm, m2);
    ASSERT(c2 != NULL);
    codegen_destroy(c2);
    ir_module_destroy(m2);
    arm64_target_destroy(arm);
    
    // RISC-V64
    SilverTarget *rv = riscv64_target_create();
    IRModule *m3 = ir_module_create("t3", "riscv64-unknown-none");
    CodeGenContext *c3 = codegen_create(rv, m3);
    ASSERT(c3 != NULL);
    codegen_destroy(c3);
    ir_module_destroy(m3);
    riscv64_target_destroy(rv);
    
    return true;
}

// ============================================================
// 测试指令选择（简单IR）
// ============================================================
static bool test_simple_isel(void) {
    SilverTarget *target = x86_target_create();
    ASSERT(target != NULL);
    
    IRModule *module = ir_module_create("test_isel", "x86_64-unknown-none");
    ASSERT(module != NULL);
    
    CodeGenContext *cg = codegen_create(target, module);
    ASSERT(cg != NULL);
    
    // 构建索引
    cg->match_table = x86_match_table;
    cg->table_size = x86_match_table_size;
    isel_build_opcode_index(cg);
    
    // 创建简单的IR指令
    IRInst ir_inst;
    memset(&ir_inst, 0, sizeof(ir_inst));
    ir_inst.opcode = IR_OP_RET;
    ir_inst.type_id = IR_TYPE_ID_INVALID;
    ir_inst.operand0_id = IR_VALUE_ID_INVALID;
    ir_inst.operand1_id = IR_VALUE_ID_INVALID;
    ir_inst.operand2_id = IR_VALUE_ID_INVALID;
    
    // 指令选择
    MachineInstExt mach_insts[4];
    uint32_t num_insts = 0;
    bool result = isel_select_instruction(cg, &ir_inst, mach_insts, &num_insts, 4);
    ASSERT(result == true);
    ASSERT(num_insts == 1);
    ASSERT(mach_insts[0].base.opcode == MACH_RET);
    
    codegen_destroy(cg);
    ir_module_destroy(module);
    x86_target_destroy(target);
    return true;
}

// ============================================================
// 测试 ISel 缓存
// ============================================================
static bool test_isel_cache(void) {
    SilverTarget *target = x86_target_create();
    IRModule *module = ir_module_create("test_cache", "x86_64-unknown-none");
    CodeGenContext *cg = codegen_create(target, module);
    cg->match_table = x86_match_table;
    cg->table_size = x86_match_table_size;
    isel_build_opcode_index(cg);
    
    // 多次选择相同类型的指令，缓存应该命中
    ASSERT(cg->isel_cache_count == 0);
    
    for (int i = 0; i < 5; i++) {
        IRInst ir;
        memset(&ir, 0, sizeof(ir));
        ir.opcode = IR_OP_RET;
        ir.type_id = IR_TYPE_ID_INVALID;
        
        MachineInstExt mach[4];
        uint32_t num = 0;
        isel_select_instruction(cg, &ir, mach, &num, 4);
    }
    
    // 缓存应该有1个条目（RET指令）
    ASSERT(cg->isel_cache_count == 1);
    ASSERT(cg->isel_cache[0].opcode == IR_OP_RET);
    
    codegen_destroy(cg);
    ir_module_destroy(module);
    x86_target_destroy(target);
    return true;
}

int main(void) {
    printf("=== Silver Code Generation Unit Tests ===\n\n");
    
    RUN_TEST(codegen_create_destroy);
    RUN_TEST(opcode_index);
    RUN_TEST(register_alloc);
    RUN_TEST(fp_register_alloc);
    RUN_TEST(condition_codes);
    RUN_TEST(cross_platform_codegen);
    RUN_TEST(simple_isel);
    RUN_TEST(isel_cache);
    
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    return tests_failed > 0 ? 1 : 0;
}