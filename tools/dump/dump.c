/**
 * Silver IR Dump Tool
 * 
 * 加载并显示 Silver IR 模块的详细信息。
 * 
 * 用法: silver_dump [options] <file>
 * 选项:
 *   --stats      显示统计信息
 *   --verbose    显示详细类型和值信息
 *   --help       帮助
 */

#include "silver/silver.h"
#include "silver/ir/ir_builder.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void print_usage(const char *prog) {
    printf("Silver IR Dump Tool v%s\n\n", SILVER_VERSION_STRING);
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --stats       Show statistics\n");
    printf("  --verbose     Show detailed type and value info\n");
    printf("  --help        Print this help\n\n");
    printf("This is a demo tool. In production, it would load IR from a file.\n");
    printf("Currently it generates and displays a sample IR module.\n");
}

// 创建一个示例模块
static IRModule *create_sample_module(void) {
    IRModule *module = ir_module_create("sample", "x86_64-unknown-none");
    if (!module) return NULL;
    
    IRBuilder *builder = ir_builder_create(module);
    if (!builder) { ir_module_destroy(module); return NULL; }
    
    // 创建几个函数
    for (int f = 0; f < 2; f++) {
        char name[32];
        snprintf(name, sizeof(name), "func_%d", f);
        IRFunction *func = ir_module_add_function(module, name, 0);
        ir_builder_set_position(builder, func, NULL);
        
        IRBlock *entry = ir_builder_create_block(builder, "entry");
        ir_builder_set_insert_point(builder, entry);
        
        // 生成一些指令
        IRValueId c1 = ir_builder_int_const(builder, builder->i64_type, f * 10 + 1);
        IRValueId c2 = ir_builder_int_const(builder, builder->i64_type, f * 10 + 2);
        IRValueId sum = ir_builder_add(builder, c1, c2);
        
        IRBlock *body = ir_builder_create_block(builder, "body");
        ir_builder_set_insert_point(builder, body);
        IRValueId c3 = ir_builder_int_const(builder, builder->i64_type, 100);
        IRValueId mul = ir_builder_mul(builder, sum, c3);
        ir_builder_ret(builder, mul);
        
        // 连接块
        entry->last_inst = entry->first_inst;  // 简化：第一个块只有一条指令
        body->first_inst = entry->last_inst + 1;
        body->last_inst = func->num_insts - 1;
    }
    
    // 创建全局变量
    IRValueId init = ir_value_create_int_const(&module->value_pool, builder->i64_type, 0);
    ir_module_add_global(module, "global_counter", builder->i64_type, init);
    ir_module_add_global(module, "global_ptr", builder->ptr_type, IR_VALUE_ID_INVALID);
    
    ir_builder_destroy(builder);
    return module;
}

int main(int argc, char **argv) {
    bool show_stats = false;
    bool verbose = false;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--stats") == 0) show_stats = true;
        else if (strcmp(argv[i], "--verbose") == 0) verbose = true;
        else if (strcmp(argv[i], "--help") == 0) { print_usage(argv[0]); return 0; }
    }
    
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              Silver IR Dump Tool v%s                  ║\n", SILVER_VERSION_STRING);
    printf("╚══════════════════════════════════════════════════════════════╝\n\n");
    
    // 创建示例模块
    IRModule *module = create_sample_module();
    if (!module) {
        fprintf(stderr, "Failed to create sample module\n");
        return 1;
    }
    
    // 验证模块
    printf("Module verification: ");
    if (ir_module_verify(module)) {
        printf("PASSED\n\n");
    } else {
        printf("FAILED\n\n");
    }
    
    // 显示 IR
    printf("=== IR Dump ===\n\n");
    ir_module_dump(module, stdout);
    
    // 统计信息
    if (show_stats) {
        printf("=== Statistics ===\n\n");
        char stats_buf[512];
        ir_module_get_stats(module, stats_buf, sizeof(stats_buf));
        printf("%s\n", stats_buf);
        
        // Arena统计
        ArenaStats arena_stats = arena_get_stats(module->arena);
        printf("  Arena blocks: %zu\n", arena_stats.num_blocks);
        printf("  Arena utilization: %.1f%%\n", arena_stats.utilization * 100.0);
    }
    
    // 详细信息
    if (verbose) {
        printf("=== Type Table ===\n\n");
        IRTypeTable *types = &module->type_table;
        for (uint32_t i = 0; i < types->num_types; i++) {
            char type_str[128];
            ir_type_to_string(types, i, type_str, sizeof(type_str));
            printf("  Type %u: %s (size=%u, align=%u)\n",
                   i, type_str, types->types[i].size, types->types[i].alignment);
        }
        
        printf("\n=== Value Pool ===\n\n");
        IRValuePool *pool = &module->value_pool;
        for (uint32_t i = 0; i < pool->num_values && i < 50; i++) {
            char val_str[64];
            ir_value_to_string(pool, i, types, val_str, sizeof(val_str));
            printf("  Value %u: %s (kind=%d, uses=%u)\n",
                   i, val_str, pool->values[i].kind, pool->values[i].use_count);
        }
        if (pool->num_values > 50) {
            printf("  ... and %u more values\n", pool->num_values - 50);
        }
    }
    
    ir_module_destroy(module);
    
    printf("\n═══ Dump Complete ═══\n");
    return 0;
}