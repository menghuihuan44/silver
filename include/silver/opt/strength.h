#ifndef SILVER_OPT_STRENGTH_H
#define SILVER_OPT_STRENGTH_H

#include "silver/opt/passes.h"

// 对单个指令进行强度削弱
bool strength_reduce_instruction(OptContext *ctx, IRFunction *func,
                                  uint32_t inst_idx, IRInst *inst);

#endif // SILVER_OPT_STRENGTH_H