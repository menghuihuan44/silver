# 贡献指南

## 项目结构

```
silver/
├── include/silver/   # 公共头文件（API）
├── src/               # 实现文件
│   ├── ir/            # IR 系统
│   ├── opt/           # 优化 Pass
│   ├── codegen/       # 代码生成
│   ├── target/        # 目标平台
│   ├── debug/         # DWARF 调试
│   ├── link/          # 输出格式
│   └── support/       # 基础设施
├── tools/             # 命令行工具
├── tests/             # 测试
│   ├── unit/          # 单元测试
│   ├── integration/   # 集成测试
│   └── benchmarks/    # 基准测试
├── examples/          # 示例
└── docs/              # 文档
```

## 构建和测试

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug -DSILVER_ENABLE_ASSERTIONS=ON
cmake --build . -j$(nproc)
cmake --build . --target check
```

## 代码风格

- C17 标准
- 缩进：4 空格
- 函数命名：`module_action_descriptor`（如 `ir_type_create_int`）
- 结构体：`PascalCase`（如 `CodeGenContext`）
- 枚举：`UPPER_SNAKE` 前缀（如 `IR_OP_ADD`）
- 宏：`UPPER_SNAKE`（如 `IR_VALUE_ID_INVALID`）

## 添加新的优化 Pass

1. 在 `include/silver/opt/` 中创建头文件
2. 在 `src/opt/` 中创建实现文件
3. 在 `src/opt/passes.c` 的 `opt_run_all` 中注册
4. 在 `tests/unit/test_opt.c` 中添加测试
5. 在 `tests/benchmarks/bench_code_quality.c` 中添加基准测试用例

## 添加新的目标平台

1. 在 `include/silver/target/<arch>/` 中创建头文件
2. 在 `src/target/<arch>/` 中创建实现文件（主文件、编码器、匹配表）
3. 在 `include/silver/codegen/machine.h` 中添加架构枚举
4. 在 `src/silver.c` 的 `silver_target_create` 中添加分支
5. 在 `tests/unit/test_targets.c` 中添加测试

## 提交规范

- 提交信息：`component: brief description`
- 示例：`isel: add opcode index for faster lookup`
- 每个提交应该是原子性的、可单独测试的

## 已知限制

以下功能目前未实现，欢迎贡献：

- 多路 PHI 节点（>2 前驱）
- 32 位目标（x86、ARM32、RISC-V32）
- 全局值编号（GVN）
- 循环优化（LICM、循环展开）
- 内联展开
- 向量化
- JIT 内存执行接口
- 完整的 DWARF 行号信息

## 联系方式

- Issues: GitHub Issues
- 讨论: GitHub Discussions