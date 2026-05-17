# 代码发射器

## 双缓冲设计

`CodeEmitter` 维护两个 64KB 缓冲区：

```
┌──────────────┐     ┌──────────────┐
│  Primary     │ ←── │  Secondary   │
│  (写入中)    │     │  (空闲)      │
└──────────────┘     └──────────────┘
```

当 Primary 满时，flush 到输出并切换到 Secondary，写入不中断。

## 快速路径

```c
// 预分配空间（函数级别，一次确保64KB足够）
if (emitter->fast_remaining < 65536)
    emitter_flush(emitter);

// 直接写入（无边界检查）
uint8_t *ptr = emitter->fast_ptr;
memcpy(ptr, encode_buf, length);
ptr += length;

// 一次性回写
emitter->fast_ptr = ptr;
```

消除每条指令的边界检查，提升编码速度约 5-10%。

## 编码器接口

各平台编码器接收一个 `MachineInstExt` 和预分配的 64 字节栈缓冲区：

```c
typedef bool (*EncodeFunc)(const SilverTarget *target,
                            const MachineInstExt *inst,
                            uint8_t *buffer,      // 至少16字节
                            uint32_t *length);
```

### x86-64

变长编码（1-15 字节）。使用预计算的 ModRM 查找表（256项）和 SIB 查找表（256项），内联 REX 前缀生成。

### ARM64

定长 4 字节编码。直接计算 32 位指令字，一次写入。立即数编码使用 `encode_imm12` 位模式匹配。

### RISC-V64

定长 4 字节编码。使用预定义的宏（`RV_R`, `RV_I`, `RV_S`, `RV_B`, `RV_J`）构建指令字。

## 输出格式包装

编码完成后，根据目标格式包装：

```
原始机器码
    ↓
elf_write_object() / pe_write_object() / macho_write_object()
    ↓
ELF/PE/Mach-O 目标文件
```

## 实现文件

- `src/codegen/emitter.c`
- `src/target/x86/x86_encode.c`
- `src/target/arm/arm_encode.c`
- `src/target/riscv/riscv_encode.c`