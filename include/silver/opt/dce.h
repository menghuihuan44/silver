#ifndef SILVER_OPT_DCE_H
#define SILVER_OPT_DCE_H

#include "silver/opt/passes.h"

// 死代码消除 - 标记-清除算法
bool opt_dead_code_eliminate(OptContext *ctx);

#endif // SILVER_OPT_DCE_H