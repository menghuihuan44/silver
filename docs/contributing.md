# 贡献指南

感谢你对 Silver 编译器后端的兴趣！本文档将帮助你了解如何贡献代码、报告问题以及参与项目开发。

## 行为准则

- 尊重所有贡献者
- 建设性的代码评审
- 专注于技术讨论

## 如何贡献

### 报告问题

如果你发现 bug 或有功能建议，请在 GitHub Issues 中提交。

**bug 报告应包含**：

```
- 操作系统和版本（如 Ubuntu 24.04, macOS 15.0, Windows 11）
- 编译器版本（gcc --version 或 clang --version）
- CMake 版本
- 复现步骤
- 期望行为 vs 实际行为
- 错误日志（如果有）
```

### 提交代码

1. **Fork** 本仓库
2. 创建特性分支：`git checkout -b feature/your-feature-name`
3. 编写代码和测试
4. 确保所有测试通过：`cmake --build build --target check`
5. 提交：`git commit -m "component: brief description"`
6. 推送：`git push origin feature/your-feature-name`
7. 创建 Pull Request

### 提交信息规范

```
<组件>: <简短描述>

<详细描述（可选）>
```

示例：

```
isel: add opcode index for faster lookup

Replace linear scan of match table with O(1) hash index.
Improves instruction selection throughput by ~15%.

const_fold: fix constant propagation for nested chains

Previously only handled two levels of depth.
Now iterates until convergence (max 10 passes).
```

### 分支命名

| 前缀 | 用途 |
|------|------|
| `feature/` | 新功能 |
| `fix/` | Bug 修复 |
| `docs/` | 文档更新 |
| `test/` | 测试相关 |
| `refactor/` | 重构（无功能变化） |

## 项目结构

```
silver/
├── include/silver/          # 公共头文件
│   ├── silver.h             # 主 API
│   ├── ir/                  # IR 系统
│   │   ├── ir_module.h      # 模块
│   │   ├── ir_inst.h        # 指令定义
│   │   ├── ir_value.h       # 值系统
│   │   ├── ir_type.h        # 类型系统
│   │   └── ir_builder.h     # IR 构建器
│   ├── opt/                 # 优化 Pass
│   │   ├── passes.h         # 优化流水线
│   │   ├── const_fold.h     # 常量折叠
│   │   ├── algebraic.h      # 代数简化
│   │   ├── strength.h       # 强度削弱
│   │   ├── dce.h            # 死代码消除
│   │   └── copy_prop.h      # 复写传播
│   ├── codegen/             # 代码生成
│   │   ├── isel.h           # 指令选择
│   │   ├── machine.h        # 机器指令定义
│   │   └── emitter.h        # 代码发射器
│   ├── target/              # 目标平台
│   │   ├── x86/             # x86-64
│   │   ├── arm/             # ARM64
│   │   └── riscv/           # RISC-V64
│   ├── debug/dwarf.h        # DWARF 调试信息
│   ├── link/                # 输出格式
│   │   ├── elf.h
│   │   ├── pe.h
│   │   └── macho.h
│   └── support/             # 基础设施
│       ├── arena.h          # Arena 分配器
│       ├── buffer.h         # 缓冲区管理
│       └── error.h          # 错误处理
├── src/                     # 实现文件（与 include 对应）
├── tools/                   # 命令行工具
│   ├── driver/main.c        # silverc 编译器驱动
│   ├── dump/dump.c          # IR 转储工具
│   └── analyzer/silver_analyze.c  # 性能分析工具
├── tests/
│   ├── unit/                # 单元测试
│   ├── integration/         # 集成测试
│   └── benchmarks/          # 基准测试
├── examples/                # 使用示例
├── docs/                    # 文档
└── cmake/                   # CMake 模块
```

## 代码规范

### C 标准

- 使用 **C17**（`-std=gnu17`）
- 允许 GCC/Clang 扩展（`typeof`, `__attribute__` 等）
- 不兼容 MSVC（不需要 `#ifdef _MSC_VER`）

### 命名规范

| 类型 | 规范 | 示例 |
|------|------|------|
| 函数 | `模块_动作_描述` | `ir_type_create_int`, `isel_select_instruction` |
| 结构体 | `PascalCase` | `CodeGenContext`, `MachineInstExt` |
| 枚举类型 | `PascalCase` | `IROpcode`, `RegisterClass` |
| 枚举值 | `UPPER_SNAKE_` 前缀 | `IR_OP_ADD`, `REG_CLASS_GPR` |
| 宏 | `UPPER_SNAKE` | `IR_VALUE_ID_INVALID`, `SILVER_ARENA_BLOCK_SIZE` |
| 局部变量 | `snake_case` | `inst_count`, `block_idx` |
| 全局变量 | `g_` 前缀 | `g_error_context` |

### 缩进和格式

- 4 空格缩进
- 大括号：K&R 风格（左大括号不换行）
- 每行不超过 100 字符
- 函数之间空一行

```c
// 正确
if (condition) {
    do_something();
} else {
    do_other();
}

// 正确
static bool safe_add_i64(int64_t a, int64_t b, int64_t *result) {
    if ((b > 0 && a > INT64_MAX - b) ||
        (b < 0 && a < INT64_MIN - b)) {
        return false;
    }
    *result = a + b;
    return true;
}
```

### 头文件

- 所有头文件使用 `#ifndef` 防护
- 命名：`SILVER_<PATH>_H`

```c
#ifndef SILVER_IR_TYPE_H
#define SILVER_IR_TYPE_H

// ... 内容 ...

#endif // SILVER_IR_TYPE_H
```

### 错误处理

- 库函数通过 `SilverErrorContext` 报告错误，不直接 `abort()` 或 `exit()`
- 致命错误用 `SILVER_ERROR_FATAL`
- 每个错误必须包含可操作的描述

```c
silver_error_add(ctx->error_ctx, SILVER_ERROR_ERROR,
    SILVER_ERR_IR_INVALID_TYPE, &location,
    "Type %u does not support operation '%s'",
    type_id, ir_opcode_name(opcode));
```

## 添加新的优化 Pass

### 步骤

1. 在 `include/silver/opt/` 中创建头文件：

```c
#ifndef SILVER_OPT_YOUR_PASS_H
#define SILVER_OPT_YOUR_PASS_H

#include "silver/opt/passes.h"

bool opt_your_pass(OptContext *ctx);

#endif
```

2. 在 `src/opt/` 中创建实现文件
3. 在 `src/opt/passes.c` 的 `opt_run_all` 中注册
4. 在 `src/opt/` 目录的 `CMakeLists.txt`（通过顶层 `CMakeLists.txt` 的 `SILVER_OPT_SOURCES`）中添加
5. 在 `tests/unit/test_opt.c` 中添加测试
6. 在 `tests/benchmarks/bench_code_quality.c` 中添加基准测试用例
7. 在 `docs/optimization/` 中添加文档

### Pass 要求

- 复杂度不超过 O(n log n)
- 不能引入新的 IR 指令（只能修改现有指令或消除死代码）
- 必须保证正确性：优化后的代码与优化前语义等价
- 通过 `OptPassStats` 记录优化统计

## 添加新的目标平台

### 步骤

1. 创建目录结构：
   - `include/silver/target/<arch>/<arch>.h`
   - `include/silver/target/<arch>/<arch>_isel.h`
   - `include/silver/target/<arch>/<arch>_encode.h`
   - `src/target/<arch>/<arch>.c`
   - `src/target/<arch>/<arch>_isel.c`
   - `src/target/<arch>/<arch>_encode.c`

2. 实现目标平台接口：

```c
// <arch>.c 中必须实现：
SilverTarget *<arch>_target_create(void);
void <arch>_target_destroy(SilverTarget *target);

// 编码器：
bool <arch>_encode_instruction(const SilverTarget *, const MachineInstExt *,
                                uint8_t *buffer, uint32_t *length);

// 函数框架：
uint32_t <arch>_emit_prologue(const SilverTarget *, IRFunction *, uint8_t *);
uint32_t <arch>_emit_epilogue(const SilverTarget *, IRFunction *, uint8_t *);
```

3. 定义匹配表（`<arch>_isel.c`）：

```c
const MatchEntry <arch>_match_table[] = {
    { .ir_opcode = IR_OP_ADD, ... },
    { .ir_opcode = IR_OP_SUB, ... },
    // 覆盖所有需要的 IR 操作码
};
const uint32_t <arch>_match_table_size = sizeof(...) / sizeof(...);
```

4. 在 `include/silver/codegen/machine.h` 中添加 `SilverTargetArch` 枚举值
5. 在 `src/silver.c` 的 `silver_target_create` 中添加分支
6. 在 `CMakeLists.txt` 的 `SILVER_TARGET_SOURCES` 中添加源文件
7. 在 `tests/unit/test_targets.c` 中添加测试

## 测试规范

### 单元测试

- 每个模块一个测试文件
- 使用 `TEST` 和 `RUN_TEST` 宏
- 覆盖正常路径和边界情况

```c
TEST(my_feature) {
    // 设置
    // 执行
    // 断言
    ASSERT(result == expected);
}

// main 中
RUN_TEST(my_feature);
```

### 集成测试

- 测试完整编译流水线
- 覆盖多平台
- 覆盖多种输出格式

### 基准测试

- 测试编译速度
- 测试代码质量
- 提供基线对比

### 运行测试

```bash
# 完整测试
cmake --build build --target check

# 仅单元测试
cmake --build build --target check-unit

# 仅基准测试（Release）
cmake --build build --target benchmark
```

## Pull Request 检查清单

提交 PR 前确认：

- [ ] 代码通过 `cmake --build build --target check`
- [ ] 基准测试通过（Release 模式）
- [ ] 新功能有单元测试
- [ ] 新功能有文档
- [ ] 代码符合命名和格式规范
- [ ] 没有遗留调试输出（`printf("DEBUG:`）
- [ ] 没有未使用的变量和函数
- [ ] 提交信息符合规范

## 获取帮助

- **Issues**: 技术问题和 bug 报告
- **Discussions**: 功能建议和一般讨论

## 许可证

贡献的代码将在 Apache-2.0 许可证下发布。提交代码即表示你同意此条款。