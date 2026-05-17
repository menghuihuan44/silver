#include "silver/support/arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>

// 内部函数：分配新的Arena块
static ArenaBlock *arena_add_block(Arena *arena, size_t min_size) {
    size_t block_size = SILVER_ARENA_BLOCK_SIZE;
    if (min_size > block_size) {
        block_size = min_size;
    }
    
    ArenaBlock *block = (ArenaBlock *)calloc(1, sizeof(ArenaBlock));
    if (!block) return NULL;
    
    block->data = (uint8_t *)malloc(block_size);
    if (!block->data) {
        free(block);
        return NULL;
    }
    
    block->size = block_size;
    block->used = 0;
    block->next = NULL;
    
    arena->current->next = block;
    arena->current = block;
    arena->total_allocated += block_size;
    arena->num_blocks++;
    
    return block;
}

Arena *arena_create(void) {
    Arena *arena = (Arena *)calloc(1, sizeof(Arena));
    if (!arena) return NULL;
    
    ArenaBlock *block = (ArenaBlock *)calloc(1, sizeof(ArenaBlock));
    if (!block) {
        free(arena);
        return NULL;
    }
    
    block->data = (uint8_t *)malloc(SILVER_ARENA_BLOCK_SIZE);
    if (!block->data) {
        free(block);
        free(arena);
        return NULL;
    }
    
    block->size = SILVER_ARENA_BLOCK_SIZE;
    block->used = 0;
    block->next = NULL;
    
    arena->head = block;
    arena->current = block;
    arena->total_allocated = SILVER_ARENA_BLOCK_SIZE;
    arena->total_used = 0;
    arena->num_blocks = 1;
    
    return arena;
}

void arena_destroy(Arena *arena) {
    if (!arena) return;
    
    ArenaBlock *block = arena->head;
    while (block) {
        ArenaBlock *next = block->next;
        free(block->data);
        free(block);
        block = next;
    }
    
    free(arena);
}

void *arena_alloc(Arena *arena, size_t size) {
    return arena_alloc_aligned(arena, size, SILVER_ARENA_ALIGNMENT);
}

void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment) {
    if (!arena || size == 0) return NULL;
    if (alignment & (alignment - 1)) return NULL;
    
    ArenaBlock *block = arena->current;
    
    // 计算对齐后的偏移
    size_t aligned_offset = (block->used + alignment - 1) & ~(alignment - 1);
    
    // 如果当前块空间不足，分配新块
    if (aligned_offset + size > block->size) {
        ArenaBlock *new_block = arena_add_block(arena, size + alignment);
        if (!new_block) return NULL;
        
        block = new_block;
        aligned_offset = 0;
    }
    
    void *ptr = block->data + aligned_offset;
    block->used = aligned_offset + size;
    arena->total_used += size;
    
    return ptr;
}

void *arena_calloc(Arena *arena, size_t size) {
    void *ptr = arena_alloc(arena, size);
    if (ptr) {
        memset(ptr, 0, size);
    }
    return ptr;
}

void arena_reset(Arena *arena) {
    if (!arena) return;
    
    ArenaBlock *block = arena->head;
    while (block) {
        block->used = 0;
        block = block->next;
    }
    
    arena->current = arena->head;
    arena->total_used = 0;
}

ArenaStats arena_get_stats(const Arena *arena) {
    ArenaStats stats = {0};
    if (!arena) return stats;
    
    stats.total_allocated = arena->total_allocated;
    stats.total_used = arena->total_used;
    stats.num_blocks = arena->num_blocks;
    stats.utilization = arena->total_allocated > 0 
        ? (float)arena->total_used / (float)arena->total_allocated 
        : 0.0f;
    
    return stats;
}

ArenaMarker arena_mark(Arena *arena) {
    ArenaMarker marker = {NULL, 0};
    if (!arena) return marker;
    
    marker.block = arena->current;
    marker.offset = arena->current->used;
    return marker;
}

void arena_rollback(Arena *arena, ArenaMarker marker) {
    if (!arena || !marker.block) return;
    
    // 释放标记块之后的所有块
    ArenaBlock *block = marker.block->next;
    while (block) {
        ArenaBlock *next = block->next;
        arena->total_allocated -= block->size;
        arena->num_blocks--;
        free(block->data);
        free(block);
        block = next;
    }
    
    marker.block->next = NULL;
    marker.block->used = marker.offset;
    arena->current = marker.block;
    arena->total_used = 0;
    
    // 重新计算total_used
    block = arena->head;
    while (block != marker.block) {
        arena->total_used += block->size;
        block = block->next;
    }
    arena->total_used += marker.offset;
}

char *arena_strdup(Arena *arena, const char *str) {
    if (!arena || !str) return NULL;
    
    size_t len = strlen(str);
    char *copy = (char *)arena_alloc(arena, len + 1);
    if (copy) {
        memcpy(copy, str, len + 1);
    }
    return copy;
}

char *arena_strndup(Arena *arena, const char *str, size_t n) {
    if (!arena || !str) return NULL;
    
    size_t len = strnlen(str, n);
    char *copy = (char *)arena_alloc(arena, len + 1);
    if (copy) {
        memcpy(copy, str, len);
        copy[len] = '\0';
    }
    return copy;
}

char *arena_sprintf(Arena *arena, const char *fmt, ...) {
    if (!arena || !fmt) return NULL;
    
    va_list args;
    va_start(args, fmt);
    
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    
    if (len < 0) {
        va_end(args);
        return NULL;
    }
    
    char *buf = (char *)arena_alloc(arena, len + 1);
    if (buf) {
        vsnprintf(buf, len + 1, fmt, args);
    }
    
    va_end(args);
    return buf;
}