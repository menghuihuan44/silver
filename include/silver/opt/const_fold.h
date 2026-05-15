#ifndef SILVER_OPT_CONST_FOLD_H
#define SILVER_OPT_CONST_FOLD_H

#include "silver/opt/passes.h"

// 尝试对单个指令进行常量折叠
// 返回true如果做了修改
bool const_fold_instruction(OptContext *ctx, IRFunction *func, 
                             uint32_t inst_idx, IRInst *inst);

#endif // SILVER_OPT_CONST_FOLD_H