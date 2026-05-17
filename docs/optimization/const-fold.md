# 常量折叠与传播

## 功能

1. **常量折叠**：两个常量操作数的运算在编译时计算
2. **常量传播**：通过指令链前向传播已知常量值

## 常量折叠

当指令的**所有操作数都是字面常量**时，在编译时计算：

```
%1 = add const_3, const_4    →  %1 = copy const_7
%2 = mul const_5, const_6    →  %2 = copy const_30
%3 = div const_100, const_3  →  %3 = copy const_33
```

## 常量传播

维护一个 `is_const` 映射表，多遍扫描指令：

```
遍历 1:
  %1 = add const_1, const_2  →  %1 = copy const_3  （折叠）
  标记 %1 为常量值 3

遍历 2:
  %2 = add %1, const_4       →  %2 = copy const_7  （%1已知为3）
  标记 %2 为常量值 7

遍历 3:
  %3 = mul %2, const_2       →  %3 = copy const_14 （%2已知为7）
  标记 %3 为常量值 14

...直到收敛
```

## 支持的运算

- 所有整数算术（add, sub, mul, div, rem）
- 所有位操作（and, or, xor, shl, lshr, ashr）
- 所有整数比较
- 浮点算术和比较
- 类型转换（trunc, zext, sext, fptosi, sitofp 等）
- 取负（neg, fneg）

## 安全保证

所有运算都做了溢出检查。如果结果溢出（如 INT64_MIN / -1），则不折叠。

## 实现文件

- `src/opt/const_fold.c`