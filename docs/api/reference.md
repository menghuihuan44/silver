# API 参考

## 编译上下文

```c
SilverContext *silver_context_create(void);
void silver_context_destroy(SilverContext *ctx);
```

## 编译选项

```c
typedef struct {
    SilverTargetArch target_arch;    // SILVER_TARGET_X86_64 / ARM64 / RISCV64
    const char *target_triple;      // "x86_64-unknown-none" 等
    const char *output_file;        // 输出文件名
    bool optimize;                  // 始终优化（保留字段）
    bool debug_info;                // 生成 DWARF 调试信息
    bool pic;                       // 位置无关代码
    bool verbose;                   // 详细输出
    enum { SILVER_OUTPUT_OBJECT, SILVER_OUTPUT_ASM, SILVER_OUTPUT_RAW } output_format;
    enum { SILVER_FORMAT_ELF, SILVER_FORMAT_PE, SILVER_FORMAT_MACHO } object_format;
} SilverCompileOptions;

SilverCompileOptions silver_options_default(void);
```

## 编译

```c
SilverCompileResult *silver_compile(SilverContext *ctx, IRModule *module,
                                     const SilverCompileOptions *options);

bool silver_compile_to_file(SilverContext *ctx, IRModule *module,
                              const SilverCompileOptions *options);

void silver_compile_result_destroy(SilverCompileResult *result);
```

## 编译结果

```c
typedef struct {
    SilverBuffer *code;        // 生成的代码
    SilverBuffer *data;        // 数据段
    SilverBuffer *debug_info;  // 调试信息
    uint64_t entry_point;
    uint32_t code_size;
    uint32_t data_size;
} SilverCompileResult;
```

## 目标平台

```c
SilverTarget *silver_target_create(SilverTargetArch arch);
void silver_target_destroy(SilverTarget *target);
SilverTargetArch silver_target_from_triple(const char *triple);
const char *silver_target_arch_name(SilverTargetArch arch);
```

## IR 类型（详见 IR 系统章节）

```c
IRTypeId ir_type_create_int(IRTypeTable *t, IRIntWidth w, bool signed);
IRTypeId ir_type_create_float(IRTypeTable *t, IRFloatWidth w);
IRTypeId ir_type_create_ptr(IRTypeTable *t, IRTypeId pointee);
IRTypeId ir_type_create_func(IRTypeTable *t, IRTypeId ret, uint32_t n, IRTypeId *params, bool variadic);
```

## IR 值（详见 IR 系统章节）

```c
IRValueId ir_value_create_int_const(IRValuePool *p, IRTypeId t, int64_t v);
IRValueId ir_value_create_float_const(IRValuePool *p, IRTypeId t, double v);
IRValue *ir_value_get(IRValuePool *p, IRValueId id);
```

## 内存管理

```c
Arena *arena_create(void);
void arena_destroy(Arena *arena);
void *arena_alloc(Arena *arena, size_t size);
void *arena_calloc(Arena *arena, size_t size);
SilverBuffer *silver_buffer_create(size_t initial_size);
void silver_buffer_destroy(SilverBuffer *buffer);
```

## 错误处理

```c
SilverErrorContext *silver_error_context_create(void);
bool silver_error_has_errors(const SilverErrorContext *ctx);
void silver_error_print_all(const SilverErrorContext *ctx, FILE *out);
```