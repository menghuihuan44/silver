#include "silver/ir/ir_module.h"
#include "silver/ir/ir_builder.h"
#include "silver/support/arena.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    tests_run++; \
    printf("  Running %s... ", #name); \
    test_##name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAILED at %s:%d: %s != %s (%lld != %lld)\n", \
               __FILE__, __LINE__, #a, #b, (long long)(a), (long long)(b)); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    if (strcmp((a), (b)) != 0) { \
        printf("FAILED at %s:%d: %s != %s ('%s' != '%s')\n", \
               __FILE__, __LINE__, #a, #b, (a), (b)); \
        tests_failed++; \
        return; \
    } \
} while(0)

// 测试模块创建和销毁
TEST(module_create_destroy) {
    IRModule *module = ir_module_create("test", "x86_64-unknown-none");
    ASSERT(module != NULL);
    ASSERT_STR_EQ(module->module_name, "test");
    ASSERT_STR_EQ(module->target_triple, "x86_64-unknown-none");
    ASSERT(module->arena != NULL);
    ASSERT(module->error_ctx != NULL);
    
    ir_module_destroy(module);
}

// 测试类型表
TEST(type_table) {
    IRModule *module = ir_module_create("test", "x86_64-unknown-none");
    ASSERT(module != NULL);
    
    IRTypeTable *types = &module->type_table;
    ASSERT(types != NULL);
    ASSERT_EQ(types->num_types, 15);
    ASSERT_EQ(types->capacity, 128);
    
    ir_module_destroy(module);
}

// 测试值池
TEST(value_pool) {
    IRModule *module = ir_module_create("test", "x86_64-unknown-none");
    ASSERT(module != NULL);
    
    IRValuePool *pool = &module->value_pool;
    ASSERT(pool != NULL);
    ASSERT_EQ(pool->num_values, 0);
    
    // 创建整数常量
    IRValueId const_id = ir_value_create_int_const(pool, 0, 42);
    ASSERT(const_id != IR_VALUE_ID_INVALID);
    ASSERT_EQ(pool->num_values, 1);
    
    IRValue *val = ir_value_get(pool, const_id);
    ASSERT(val != NULL);
    ASSERT(val->kind == IR_VALUE_CONSTANT);
    ASSERT_EQ(val->int_val, 42);
    
    // 常量去重
    IRValueId same_id = ir_value_create_int_const(pool, 0, 42);
    ASSERT_EQ(const_id, same_id);  // 应该返回相同的ID
    
    ir_module_destroy(module);
}

// 测试函数创建
TEST(function_create) {
    IRModule *module = ir_module_create("test", "x86_64-unknown-none");
    ASSERT(module != NULL);
    
    // 创建函数类型: i64()
    IRTypeTable *types = &module->type_table;
    IRTypeId i64_type = types->i64_type;
    IRTypeId func_type = ir_type_create_func(types, i64_type, 0, NULL, false);
    
    if (func_type == IR_TYPE_ID_INVALID) {
        // 类型可能还没初始化，跳过类型检查
    }
    
    IRFunction *func = ir_module_add_function(module, "main", func_type);
    ASSERT(func != NULL);
    ASSERT_STR_EQ(func->name, "main");
    ASSERT_EQ(func->id, 0);
    ASSERT_EQ(module->num_functions, 1);
    
    // 获取函数
    IRFunction *found = ir_module_find_function(module, "main");
    ASSERT(found != NULL);
    ASSERT_EQ(found->id, 0);
    
    // 查找不存在的函数
    IRFunction *not_found = ir_module_find_function(module, "nonexistent");
    ASSERT(not_found == NULL);
    
    ir_module_destroy(module);
}

// 测试基本块创建
TEST(basic_block_create) {
    IRModule *module = ir_module_create("test", "x86_64-unknown-none");
    ASSERT(module != NULL);
    
    IRFunction *func = ir_module_add_function(module, "test_func", 0);
    ASSERT(func != NULL);
    
    // 创建块
    IRBlock *entry = ir_function_add_block(func, "entry");
    ASSERT(entry != NULL);
    ASSERT_EQ(entry->id, 0);
    ASSERT_EQ(func->num_blocks, 1);
    
    IRBlock *body = ir_function_add_block(func, "body");
    ASSERT(body != NULL);
    ASSERT_EQ(body->id, 1);
    ASSERT_EQ(func->num_blocks, 2);
    
    // 获取块
    IRBlock *found = ir_function_get_block(func, 0);
    ASSERT(found != NULL);
    ASSERT_EQ(found->id, 0);
    
    ir_module_destroy(module);
}

// 测试指令创建
TEST(instruction_create) {
    IRModule *module = ir_module_create("test", "x86_64-unknown-none");
    ASSERT(module != NULL);
    
    IRFunction *func = ir_module_add_function(module, "test_func", 0);
    ASSERT(func != NULL);
    
    // 创建指令
    IRTypeId i64_type = module->type_table.i64_type;
    if (i64_type == IR_TYPE_ID_INVALID) {
        i64_type = 0;  // 使用0作为占位类型
    }
    
    IRInst *inst = ir_function_add_inst(func, IR_OP_ADD, i64_type, 0,
                                         0, 1, IR_VALUE_ID_INVALID, 0);
    ASSERT(inst != NULL);
    ASSERT(inst->opcode == IR_OP_ADD);
    ASSERT(inst->type_id == i64_type);
    ASSERT_EQ(func->num_insts, 1);
    
    // 创建更多指令
    IRInst *inst2 = ir_function_add_inst(func, IR_OP_RET, IR_TYPE_ID_INVALID, 0,
                                          0, IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 0);
    ASSERT(inst2 != NULL);
    ASSERT(inst2->opcode == IR_OP_RET);
    ASSERT_EQ(func->num_insts, 2);
    
    ir_module_destroy(module);
}

// 测试IR构建器
TEST(ir_builder_basic) {
    IRModule *module = ir_module_create("test", "x86_64-unknown-none");
    ASSERT(module != NULL);
    
    IRBuilder *builder = ir_builder_create(module);
    ASSERT(builder != NULL);
    ASSERT(builder->module == module);
    
    IRFunction *func = ir_module_add_function(module, "main", 0);
    ASSERT(func != NULL);
    
    ir_builder_set_position(builder, func, NULL);
    
    IRBlock *entry = ir_builder_create_block(builder, "entry");
    ASSERT(entry != NULL);
    
    ir_builder_set_insert_point(builder, entry);
    
    // 创建常量
    IRValueId const_42 = ir_builder_int_const(builder, builder->i64_type, 42);
    ASSERT(const_42 != IR_VALUE_ID_INVALID);
    
    IRValueId const_10 = ir_builder_int_const(builder, builder->i64_type, 10);
    ASSERT(const_10 != IR_VALUE_ID_INVALID);
    
    // 创建加法
    IRValueId sum = ir_builder_add(builder, const_42, const_10);
    ASSERT(sum != IR_VALUE_ID_INVALID);
    
    // 创建返回
    ir_builder_ret(builder, sum);
    
    ASSERT_EQ(func->num_insts, 2);  // add, ret，常量不算指令
    
    ir_builder_destroy(builder);
    ir_module_destroy(module);
}

// 测试IR模块验证
TEST(module_verify_empty) {
    IRModule *module = ir_module_create("test", "x86_64-unknown-none");
    ASSERT(module != NULL);
    
    // 空模块应该验证通过
    bool valid = ir_module_verify(module);
    ASSERT(valid == true);
    
    ir_module_destroy(module);
}

// 测试全局变量
TEST(global_variable) {
    IRModule *module = ir_module_create("test", "x86_64-unknown-none");
    ASSERT(module != NULL);
    
    IRValueId init_val = ir_value_create_int_const(&module->value_pool, 0, 0);
    ASSERT(init_val != IR_VALUE_ID_INVALID);
    
    IRGlobal *global = ir_module_add_global(module, "test_global", 0, init_val);
    ASSERT(global != NULL);
    ASSERT_STR_EQ(global->name, "test_global");
    ASSERT_EQ(global->id, 0);
    ASSERT_EQ(module->num_globals, 1);
    
    // 查找全局变量
    IRGlobal *found = ir_module_find_global(module, "test_global");
    ASSERT(found != NULL);
    ASSERT_EQ(found->id, 0);
    
    ir_module_destroy(module);
}

// 测试IR指令操作码查询
TEST(opcode_queries) {
    ASSERT(ir_opcode_is_terminator(IR_OP_RET) == true);
    ASSERT(ir_opcode_is_terminator(IR_OP_BR) == true);
    ASSERT(ir_opcode_is_terminator(IR_OP_ADD) == false);
    
    ASSERT(ir_opcode_has_side_effects(IR_OP_STORE) == true);
    ASSERT(ir_opcode_has_side_effects(IR_OP_CALL) == true);
    ASSERT(ir_opcode_has_side_effects(IR_OP_ADD) == false);
    
    ASSERT(ir_opcode_is_commutative(IR_OP_ADD) == true);
    ASSERT(ir_opcode_is_commutative(IR_OP_MUL) == true);
    ASSERT(ir_opcode_is_commutative(IR_OP_SUB) == false);
    
    ASSERT(ir_opcode_is_comparison(IR_OP_CMPEQ) == true);
    ASSERT(ir_opcode_is_comparison(IR_OP_ADD) == false);
    
    ASSERT_EQ(ir_opcode_num_operands(IR_OP_ADD), 2);
    ASSERT_EQ(ir_opcode_num_operands(IR_OP_RET), 1);
    ASSERT_EQ(ir_opcode_num_operands(IR_OP_BR), 1);
    ASSERT_EQ(ir_opcode_num_operands(IR_OP_UNREACHABLE), 0);
}

// 测试IR转储（至少不崩溃）
TEST(ir_dump) {
    IRModule *module = ir_module_create("test", "x86_64-unknown-none");
    ASSERT(module != NULL);
    
    IRFunction *func = ir_module_add_function(module, "main", 0);
    ASSERT(func != NULL);
    
    IRBlock *entry = ir_function_add_block(func, "entry");
    ASSERT(entry != NULL);
    
    ir_function_add_inst(func, IR_OP_RET, IR_TYPE_ID_INVALID, 0,
                         IR_VALUE_ID_INVALID, IR_VALUE_ID_INVALID, 
                         IR_VALUE_ID_INVALID, 0);
    
    entry->first_inst = 0;
    entry->last_inst = 0;
    entry->is_sealed = true;
    
    // 转储到临时文件
    FILE *tmp = tmpfile();
    if (tmp) {
        ir_module_dump(module, tmp);
        fclose(tmp);
    }
    
    ir_module_destroy(module);
}

int main(void) {
    printf("=== Silver IR Unit Tests ===\n\n");
    
    RUN_TEST(module_create_destroy);
    RUN_TEST(type_table);
    RUN_TEST(value_pool);
    RUN_TEST(function_create);
    RUN_TEST(basic_block_create);
    RUN_TEST(instruction_create);
    RUN_TEST(ir_builder_basic);
    RUN_TEST(module_verify_empty);
    RUN_TEST(global_variable);
    RUN_TEST(opcode_queries);
    RUN_TEST(ir_dump);
    
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}