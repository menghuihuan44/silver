# ELF 输出格式

Linux/Unix 系统的标准目标文件格式。

## 生成内容

| 节 | 类型 | 内容 |
|----|------|------|
| .text | SHT_PROGBITS | 机器代码（可执行+可读） |
| .data | SHT_PROGBITS | 已初始化数据（可读写） |
| .bss | SHT_NOBITS | 未初始化数据（占位） |
| .symtab | SHT_SYMTAB | 符号表（函数+全局变量） |
| .strtab | SHT_STRTAB | 字符串表 |
| .shstrtab | SHT_STRTAB | 节名称字符串表 |
| .debug_info | SHT_PROGBITS | DWARF 调试信息（可选） |

## 支持架构

| 架构 | ELF Machine |
|------|------------|
| x86-64 | EM_X86_64 (62) |
| ARM64 | EM_AARCH64 (183) |
| RISC-V64 | EM_RISCV (243) |

## 符号表

每个函数和全局变量都会创建对应的 ELF 符号：

```c
Elf64_Sym sym;
sym.st_name = <字符串表偏移>;
sym.st_info = (STB_GLOBAL << 4) | STT_FUNC;  // 全局函数
sym.st_shndx = 1;  // .text 节索引
sym.st_value = 0;  // 函数地址
sym.st_size = 0;   // 函数大小
```

## 限制

- 当前只生成 ET_REL（可重定位目标文件），不生成 ET_EXEC
- 重定位信息简化（函数地址默认置 0）
- 无程序头表（目标文件不需要）

## 实现文件

- `src/link/elf.c`