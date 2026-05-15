#ifndef SILVER_TARGET_X86_ISEL_H
#define SILVER_TARGET_X86_ISEL_H

#include "silver/codegen/isel.h"
#include "silver/target/x86/x86.h"

// x86-64匹配表定义
// 这个表定义了从IR操作码到x86-64机器指令的映射

extern const MatchEntry x86_match_table[];
extern const uint32_t x86_match_table_size;

#endif // SILVER_TARGET_X86_ISEL_H