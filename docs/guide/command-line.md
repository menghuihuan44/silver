# 命令行工具

Silver 提供 `silverc` 命令行驱动，用于将 IR 文件编译为目标文件。

## 基本用法

```bash
silverc -target <arch> [-o <output>] [-g] [-v] <input.ir>
```

## 选项

| 选项 | 说明 |
|------|------|
| `-target <arch>` | 目标架构：`x86-64`, `arm64`, `riscv64` |
| `-o <file>` | 输出文件（默认：`output.o`） |
| `-g` | 生成 DWARF 调试信息 |
| `-v` `--verbose` | 详细输出（显示优化统计、编译时间） |
| `-h` `--help` | 帮助信息 |
| `--version` | 版本信息 |

## 示例

```bash
# 编译到 x86-64 目标文件
silverc -target x86-64 -o program.o program.ir

# 带调试信息
silverc -target arm64 -g -o program.o program.ir

# 详细输出
silverc -target x86-64 -v -o program.o program.ir
```

## 输出格式

默认输出 ELF 格式。可通过 API 指定 PE 或 Mach-O：

```c
SilverCompileOptions opts = silver_options_default();
opts.object_format = SILVER_FORMAT_PE;    // Windows PE/COFF
opts.object_format = SILVER_FORMAT_MACHO; // macOS Mach-O
opts.object_format = SILVER_FORMAT_ELF;   // Linux ELF（默认）
```

## 链接

Silver 生成**可重定位目标文件**，需要用外部链接器完成最终链接：

```bash
# Linux（GNU ld）
ld program.o -o program

# Linux（GCC）
gcc program.o -o program

# macOS
ld program.o -o program -lSystem

# Windows（MSVC）
link program.obj /OUT:program.exe
```