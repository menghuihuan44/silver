#ifndef SILVER_ARENA_H
#define SILVER_ARENA_H

#include <stddef.h>
#include <stdint.h>
#include <stdalign.h>

#ifdef __cplusplus
extern "C" {
#endif

// Arena分配器 - 用于模块生命周期的快速内存管理
// 采用块链式设计，支持快速分配和批量释放

#define SILVER_ARENA_BLOCK_SIZE (64 * 1024)  // 64KB默认块大小
#define SILVER_ARENA_ALIGNMENT 16            // 16字节对齐

typedef struct ArenaBlock {
    struct ArenaBlock *next;
    uint8_t *data;
    size_t size;       // 块总大小
    size_t used;       // 已使用大小
} ArenaBlock;

typedef struct {
    ArenaBlock *head;        // 块链表头
    ArenaBlock *current;     // 当前活跃块
    size_t total_allocated;  // 总共分配大小
    size_t total_used;       // 总共使用大小
    size_t num_blocks;       // 块数量
} Arena;

// 创建新的Arena
Arena *arena_create(void);

// 销毁Arena并释放所有内存
void arena_destroy(Arena *arena);

// 从Arena分配内存
void *arena_alloc(Arena *arena, size_t size);

// 从Arena分配对齐内存
void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment);

// 从Arena分配清零内存
void *arena_calloc(Arena *arena, size_t size);

// 重置Arena（释放所有块回到初始状态，但不释放内存）
void arena_reset(Arena *arena);

// 获取Arena统计信息
typedef struct {
    size_t total_allocated;
    size_t total_used;
    size_t num_blocks;
    float utilization;  // 使用率 0.0-1.0
} ArenaStats;

ArenaStats arena_get_stats(const Arena *arena);

// 临时标记Arena位置，用于回滚
typedef struct {
    ArenaBlock *block;
    size_t offset;
} ArenaMarker;

ArenaMarker arena_mark(Arena *arena);
void arena_rollback(Arena *arena, ArenaMarker marker);

// 字符串复制到Arena
char *arena_strdup(Arena *arena, const char *str);
char *arena_strndup(Arena *arena, const char *str, size_t n);

// 格式化字符串到Arena
char *arena_sprintf(Arena *arena, const char *fmt, ...)
    __attribute__((format(printf, 2, 3)));

#ifdef __cplusplus
}
#endif

#endif // SILVER_ARENA_H