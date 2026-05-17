# 复写传播与窥孔优化

## 复写传播

### 功能

将 copy 指令的结果替换为其源操作数：

```
%a = copy %b
%c = add %a, 1     →    %c = add %b, 1
```

### 多级传播

迭代展开多级 copy 链：

```
%a = copy const_42
%b = copy %a
%c = add %b, 1

遍历 1:  %c = add %a, 1   （替换%b→%a）
遍历 2:  %c = add const_42, 1  （替换%a→const_42）
```

### 与常量传播的协作

复写传播将 copy 链展开后，常量传播可以进一步折叠：

```
%c = add const_42, 1  →  %c = copy const_43
```

## 窥孔优化

扫描连续的指令对，识别并简化特定模式：

### 支持的优化

1. **连续 copy**：`copy %a; copy (result of first)` → 消除第一条 copy
2. **冗余 load**：两次 load 同一地址 → 复用第一次结果
3. **无用 copy**：copy 结果未被使用 → 消除
4. **add+cmp**：标记供代码生成阶段合并（x86 上 ADD 已设置标志位）

## 实现文件

- `src/opt/copy_prop.c`