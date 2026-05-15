#ifndef SILVER_OPT_ALGEBRAIC_H
#define SILVER_OPT_ALGEBRAIC_H

#include "silver/opt/passes.h"

// 对单个指令进行代数简化
bool algebraic_simplify_instruction(OptContext *ctx, IRFunction *func,
                                     uint32_t inst_idx, IRInst *inst);

#endif // SILVER_OPT_ALGEBRAIC_H