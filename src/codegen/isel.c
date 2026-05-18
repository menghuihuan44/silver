#include "silver/codegen/isel.h"
#include <stdlib.h>
#include <string.h>

// ============================================================
// 活跃区间 & 寄存器分配器
// ============================================================
typedef struct {
    uint32_t start;
    uint32_t end;
    IRValueId value_id;
    IRTypeId type_id;
    MachineRegister reg;
    uint32_t spill_slot;
    bool spilled;
} LiveInterval;

typedef struct {
    CodeGenContext *cg_ctx;
    LiveInterval *intervals;
    uint32_t num_intervals;
    uint32_t interval_capacity;
    RegisterSet free_gpr;
    RegisterSet free_fpr;
    struct {
        MachineRegister reg;
        IRValueId value;
        uint32_t end_point;
    } active_regs[32];
    uint32_t num_active;
    uint32_t next_spill_slot;
    uint32_t *value_to_slot;
    uint32_t value_to_slot_size;
} RegisterAllocator;

static RegisterAllocator *ra_create(CodeGenContext *cg_ctx) {
    RegisterAllocator *ra = (RegisterAllocator *)calloc(1, sizeof(RegisterAllocator));
    if (!ra) return NULL;
    ra->cg_ctx = cg_ctx;
    ra->interval_capacity = 256;
    ra->intervals = (LiveInterval *)malloc(ra->interval_capacity * sizeof(LiveInterval));
    if (!ra->intervals) { free(ra); return NULL; }
    ra->free_gpr = cg_ctx->target->available_gpr;
    ra->free_fpr = cg_ctx->target->available_fpr;
    ra->value_to_slot_size = 4096;
    ra->value_to_slot = (uint32_t *)malloc(ra->value_to_slot_size * sizeof(uint32_t));
    if (!ra->value_to_slot) { free(ra->intervals); free(ra); return NULL; }
    for (uint32_t i = 0; i < ra->value_to_slot_size; i++) ra->value_to_slot[i] = UINT32_MAX;
    return ra;
}

static void ra_destroy(RegisterAllocator *ra) {
    if (!ra) return;
    free(ra->intervals);
    free(ra->value_to_slot);
    free(ra);
}

static bool is_float_type(IRTypeTable *types, IRTypeId type_id) {
    if (type_id >= types->num_types) return false;
    return types->types[type_id].kind == IR_TYPE_FLOAT;
}

static void compute_live_intervals(RegisterAllocator *ra, IRFunction *func) {
    if (!ra || !func) return;
    ra->num_intervals = 0;
    IRValuePool *pool = &ra->cg_ctx->module->value_pool;
    
    for (uint32_t i = 0; i < func->num_insts; i++) {
        IRInst *inst = &func->instructions[i];
        
        for (int o = 0; o < 3; o++) {
            IRValueId op_id = (o == 0) ? inst->operand0_id :
                              (o == 1) ? inst->operand1_id : inst->operand2_id;
            if (op_id == IR_VALUE_ID_INVALID) continue;
            IRValue *op = ir_value_get(pool, op_id);
            if (!op || op->kind != IR_VALUE_INSTRUCTION) continue;
            
            bool found = false;
            for (uint32_t j = 0; j < ra->num_intervals; j++) {
                if (ra->intervals[j].value_id == op_id) {
                    ra->intervals[j].end = i; found = true; break;
                }
            }
            if (!found) {
                if (ra->num_intervals >= ra->interval_capacity) {
                    ra->interval_capacity *= 2;
                    ra->intervals = (LiveInterval *)realloc(ra->intervals,
                        ra->interval_capacity * sizeof(LiveInterval));
                }
                LiveInterval *iv = &ra->intervals[ra->num_intervals++];
                memset(iv, 0, sizeof(*iv));
                iv->value_id = op_id; iv->type_id = op->type_id;
                iv->start = op->inst_id; iv->end = i;
                iv->reg = REG_NONE; iv->spill_slot = UINT32_MAX;
            }
        }
        
        if (inst->type_id != IR_TYPE_ID_INVALID && inst->type_id != 0) {
            if (func->inst_to_value && i < func->inst_to_value_capacity) {
                IRValueId vid = func->inst_to_value[i];
                if (vid != IR_VALUE_ID_INVALID) {
                    bool found = false;
                    for (uint32_t j = 0; j < ra->num_intervals; j++) {
                        if (ra->intervals[j].value_id == vid) { found = true; break; }
                    }
                    if (!found) {
                        if (ra->num_intervals >= ra->interval_capacity) {
                            ra->interval_capacity *= 2;
                            ra->intervals = (LiveInterval *)realloc(ra->intervals,
                                ra->interval_capacity * sizeof(LiveInterval));
                        }
                        LiveInterval *iv = &ra->intervals[ra->num_intervals++];
                        memset(iv, 0, sizeof(*iv));
                        iv->value_id = vid; iv->type_id = inst->type_id;
                        iv->start = i; iv->end = i;
                        iv->reg = REG_NONE; iv->spill_slot = UINT32_MAX;
                    }
                }
            }
        }
    }
}

static bool linear_scan_allocate(RegisterAllocator *ra) {
    if (!ra) return false;
    
    for (uint32_t i = 1; i < ra->num_intervals; i++) {
        LiveInterval key = ra->intervals[i];
        int32_t j = (int32_t)i - 1;
        while (j >= 0 && ra->intervals[j].start > key.start) {
            ra->intervals[j + 1] = ra->intervals[j]; j--;
        }
        ra->intervals[j + 1] = key;
    }
    
    for (uint32_t i = 0; i < ra->num_intervals; i++) {
        LiveInterval *cur = &ra->intervals[i];
        
        for (uint32_t j = 0; j < ra->num_active; j++) {
            if (ra->active_regs[j].end_point < cur->start) {
                MachineRegister reg = ra->active_regs[j].reg;
                const RegisterDesc *desc = machine_get_register_desc(ra->cg_ctx->target, reg);
                if (desc && desc->reg_class == REG_CLASS_FPR)
                    ra->free_fpr |= machine_reg_mask(reg);
                else if (desc)
                    ra->free_gpr |= machine_reg_mask(reg);
                ra->active_regs[j] = ra->active_regs[--ra->num_active]; j--;
            }
        }
        
        bool need_float = is_float_type(&ra->cg_ctx->module->type_table, cur->type_id);
        RegisterSet *free_set = need_float ? &ra->free_fpr : &ra->free_gpr;
        
        if (*free_set != 0) {
            MachineRegister reg = REG_NONE;
            for (int r = 0; r < REG_COUNT; r++) {
                if (*free_set & (1ULL << r)) { reg = (MachineRegister)r; *free_set &= ~(1ULL << r); break; }
            }
            cur->reg = reg;
            if (ra->num_active < 32) {
                ra->active_regs[ra->num_active].reg = reg;
                ra->active_regs[ra->num_active].value = cur->value_id;
                ra->active_regs[ra->num_active].end_point = cur->end;
                ra->num_active++;
            }
        } else {
            uint32_t max_end = 0, spill_idx = 0;
            for (uint32_t j = 0; j < ra->num_active; j++) {
                if (ra->active_regs[j].end_point > max_end) {
                    max_end = ra->active_regs[j].end_point; spill_idx = j;
                }
            }
            if (max_end > cur->end) {
                MachineRegister reg = ra->active_regs[spill_idx].reg;
                for (uint32_t j = 0; j < ra->num_intervals; j++) {
                    if (ra->intervals[j].value_id == ra->active_regs[spill_idx].value) {
                        ra->intervals[j].spilled = true;
                        ra->intervals[j].spill_slot = ra->next_spill_slot;
                        ra->value_to_slot[ra->intervals[j].value_id] = ra->next_spill_slot;
                        break;
                    }
                }
                ra->next_spill_slot++;
                cur->reg = reg;
                ra->active_regs[spill_idx].reg = reg;
                ra->active_regs[spill_idx].value = cur->value_id;
                ra->active_regs[spill_idx].end_point = cur->end;
            } else {
                cur->spilled = true;
                cur->spill_slot = ra->next_spill_slot;
                ra->value_to_slot[cur->value_id] = ra->next_spill_slot;
                ra->next_spill_slot++;
            }
        }
    }
    return true;
}

// ============================================================
// CodeGenContext 创建/销毁
// ============================================================
CodeGenContext *codegen_create(SilverTarget *target, IRModule *module) {
    if (!target || !module) return NULL;
    
    CodeGenContext *ctx = (CodeGenContext *)calloc(1, sizeof(CodeGenContext));
    if (!ctx) return NULL;
    
    ctx->target = target;
    ctx->module = module;
    
    // ✅ 创建 CodeEmitter 而非 SilverBuffer
    ctx->code_emitter = emitter_create();
    ctx->data_buffer = silver_buffer_create(4096);
    ctx->reloc_buffer = silver_buffer_create(4096);
    
    if (!ctx->code_emitter || !ctx->data_buffer || !ctx->reloc_buffer) {
        codegen_destroy(ctx); return NULL;
    }
    
    ctx->reg_alloc.map_size = 8192;
    ctx->reg_alloc.reg_map = (MachineRegister *)malloc(ctx->reg_alloc.map_size * sizeof(MachineRegister));
    if (!ctx->reg_alloc.reg_map) { codegen_destroy(ctx); return NULL; }
    for (uint32_t i = 0; i < ctx->reg_alloc.map_size; i++) ctx->reg_alloc.reg_map[i] = REG_NONE;
    
    ctx->reg_alloc.reg_values = (int32_t *)malloc(ctx->reg_alloc.map_size * sizeof(int32_t));
    if (!ctx->reg_alloc.reg_values) { codegen_destroy(ctx); return NULL; }
    for (uint32_t i = 0; i < ctx->reg_alloc.map_size; i++) ctx->reg_alloc.reg_values[i] = -1;
    
    return ctx;
}

void codegen_destroy(CodeGenContext *ctx) {
    if (!ctx) return;
    isel_free_opcode_index(ctx);
    emitter_destroy(ctx->code_emitter);
    silver_buffer_destroy(ctx->data_buffer);
    silver_buffer_destroy(ctx->reloc_buffer);
    free(ctx->reg_alloc.reg_map);
    free(ctx->reg_alloc.reg_values);
    free(ctx->reg_alloc.spill_map);
    free(ctx);
}

// ============================================================
// Opcode索引
// ============================================================
void isel_build_opcode_index(CodeGenContext *ctx) {
    if (!ctx || !ctx->match_table) return;
    for (int i = 0; i < IR_OP_COUNT; i++) {
        ctx->opcode_lookup[i] = NULL; ctx->opcode_count[i] = 0;
    }
    for (uint32_t i = 0; i < ctx->table_size; i++) {
        IROpcode op = ctx->match_table[i].ir_opcode;
        if (op < IR_OP_COUNT) ctx->opcode_count[op]++;
    }
    for (int op = 0; op < IR_OP_COUNT; op++) {
        if (ctx->opcode_count[op] > 0) {
            ctx->opcode_lookup[op] = (const MatchEntry **)malloc(
                ctx->opcode_count[op] * sizeof(MatchEntry *));
            uint32_t idx = 0;
            for (uint32_t i = 0; i < ctx->table_size; i++) {
                if (ctx->match_table[i].ir_opcode == (IROpcode)op)
                    ctx->opcode_lookup[op][idx++] = &ctx->match_table[i];
            }
        }
    }
    ctx->isel_cache_count = 0;
}

void isel_free_opcode_index(CodeGenContext *ctx) {
    if (!ctx) return;
    for (int i = 0; i < IR_OP_COUNT; i++) {
        free(ctx->opcode_lookup[i]); ctx->opcode_lookup[i] = NULL;
    }
}

// ============================================================
// 寄存器分配接口
// ============================================================
static bool ensure_reg_map_size(CodeGenContext *ctx, uint32_t needed) {
    if (needed <= ctx->reg_alloc.map_size) return true;
    uint32_t ns = ctx->reg_alloc.map_size * 2;
    while (ns < needed) ns *= 2;
    MachineRegister *nm = (MachineRegister *)realloc(ctx->reg_alloc.reg_map, ns * sizeof(MachineRegister));
    if (!nm) return false;
    for (uint32_t i = ctx->reg_alloc.map_size; i < ns; i++) nm[i] = REG_NONE;
    ctx->reg_alloc.reg_map = nm;
    int32_t *nv = (int32_t *)realloc(ctx->reg_alloc.reg_values, ns * sizeof(int32_t));
    if (!nv) return false;
    for (uint32_t i = ctx->reg_alloc.map_size; i < ns; i++) nv[i] = -1;
    ctx->reg_alloc.reg_values = nv;
    ctx->reg_alloc.map_size = ns;
    return true;
}

MachineRegister isel_allocate_register(CodeGenContext *ctx, RegisterClass reg_class, IRValueId value_id) {
    if (!ctx) return REG_NONE;
    if (value_id >= ctx->reg_alloc.map_size && !ensure_reg_map_size(ctx, value_id + 4096)) return REG_NONE;
    if (ctx->reg_alloc.reg_map[value_id] != REG_NONE) return ctx->reg_alloc.reg_map[value_id];
    
    const SilverTarget *target = ctx->target;
    RegisterSet *used = (reg_class == REG_CLASS_FPR) ? &ctx->reg_alloc.used_fpr : &ctx->reg_alloc.used_gpr;
    RegisterSet avail = (reg_class == REG_CLASS_FPR) ? target->available_fpr : target->available_gpr;
    avail &= ~(*used);
    
    for (int i = 0; i < REG_COUNT; i++) {
        if (reg_set_has(avail, (MachineRegister)i)) {
            MachineRegister reg = (MachineRegister)i;
            *used |= machine_reg_mask(reg);
            ctx->reg_alloc.reg_map[value_id] = reg;
            ctx->reg_alloc.reg_values[reg] = (int32_t)value_id;
            return reg;
        }
    }
    for (int i = 0; i < REG_COUNT; i++) {
        if (reg_set_has(*used, (MachineRegister)i)) {
            MachineRegister reg = (MachineRegister)i;
            int32_t old = ctx->reg_alloc.reg_values[reg];
            if (old >= 0) {
                uint32_t slot = ctx->reg_alloc.num_spills++;
                isel_spill_value(ctx, (IRValueId)old, reg, slot);
                ctx->reg_alloc.reg_values[reg] = -1;
                ctx->reg_alloc.reg_map[old] = REG_NONE;
                ctx->reg_alloc.reg_map[value_id] = reg;
                ctx->reg_alloc.reg_values[reg] = (int32_t)value_id;
                ctx->num_spills++;
                return reg;
            }
        }
    }
    return REG_NONE;
}

void isel_release_register(CodeGenContext *ctx, MachineRegister reg) {
    if (!ctx || reg == REG_NONE) return;
    const RegisterDesc *desc = machine_get_register_desc(ctx->target, reg);
    if (!desc) return;
    int32_t val = ctx->reg_alloc.reg_values[reg];
    if (val >= 0 && (uint32_t)val < ctx->reg_alloc.map_size) {
        ctx->reg_alloc.reg_map[val] = REG_NONE;
    }
    ctx->reg_alloc.reg_values[reg] = -1;
    if (desc->reg_class == REG_CLASS_FPR)
        ctx->reg_alloc.used_fpr = reg_set_remove(ctx->reg_alloc.used_fpr, reg);
    else if (desc->reg_class == REG_CLASS_GPR)
        ctx->reg_alloc.used_gpr = reg_set_remove(ctx->reg_alloc.used_gpr, reg);
}

MachineRegister isel_get_value_reg(CodeGenContext *ctx, IRValueId value_id) {
    if (!ctx || value_id >= ctx->reg_alloc.map_size) return REG_NONE;
    return ctx->reg_alloc.reg_map[value_id];
}

void isel_spill_value(CodeGenContext *ctx, IRValueId value_id, MachineRegister reg, uint32_t slot) {
    if (!ctx || !ctx->target || !ctx->target->encode) return;
    MachineInstExt inst;
    memset(&inst, 0, sizeof(inst));
    inst.base.opcode = MACH_STORE;
    inst.base.rn = reg;
    inst.base.rm = REG_RSP;
    inst.displacement = -(int32_t)((slot + 1) * 8);
    uint8_t buf[16]; uint32_t len = 0;
    if (ctx->target->encode(ctx->target, &inst, buf, &len))
        emitter_emit_bytes(ctx->code_emitter, buf, len);
}

void isel_reload_value(CodeGenContext *ctx, IRValueId value_id, MachineRegister reg) {
    if (!ctx || !ctx->target || !ctx->target->encode) return;
    uint32_t slot = 0;
    if (value_id < ctx->reg_alloc.map_size && ctx->reg_alloc.spill_map)
        slot = ctx->reg_alloc.spill_map[value_id];
    MachineInstExt inst;
    memset(&inst, 0, sizeof(inst));
    inst.base.opcode = MACH_LOAD;
    inst.base.rd = reg;
    inst.base.rn = REG_RSP;
    inst.displacement = -(int32_t)((slot + 1) * 8);
    uint8_t buf[16]; uint32_t len = 0;
    if (ctx->target->encode(ctx->target, &inst, buf, &len))
        emitter_emit_bytes(ctx->code_emitter, buf, len);
}

MachineCondition isel_ir_cmp_to_mach_cond(IROpcode ir_cmp) {
    switch (ir_cmp) {
        case IR_OP_CMPEQ:  return COND_E;  case IR_OP_CMPNE: return COND_NE;
        case IR_OP_CMPLT:  return COND_L;  case IR_OP_CMPLE: return COND_LE;
        case IR_OP_CMPGT:  return COND_G;  case IR_OP_CMPGE: return COND_GE;
        case IR_OP_CMPULT: return COND_B;  case IR_OP_CMPULE: return COND_BE;
        case IR_OP_CMPUGT: return COND_A;  case IR_OP_CMPUGE: return COND_AE;
        case IR_OP_FCMPOEQ: return COND_E; case IR_OP_FCMPONE: return COND_NE;
        case IR_OP_FCMPOLT: return COND_B; case IR_OP_FCMPOLE: return COND_BE;
        case IR_OP_FCMPOGT: return COND_A; case IR_OP_FCMPOGE: return COND_AE;
        case IR_OP_FCMPUEQ: return COND_P; case IR_OP_FCMPUNE: return COND_NP;
        default: return COND_NONE;
    }
}

// ============================================================
// 优化后的指令选择
// ============================================================
bool isel_select_instruction(CodeGenContext *ctx, IRInst *ir_inst,
                              MachineInstExt *mach_insts,
                              uint32_t *num_insts, uint32_t max_insts) {
    if (!ctx || !ir_inst || !mach_insts || !num_insts) return false;
    *num_insts = 0;
    
    IROpcode op = ir_inst->opcode;
    if (op >= IR_OP_COUNT) return false;
    
    // 检查ISel缓存
    for (uint32_t c = 0; c < ctx->isel_cache_count; c++) {
        ISelCacheEntry *ce = &ctx->isel_cache[c];
        if (ce->opcode == op && ce->type_id == ir_inst->type_id && ce->flags == ir_inst->flags) {
            const MatchEntry *match = ce->match;
            if (*num_insts >= max_insts) return false;
            MachineInstExt *inst = &mach_insts[(*num_insts)++];
            memset(inst, 0, sizeof(*inst));
            inst->base.opcode = match->mach_opcode;
            
            if (match->result_reg_class != REG_CLASS_NONE && ir_inst->type_id != IR_TYPE_ID_INVALID && ir_inst->type_id != 0) {
                IRValueId rid = IR_VALUE_ID_INVALID;
                uint32_t idx = ir_inst->original_order;
                if (ctx->current_func && ctx->current_func->inst_to_value && idx < ctx->current_func->inst_to_value_capacity)
                    rid = ctx->current_func->inst_to_value[idx];
                if (rid == IR_VALUE_ID_INVALID) {
                    IRValuePool *pool = &ctx->module->value_pool;
                    for (uint32_t v = 0; v < pool->num_values; v++) {
                        if (pool->values[v].kind == IR_VALUE_INSTRUCTION && pool->values[v].inst_id == idx && pool->values[v].type_id == ir_inst->type_id)
                        { rid = v; break; }
                    }
                }
                if (rid != IR_VALUE_ID_INVALID) inst->base.rd = isel_allocate_register(ctx, match->result_reg_class, rid);
            }
            for (int o = 0; o < 3; o++) {
                int8_t map = match->operand_mapping[o];
                if (map < 0) continue;
                IRValueId oid = (o == 0) ? ir_inst->operand0_id : (o == 1) ? ir_inst->operand1_id : ir_inst->operand2_id;
                if (oid == IR_VALUE_ID_INVALID) continue;
                MachineRegister reg = isel_get_value_reg(ctx, oid);
                if (reg == REG_NONE) reg = isel_allocate_register(ctx, match->operand_reg_class[o], oid);
                if (map == 0) inst->base.rn = reg; else if (map == 1) inst->base.rm = reg;
            }
            ctx->num_machine_insts++;
            return true;
        }
    }
    
    // opcode索引查找
    if (ctx->opcode_lookup[op] == NULL) return false;
    uint32_t count = ctx->opcode_count[op];
    const MatchEntry **entries = ctx->opcode_lookup[op];
    
    for (uint32_t i = 0; i < count; i++) {
        const MatchEntry *match = entries[i];
        
        if ((match->flags & MATCH_HAS_IMM_VARIANT)) {
            IRValueId check_op = (match->operand_mapping[0] == 0) ? ir_inst->operand0_id : ir_inst->operand1_id;
            if (check_op != IR_VALUE_ID_INVALID && !ir_value_is_constant(&ctx->module->value_pool, check_op))
                continue;
        }
        
        if (*num_insts >= max_insts) break;
        MachineInstExt *inst = &mach_insts[(*num_insts)++];
        memset(inst, 0, sizeof(*inst));
        inst->base.opcode = match->mach_opcode;
        
        if (match->result_reg_class != REG_CLASS_NONE && ir_inst->type_id != IR_TYPE_ID_INVALID && ir_inst->type_id != 0) {
            IRValueId rid = IR_VALUE_ID_INVALID;
            uint32_t idx = ir_inst->original_order;
            if (ctx->current_func && ctx->current_func->inst_to_value && idx < ctx->current_func->inst_to_value_capacity)
                rid = ctx->current_func->inst_to_value[idx];
            if (rid == IR_VALUE_ID_INVALID) {
                IRValuePool *pool = &ctx->module->value_pool;
                for (uint32_t v = 0; v < pool->num_values; v++) {
                    if (pool->values[v].kind == IR_VALUE_INSTRUCTION && pool->values[v].inst_id == idx && pool->values[v].type_id == ir_inst->type_id)
                    { rid = v; break; }
                }
            }
            if (rid != IR_VALUE_ID_INVALID) inst->base.rd = isel_allocate_register(ctx, match->result_reg_class, rid);
        }
        
        for (int o = 0; o < 3; o++) {
            int8_t map = match->operand_mapping[o];
            if (map < 0) continue;
            IRValueId oid = (o == 0) ? ir_inst->operand0_id : (o == 1) ? ir_inst->operand1_id : ir_inst->operand2_id;
            if (oid == IR_VALUE_ID_INVALID) continue;
            MachineRegister reg = isel_get_value_reg(ctx, oid);
            if (reg == REG_NONE) reg = isel_allocate_register(ctx, match->operand_reg_class[o], oid);
            if (map == 0) inst->base.rn = reg; else if (map == 1) inst->base.rm = reg;
        }
        
        if (ctx->isel_cache_count < ISEL_CACHE_SIZE) {
            ISelCacheEntry *ce = &ctx->isel_cache[ctx->isel_cache_count++];
            ce->opcode = op; ce->type_id = ir_inst->type_id;
            ce->flags = ir_inst->flags; ce->match = match;
        }
        
        ctx->num_machine_insts++;
        return true;
    }
    
    return false;
}

// ============================================================
// PHI elimination: 将 PHI 指令转换为前驱块末尾的 COPY
// 在寄存器分配前运行
// ============================================================
static void phi_elimination(IRFunction *func, IRModule *module) {
    IRValuePool *pool = &module->value_pool;
    
    for (uint32_t i = 0; i < func->num_insts; i++) {
        IRInst *inst = &func->instructions[i];
        if (inst->opcode != IR_OP_PHI) continue;
        
        uint32_t num_incoming = inst->extra;
        if (num_incoming == 0) continue;
        
        // 找到 PHI 所在的基本块
        IRBlock *phi_block = NULL;
        for (uint32_t b = 0; b < func->num_blocks; b++) {
            if (func->blocks[b].first_inst <= i && i <= func->blocks[b].last_inst) {
                phi_block = &func->blocks[b];
                break;
            }
        }
        if (!phi_block) continue;
        
        // 找到 PHI 指令的结果值ID
        IRValueId phi_result = IR_VALUE_ID_INVALID;
        for (uint32_t v = 0; v < pool->num_values; v++) {
            if (pool->values[v].kind == IR_VALUE_INSTRUCTION &&
                pool->values[v].inst_id == i &&
                pool->values[v].type_id == inst->type_id) {
                phi_result = v;
                break;
            }
        }
        if (phi_result == IR_VALUE_ID_INVALID) continue;
        
        // 获取前驱块和对应的值
        // operand0 = 第一个值, operand1 = 第一个块
        // operand2 = 第二个值（如果存在）
        IRValueId val0 = inst->operand0_id;
        IRValueId block0_id = inst->operand1_id;
        
        IRValue *block0_val = ir_value_get(pool, block0_id);
        if (!block0_val || block0_val->kind != IR_VALUE_BLOCK) continue;
        uint32_t pred0_id = block0_val->block_id;
        IRBlock *pred0 = &func->blocks[pred0_id];
        
        // 在前驱块0末尾插入 COPY phi_result = val0
        // 找到前驱块的终结指令位置
        if (pred0->last_inst != IR_VALUE_ID_INVALID) {
            uint32_t term_pos = pred0->last_inst;
            // 在终结指令之前插入 COPY
            IRInst copy_inst;
            memset(&copy_inst, 0, sizeof(copy_inst));
            copy_inst.opcode = IR_OP_COPY;
            copy_inst.type_id = inst->type_id;
            copy_inst.operand0_id = (uint16_t)val0;
            copy_inst.original_order = func->num_insts;
            
            // 插入：终结指令后移，COPY放在它前面
            // 简化：直接在终结指令位置写入COPY，原终结指令后移
            if (func->num_insts >= func->inst_capacity) {
                // 扩展指令数组
                uint32_t new_cap = func->inst_capacity * 2;
                IRInst *new_insts = (IRInst *)arena_calloc(
                    module->arena, new_cap * sizeof(IRInst));
                if (!new_insts) continue;
                memcpy(new_insts, func->instructions,
                       func->num_insts * sizeof(IRInst));
                func->instructions = new_insts;
                func->inst_capacity = new_cap;
            }
            
            // 将终结指令后移一位
            memmove(&func->instructions[term_pos + 1],
                    &func->instructions[term_pos],
                    (func->num_insts - term_pos) * sizeof(IRInst));
            
            // 在终结指令位置写入 COPY
            func->instructions[term_pos] = copy_inst;
            func->num_insts++;
            
            // 更新块和后续指令的索引
            pred0->last_inst++;
            for (uint32_t b = 0; b < func->num_blocks; b++) {
                if (func->blocks[b].first_inst > term_pos) {
                    func->blocks[b].first_inst++;
                }
                if (func->blocks[b].last_inst >= term_pos) {
                    func->blocks[b].last_inst++;
                }
            }
            // 更新值池中的 inst_id
            for (uint32_t v = 0; v < pool->num_values; v++) {
                if (pool->values[v].kind == IR_VALUE_INSTRUCTION &&
                    pool->values[v].inst_id > term_pos) {
                    pool->values[v].inst_id++;
                }
            }
            // 更新 inst_to_value 映射
            if (func->inst_to_value) {
                for (uint32_t vi = func->inst_to_value_capacity - 1; vi > term_pos; vi--) {
                    func->inst_to_value[vi] = func->inst_to_value[vi - 1];
                }
                func->inst_to_value[term_pos] = IR_VALUE_ID_INVALID;
            }
        }
        
        // 处理第二个前驱（如果存在）
        if (num_incoming >= 2 && inst->operand2_id != IR_VALUE_ID_INVALID) {
            IRValueId val1 = inst->operand2_id;
            
            // 找到第二个前驱块
            // 从 phi_block 的前驱列表中获取
            uint32_t pred1_id = UINT32_MAX;
            for (uint32_t p = 0; p < phi_block->num_predecessors; p++) {
                if (phi_block->predecessors[p] != pred0_id) {
                    pred1_id = phi_block->predecessors[p];
                    break;
                }
            }
            
            if (pred1_id != UINT32_MAX && pred1_id < func->num_blocks) {
                IRBlock *pred1 = &func->blocks[pred1_id];
                if (pred1->last_inst != IR_VALUE_ID_INVALID) {
                    uint32_t term_pos1 = pred1->last_inst;
                    
                    // 扩展数组
                    if (func->num_insts >= func->inst_capacity) {
                        uint32_t new_cap = func->inst_capacity * 2;
                        IRInst *new_insts = (IRInst *)arena_calloc(
                            module->arena, new_cap * sizeof(IRInst));
                        if (!new_insts) continue;
                        memcpy(new_insts, func->instructions,
                               func->num_insts * sizeof(IRInst));
                        func->instructions = new_insts;
                        func->inst_capacity = new_cap;
                    }
                    
                    // 插入 COPY
                    IRInst copy_inst;
                    memset(&copy_inst, 0, sizeof(copy_inst));
                    copy_inst.opcode = IR_OP_COPY;
                    copy_inst.type_id = inst->type_id;
                    copy_inst.operand0_id = (uint16_t)val1;
                    copy_inst.original_order = func->num_insts;
                    
                    memmove(&func->instructions[term_pos1 + 1],
                            &func->instructions[term_pos1],
                            (func->num_insts - term_pos1) * sizeof(IRInst));
                    
                    func->instructions[term_pos1] = copy_inst;
                    func->num_insts++;
                    
                    pred1->last_inst++;
                    for (uint32_t b = 0; b < func->num_blocks; b++) {
                        if (func->blocks[b].first_inst > term_pos1)
                            func->blocks[b].first_inst++;
                        if (func->blocks[b].last_inst >= term_pos1)
                            func->blocks[b].last_inst++;
                    }
                    for (uint32_t v = 0; v < pool->num_values; v++) {
                        if (pool->values[v].kind == IR_VALUE_INSTRUCTION &&
                            pool->values[v].inst_id > term_pos1) {
                            pool->values[v].inst_id++;
                        }
                    }
                }
            }
        }
        
        // 将 PHI 指令本身替换为 NOP
        inst->opcode = IR_OP_NOP;
    }
}

bool codegen_generate_function(CodeGenContext *ctx, IRFunction *func) {
    if (!ctx || !func) return false;
    ctx->current_func = func;
    ctx->isel_cache_count = 0;
    
    // ============================================================
    // 寄存器分配
    // ============================================================
    RegisterAllocator *ra = ra_create(ctx);
    if (ra) {
        compute_live_intervals(ra, func);
        linear_scan_allocate(ra);
        for (uint32_t i = 0; i < ra->num_intervals; i++) {
            LiveInterval *iv = &ra->intervals[i];
            if (!iv->spilled && iv->reg != REG_NONE) {
                if (iv->value_id < ctx->reg_alloc.map_size) {
                    ctx->reg_alloc.reg_map[iv->value_id] = iv->reg;
                    ctx->reg_alloc.reg_values[iv->reg] = (int32_t)iv->value_id;
                }
            } else if (iv->spilled) {
                if (!ctx->reg_alloc.spill_map) {
                    ctx->reg_alloc.spill_map = (uint32_t *)calloc(
                        ra->next_spill_slot + 1, sizeof(uint32_t));
                    ctx->reg_alloc.num_spills = ra->next_spill_slot;
                }
                if (iv->value_id < ctx->reg_alloc.map_size && ctx->reg_alloc.spill_map)
                    ctx->reg_alloc.spill_map[iv->value_id] = iv->spill_slot;
            }
        }
        ra_destroy(ra);
    }
    
    // ============================================================
    // 快速编码路径
    // ============================================================
    if (ctx->code_emitter->fast_remaining < 65536) {
        emitter_flush(ctx->code_emitter);
    }
    
    uint8_t *fast_ptr = ctx->code_emitter->fast_ptr;
    uint32_t fast_rem = ctx->code_emitter->fast_remaining;
    uint32_t emitted = 0;
    
    // ============================================================
    // 序言
    // ============================================================
    uint8_t prologue[128];
    uint32_t plen = ctx->target->emit_prologue(ctx->target, func, prologue);
    if (plen > 0) {
        memcpy(fast_ptr, prologue, plen);
        fast_ptr += plen;
        fast_rem -= plen;
        emitted += plen;
    }
    
    // ============================================================
    // 指令编码
    // ============================================================
    uint8_t encode_buf[64];
    // ✅ 找到 ret 指令的返回值（如果有）
    IRValueId return_value_id = IR_VALUE_ID_INVALID;
    MachineRegister return_value_reg = REG_NONE;
    bool has_ret = false;
    
    for (uint32_t b = 0; b < func->num_blocks; b++) {
        IRBlock *block = &func->blocks[b];
        if (block->first_inst == IR_VALUE_ID_INVALID) continue;
        
        for (uint32_t i = block->first_inst; i <= block->last_inst; i++) {
            IRInst *ir_inst = &func->instructions[i];
            
            // ============================================================
            // ✅ 处理 ret 指令：不生成 ret 机器码，只记录返回值
            // 真正的 ret 由尾声生成
            // ============================================================
            // 跟踪 ret 指令的操作数
            if (ir_inst->opcode == IR_OP_RET) {
                has_ret = true;
                if (ir_inst->operand0_id != IR_VALUE_ID_INVALID) {
                    return_value_id = ir_inst->operand0_id;
        
                    // ✅ 跟踪 COPY 链：如果操作数是指令结果，找到真正的值
                    IRValue *rv = ir_value_get(&ctx->module->value_pool, return_value_id);
                    if (rv && rv->kind == IR_VALUE_INSTRUCTION) {
                        uint32_t def_inst_idx = rv->inst_id;
                        if (def_inst_idx < func->num_insts) {
                            IRInst *def_inst = &func->instructions[def_inst_idx];
                            if (def_inst->opcode == IR_OP_COPY) {
                                // COPY 指令：真正的值是 operand0
                                return_value_id = def_inst->operand0_id;
                            }
                        }
                    }
        
                    return_value_reg = isel_get_value_reg(ctx, return_value_id);
                }
                continue;
            } 
            
            // ============================================================
            // ✅ 处理 br 和 brcond 指令
            // 无条件跳转：如果目标是下一个块，可以省略
            // 条件跳转：编码为 JMP/JCC
            // ============================================================
            if (ir_inst->opcode == IR_OP_BR) {
                // 简化：始终生成 JMP
                MachineInstExt jmp_inst;
                memset(&jmp_inst, 0, sizeof(jmp_inst));
                jmp_inst.base.opcode = MACH_JMP;
                jmp_inst.extended_imm = 0; // 偏移在链接时计算
                
                uint32_t length = 0;
                if (ctx->target->encode(ctx->target, &jmp_inst, encode_buf, &length)) {
                    memcpy(fast_ptr, encode_buf, length);
                    fast_ptr += length;
                    fast_rem -= length;
                    emitted += length;
                }
                continue;
            }
            
            if (ir_inst->opcode == IR_OP_BRCOND) {
                // 条件跳转
                MachineCondition cond = isel_ir_cmp_to_mach_cond(
                    (IROpcode)ir_inst->operand0_id); // 简化：从最后一条cmp推断
                // 实际需要从上一个cmp指令获取条件码
                // 简化处理：生成 JCC
                MachineInstExt jcc_inst;
                memset(&jcc_inst, 0, sizeof(jcc_inst));
                jcc_inst.base.opcode = MACH_JCC;
                jcc_inst.base.imm = cond;
                jcc_inst.extended_imm = 0;
                
                uint32_t length = 0;
                if (ctx->target->encode(ctx->target, &jcc_inst, encode_buf, &length)) {
                    memcpy(fast_ptr, encode_buf, length);
                    fast_ptr += length;
                    fast_rem -= length;
                    emitted += length;
                }
                continue;
            }
            
            if (ir_inst->opcode == IR_OP_UNREACHABLE) {
                // 生成断点指令
                MachineInstExt brk_inst;
                memset(&brk_inst, 0, sizeof(brk_inst));
                brk_inst.base.opcode = MACH_INT3;
                
                uint32_t length = 0;
                if (ctx->target->encode(ctx->target, &brk_inst, encode_buf, &length)) {
                    memcpy(fast_ptr, encode_buf, length);
                    fast_ptr += length;
                    fast_rem -= length;
                    emitted += length;
                }
                continue;
            }
            
            // ============================================================
            // 其他指令：正常 ISel
            // ============================================================
            MachineInstExt mach_insts[4];
            uint32_t num_insts = 0;
            
            if (isel_select_instruction(ctx, ir_inst, mach_insts, &num_insts, 4)) {
                for (uint32_t j = 0; j < num_insts; j++) {
                    uint32_t length = 0;
                    if (ctx->target->encode(ctx->target, &mach_insts[j], 
                                            encode_buf, &length)) {
                        memcpy(fast_ptr, encode_buf, length);
                        fast_ptr += length;
                        fast_rem -= length;
                        emitted += length;
                    }
                }
            }
        }
    }
    
    // ============================================================
    // ✅ 尾声前：确保返回值在 RAX 中
    // ============================================================
    if (has_ret && return_value_id != IR_VALUE_ID_INVALID) {
    IRValue *ret_val = ir_value_get(&ctx->module->value_pool, return_value_id);
    if (ret_val) {
        // ✅ 先检查是否已经分配了寄存器
        MachineRegister ret_reg = isel_get_value_reg(ctx, return_value_id);
        if (ret_reg != REG_NONE) {
            // 已经在寄存器中，移到 RAX
            if (ret_reg != REG_RAX) {
                MachineInstExt mov_inst;
                memset(&mov_inst, 0, sizeof(mov_inst));
                mov_inst.base.opcode = MACH_MOV;
                mov_inst.base.rd = REG_RAX;
                mov_inst.base.rn = ret_reg;
                uint32_t length = 0;
                if (ctx->target->encode(ctx->target, &mov_inst, encode_buf, &length)) {
                    memcpy(fast_ptr, encode_buf, length);
                    fast_ptr += length;
                    fast_rem -= length;
                    emitted += length;
                }
            }
        } else if (ret_val->kind == IR_VALUE_CONSTANT) {
            // 常量，直接 MOV_IMM
            MachineInstExt mov_imm;
            memset(&mov_imm, 0, sizeof(mov_imm));
            mov_imm.base.opcode = MACH_MOV_IMM;
            mov_imm.base.rd = REG_RAX;
            mov_imm.extended_imm = (uint64_t)ret_val->int_val;
            uint32_t length = 0;
            if (ctx->target->encode(ctx->target, &mov_imm, encode_buf, &length)) {
                memcpy(fast_ptr, encode_buf, length);
                fast_ptr += length;
                fast_rem -= length;
                emitted += length;
            }
        }
    }
} 
    
    // ============================================================
    // 尾声
    // ============================================================
    uint8_t epilogue[128];
    uint32_t elen = ctx->target->emit_epilogue(ctx->target, func, epilogue);
    if (elen > 0) {
        memcpy(fast_ptr, epilogue, elen);
        fast_ptr += elen;
        fast_rem -= elen;
        emitted += elen;
    }
    
    // ============================================================
    // 回写指针
    // ============================================================
    ctx->code_emitter->fast_ptr = fast_ptr;
    ctx->code_emitter->fast_remaining = fast_rem;
    ctx->code_emitter->total_emitted += emitted;
    
    return true;
}

bool codegen_generate(CodeGenContext *ctx, SilverBuffer *output) {
    if (!ctx || !output || !ctx->module) return false;
    
    // ✅ 重置 emitter
    emitter_flush(ctx->code_emitter);
    ctx->code_emitter->fast_ptr = ctx->code_emitter->active->data;
    ctx->code_emitter->fast_remaining = ctx->code_emitter->active->size;
    
    for (uint32_t i = 0; i < ctx->module->num_functions; i++) {
        if (!codegen_generate_function(ctx, &ctx->module->functions[i]))
            return false;
    }
    
    // ✅ 同步并输出
    emitter_sync_fast(ctx->code_emitter);
    emitter_flush(ctx->code_emitter);
    
    // 将 emitter 主缓冲区内容复制到 output
    silver_buffer_append(output, ctx->code_emitter->primary);
    
    return true;
}