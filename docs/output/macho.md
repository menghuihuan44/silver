# Mach-O 输出格式

macOS/iOS 系统的目标文件格式。

## 生成内容

| 段 | 节 | 内容 |
|----|-----|------|
| __TEXT | __text | 机器代码 |
| __DATA | __data | 已初始化数据 |

## 头结构

```
Mach-O Header 64 (32 bytes)
Segment Command 64 × 2 (72 bytes each)
Section Header 64 × 2 (80 bytes each)
Code + Data
```

## 支持的 CPU 类型

| 架构 | CPU Type |
|------|---------|
| x86-64 | CPU_TYPE_X86_64 |
| ARM64 | CPU_TYPE_ARM64 |

## 限制

- 无符号表（nlist 留空）
- 无重定位信息
- 无代码签名
- 不支持通用二进制（Universal Binary）

## 实现文件

- `src/link/macho.c`