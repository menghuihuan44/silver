# PE/COFF 输出格式

Windows 系统的目标文件格式。

## 生成内容

| 节 | 特征 | 内容 |
|----|------|------|
| .text | CODE + EXECUTE + READ | 机器代码 |
| .data | INITIALIZED_DATA + READ + WRITE | 已初始化数据 |
| .bss | READ + WRITE | 未初始化数据 |

## 头结构

```
COFF Header (20 bytes)
PE Optional Header (112 bytes)
Section Headers × 3 (40 bytes each)
Code (.text)
Data (.data)
```

## 对齐

- 文件对齐：512 字节
- 节对齐：4096 字节

## 限制

- 无重定位信息
- 无符号表（COFF 符号表留空）
- 入口点默认 0

## 实现文件

- `src/link/pe.c`