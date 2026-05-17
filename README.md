![CI](https://github.com/menghuihuan44/silver/actions/workflows/ci.yml/badge.svg)

# **Silver Compiler Backend**
**一个快速、自包含、跨平台的编译器后端，追求编译速度与代码质量的平衡。**

# ***前提提要***
***本项目在初始构建以及代码实现等方面使用到了AI，本人不反对使用AI，本人认为如果能够保证可维护性，实现正确，测试全通过，代码质量优秀等要求，那么AI就是一个很好的工具。***

# **设计理念**
Silver 将优化Pass与代码生成紧密集成为一个流水线，避免了传统编译器"中端→后端"的中间表示转换开销。它不区分优化等级——始终生成较高质量代码，无需 -O0/-O1/-O2 选择。

## 架构
```text
IR构建 → [常量传播 → 代数简化 → 强度削弱 → 复写传播 → DCE → 窥孔] × N次迭代
         → 指令选择(集成RA) → 机器编码 → ELF/PE/Mach-O输出
```
- **SSA形式IR**：16字节定长指令，数组存储，值池管理
- **5个优化Pass**：多遍迭代直到收敛
- **表驱动指令选择**：三平台共享ISel引擎，平台特定匹配表
- **集成寄存器分配**：线性扫描+贪心溢出
- **自包含输出**：无需外部汇编器或链接器

## **性能定位**
```text
        编译速度              代码质量
TCC    ██████████ 极快       ██░░░░░░░░ 低
QBE    ████████░░ 很快       ███████░░░ 接近-O1
Silver ████████░░ 很快       ███████░░░ 接近-O1
GCC    ██░░░░░░░░ 慢         ██████████ -O2优化
LLVM   █░░░░░░░░░ 最慢       ██████████ -O2优化
```
Silver 的编译速度快，在保持接近 GCC/Clang -O1 代码质量的同时，吞吐量超过传统编译器后端。Silver 的编译速度和代码质量和 QBE 的处于差不多的地位。

# **快速开始**

## **构建**
```bash
# 依赖：CMake >= 3.16, C17编译器 (GCC/Clang/MSVC)
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . -j$(nproc)
```

## **使用示例**
```c
#include "silver/silver.h"
#include "silver/ir/ir_builder.h"

int main() {
    // 创建编译上下文
    SilverContext *ctx = silver_context_create();
    
    // 创建IR模块
    IRModule *module = ir_module_create("hello", "x86_64-unknown-none");
    IRBuilder *builder = ir_builder_create(module);
    
    // 构建函数
    IRFunction *func = ir_module_add_function(module, "hello", 0);
    ir_builder_set_position(builder, func, NULL);
    IRBlock *entry = ir_builder_create_block(builder, "entry");
    ir_builder_set_insert_point(builder, entry);
    
    // 生成IR: return 42
    IRValueId c42 = ir_builder_int_const(builder, builder->i64_type, 42);
    ir_builder_ret(builder, c42);
    
    // 编译到目标文件
    SilverCompileOptions opts = silver_options_default();
    opts.output_file = "hello.o";
    bool ok = silver_compile_to_file(ctx, module, &opts);
    
    // 清理
    ir_builder_destroy(builder);
    ir_module_destroy(module);
    silver_context_destroy(ctx);
    return ok ? 0 : 1;
}
```

## **命令行工具**
```bash
# 编译IR到x86-64目标文件
./silverc -target x86-64 -o output.o input.ir

# 编译到ARM64
./silverc -target arm64 -o output.o input.ir

# 带调试信息
./silverc -target x86-64 -g -o output.o input.ir

# 详细输出
./silverc -target x86-64 -v -o output.o input.ir
```

# **平台支持**
| 目标 | 状态 | 整数 | 浮点 | 输出格式 |
|:-----|:-----|:-----|:-----|:-----|
| x86_64 | 完整 | ✅ | ✅ SSE/SSE2 | ELF, PE, Mach-O |
| ARM64 (AArch64) | 完整 | ✅ | ✅ | ELF, PE, Mach-O |
| RISC-V64 |  完整 | ✅ | ✅ F/D扩展 | ELF |

## **已知限制**
- **PHI节点**：支持两路分支（if-else），不支持多路分支和复杂控制流合并
- **32位目标**：仅支持64位架构
- **向量化**：不支持自动向量化
- **链接时优化**：输出目标文件，需外部链接器完成最终链接
- **调试信息**：DWARF5基础支持，行号信息不完整

# **优化Pass详解**
| Pass | 描述 | 示例 |
|:-----|:-----|:-----|
| 常量折叠+传播 | 多遍扫描，将常量链折叠 | ```((1+2)*(3+4)-10)/2 → 5``` |
| 代数简化 | 恒等式消除 | ```x+0→x, x*1→x, x&x→x, x\|0→x``` |
| 强度削弱 | 昂贵操作替换 | ```x*8→x<<3, x/4→x>>2, x%16→x&15``` |
| 死代码消除 | 标记-清除算法 | 消除未使用的计算 |
| 复写传播 | 多级copy链展开	| ```a=copy b; c=copy a → c=copy b``` |
| 窥孔优化 | 指令序列简化 | 连续copy消除、冗余load消除 |
| 结合律 | 常量重排	| ```(x+5)+10 → x+15``` |

优化Pass多遍迭代直到收敛（最多10遍），确保每遍都充分利用前一遍暴露的优化机会。

## **基准数据**
**代码质量**（6个测试用例，Benchmark vs Optimized）：
| Pass | 代码缩小 | 估算加速 |
|:-----|:-----|:-----|
| Dead Code Elimination	| 61.6% | 4.67× |
| Constant Folding | 2.9% | 1.02× |
| Algebraic Simplification | 2.4% | 1.02× |
| Copy Propagation | 1.7% | 1.03× |
| Strength Reduction | 1.4%	| 1.01× |
| Reassociation | 1.2% | 1.02× |
| **综合** | **22.1%** | **1.63×** |

**编译速度**（Arith Chain, 1000条IR指令）：
| 阶段 | 耗时 | 占比 |
|:-----|:-----|:-----|
| IR构建 | ~27 µs | 40% |
| 优化 | ~28 µs | 43% |
| 代码生成 | ~11 µs | 17% |
| **总计** | **~66 µs** | |

# **项目结构**
```text
silver/
├── include/silver/          # 公共头文件
│   ├── silver.h             # 主API
│   ├── ir/                  # IR系统（类型、值、指令、模块、构建器）
│   ├── opt/                 # 优化Pass
│   ├── codegen/             # 指令选择、寄存器分配、代码发射
│   ├── target/x86/          # x86-64平台
│   ├── target/arm/          # ARM64平台
│   ├── target/riscv/        # RISC-V64平台
│   ├── debug/               # DWARF调试信息
│   ├── link/                # ELF/PE/Mach-O输出
│   └── support/             # Arena分配器、缓冲区、错误处理
├── src/                     # 实现文件
├── tools/driver/            # 命令行驱动
├── tools/dump/              # IR转储工具
├── tools/analyzer/          # 性能分析工具
├── tests/unit/              # 单元测试
├── tests/integration/       # 集成测试
├── tests/benchmarks/        # 性能基准测试
├── examples/                # 使用示例
└── docs/                    # 文档
```
# **测试**
```bash
# 运行所有测试
cmake --build . --target check

# 仅单元测试
cmake --build . --target check-unit

# 仅集成测试
cmake --build . --target check-integration

# 性能基准测试
cmake --build . --target benchmark
```

# **架构限制与不适用场景**
## **不适合的场景**
- **需要-O0快速编译**：Silver始终运行完整优化，编译最小程序也有固定开销（~10µs函数框架）
- **需要深度优化**：无向量化、循环优化、内联展开、全局值编号、别名分析
- **多路PHI合并**：无法处理switch语句的PHI节点
- **需要JIT内存管理**：Silver生成目标文件，不提供内存中执行接口
- **32位目标**：仅支持64位架构

## **适合的场景**
- **轻量级AOT编译**：嵌入式、WebAssembly编译器
- **JIT后端**：低延迟编译，快速完成优化+代码生成
- **教学/研究**：代码量小（~3万行），完整编译器后端实现
- **需要自包含**：不依赖LLVM/GCC工具链

# **构建选项**
| 选项 | 默认 | 描述 |
|:-----|:-----|:-----|
| ```CMAKE_BUILD_TYPE``` | Release | Debug/Release/RelWithDebInfo/MinSizeRel |
| ```SILVER_BUILD_SHARED``` | ON | 构建共享库 |
| ```SILVER_BUILD_STATIC``` | ON | 构建静态库 |
| ```SILVER_BUILD_TESTS``` | ON | 构建测试 |
| ```SILVER_BUILD_BENCHMARKS``` | ON | 构建基准测试 |
| ```SILVER_BUILD_EXAMPLES``` | ON | 构建示例 |
| ```SILVER_BUILD_TOOLS``` | ON | 构建工具 |
| ```SILVER_WARNINGS_AS_ERRORS``` | OFF | 警告视为错误 |
| ```SILVER_ENABLE_PROFILING``` | OFF | 启用性能分析 |

# **致歉**
**本人的英文不好，所以很多地方只能使用中文，非常抱歉。同时本项目的注释也少且不够清晰，再次致以歉意。**

# **致谢**
Silver 的设计受以下项目启发：
- [QBE](https://c9x.me/compile/) — **简洁的编译器后端设计**
- [LLVM](https://llvm.org/) — **表驱动指令选择和SSA优化理论**
- [TCC](https://bellard.org/tcc/) — **极速编译理念**