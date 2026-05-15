#ifndef SILVER_CODEGEN_ISEL_H
#define SILVER_CODEGEN_ISEL_H

#include "silver/codegen/machine.h"
#include "silver/codegen/emitter.h"
#include "silver/ir/ir_module.h"
#include "silver/support/buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OPERAND_ANY,
    OPERAND_REG,
    OPERAND_IMM,
    OPERAND_MEM,
    OPERAND_LABEL,
} OperandConstraint;

typedef struct {
    IROpcode ir_opcode;
    uint8_t operand_constraints[3];
    uint8_t operand_reg_class[3];
    MachineOpcode mach_opcode;
    int8_t operand_mapping[3];
    uint8_t result_reg_class;
    uint16_t flags;
    uint32_t cost;
} MatchEntry;

enum {
    MATCH_COMMUTATIVE       = (1 << 0),
    MATCH_DESTROYS_OPERAND  = (1 << 1),
    MATCH_NEEDS_SPECIAL_REG = (1 << 2),
    MATCH_HAS_IMM_VARIANT   = (1 << 3),
};

// ✅ ISel缓存项
typedef struct {
    IROpcode opcode;
    IRTypeId type_id;
    uint16_t flags;
    const MatchEntry *match;
} ISelCacheEntry;

#define ISEL_CACHE_SIZE 32

typedef struct {
    SilverTarget *target;
    IRModule *module;
    IRFunction *current_func;
    
    const MatchEntry *match_table;
    uint32_t table_size;
    
    // ✅ 按opcode索引的快速查找表
    const MatchEntry **opcode_lookup[IR_OP_COUNT];
    uint32_t opcode_count[IR_OP_COUNT];
    
    // ✅ ISel缓存
    ISelCacheEntry isel_cache[ISEL_CACHE_SIZE];
    uint32_t isel_cache_count;
    
    CodeEmitter *code_emitter;
    SilverBuffer *data_buffer;
    SilverBuffer *reloc_buffer;
    
    struct {
        MachineRegister *reg_map;
        uint32_t map_size;
        RegisterSet used_gpr;
        RegisterSet used_fpr;
        uint32_t *spill_map;
        uint32_t num_spills;
        int32_t *reg_values;
    } reg_alloc;
    
    uint32_t num_machine_insts;
    uint32_t num_spills;
    uint32_t num_reloads;
} CodeGenContext;

CodeGenContext *codegen_create(SilverTarget *target, IRModule *module);
void codegen_destroy(CodeGenContext *ctx);

// ✅ 构建/释放opcode索引
void isel_build_opcode_index(CodeGenContext *ctx);
void isel_free_opcode_index(CodeGenContext *ctx);

bool codegen_generate(CodeGenContext *ctx, SilverBuffer *output);
bool codegen_generate_function(CodeGenContext *ctx, IRFunction *func);
bool isel_select_instruction(CodeGenContext *ctx, IRInst *ir_inst,
                              MachineInstExt *mach_insts,
                              uint32_t *num_insts, uint32_t max_insts);

MachineCondition isel_ir_cmp_to_mach_cond(IROpcode ir_cmp);
MachineRegister isel_allocate_register(CodeGenContext *ctx,
                                        RegisterClass reg_class,
                                        IRValueId value_id);
void isel_release_register(CodeGenContext *ctx, MachineRegister reg);
MachineRegister isel_get_value_reg(CodeGenContext *ctx, IRValueId value_id);
void isel_spill_value(CodeGenContext *ctx, IRValueId value_id,
                      MachineRegister reg, uint32_t slot);
void isel_reload_value(CodeGenContext *ctx, IRValueId value_id,
                       MachineRegister reg);

#ifdef __cplusplus
}
#endif

#endif // SILVER_CODEGEN_ISEL_H