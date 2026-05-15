// src/support/arena.c
#include "silver/support/arena.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// 内部函数：分配新的Arena块
static ArenaBlock *arena_new_block(size_t block_size) {
    ArenaBlock *block = (ArenaBlock *)malloc(sizeof(ArenaBlock) + block_size);
    if (!block) {
        fprintf(stderr, "FATAL: Arena block allocation failed\n");
        abort();
    }
    block->next = NULL;
    block->current = block->data;
    block->end = block->data + block_size;
    return block;
}

Arena *arena_create(void) {
    return arena_create_with_size(SILVER_ARENA_BLOCK_SIZE);
}

Arena *arena_create_with_size(size_t block_size) {
    Arena *arena = (Arena *)malloc(sizeof(Arena));
    if (!arena) {
        fprintf(stderr, "FATAL: Arena creation failed\n");
        abort();
    }
    
    // 确保块大小至少为4KB
    if (block_size < 4096) {
        block_size = 4096;
    }
    
    arena->block_size = block_size;
    arena->head = arena_new_block(block_size);
    arena->current_block = arena->head;
    arena->total_allocated = block_size;
    arena->total_used = 0;
    
    return arena;
}

void arena_destroy(Arena *arena) {
    if (!arena) return;
    
    // 释放所有块
    ArenaBlock *block = arena->head;
    while (block) {
        ArenaBlock *next = block->next;
        free(block);
        block = next;
    }
    
    free(arena);
}

void *arena_alloc_aligned(Arena *arena, size_t size, size_t alignment) {
    if (!arena || size == 0) return NULL;
    
    // 确保对齐至少为基本对齐
    if (alignment < SILVER_ARENA_ALIGNMENT) {
        alignment = SILVER_ARENA_ALIGNMENT;
    }
    
    // 在当前块中对齐分配
    ArenaBlock *block = arena->current_block;
    char *aligned = (char *)ARENA_ALIGN_PTR(block->current, alignment);
    
    // 检查当前块是否有足够空间
    if (aligned + size > block->end) {
        // 需要新块
        size_t new_block_size = arena->block_size;
        if (size > new_block_size / 2) {
            // 大对象分配专用块
            new_block_size = size + sizeof(ArenaBlock) + alignment;
        }
        
        ArenaBlock *new_block = arena_new_block(new_block_size);
        new_block->next = NULL;
        block->next = new_block;
        arena->current_block = new_block;
        arena->total_allocated += new_block_size;
        
        block = new_block;
        aligned = (char *)ARENA_ALIGN_PTR(block->current, alignment);
    }
    
    // 从当前块分配
    block->current = aligned + size;
    arena->total_used += size;
    
    // 清零分配的内存
    memset(aligned, 0, size);
    
    return aligned;
}

void *arena_alloc(Arena *arena, size_t size) {
    return arena_alloc_aligned(arena, size, SILVER_ARENA_ALIGNMENT);
}

void *arena_calloc(Arena *arena, size_t count, size_t size) {
    // arena_alloc已经清零，所以可以直接使用
    return arena_alloc(arena, count * size);
}

void *arena_alloc_array(Arena *arena, size_t count, size_t elem_size) {
    // 检查溢出
    if (count > SIZE_MAX / elem_size) {
        fprintf(stderr, "FATAL: Array allocation overflow\n");
        abort();
    }
    return arena_alloc_aligned(arena, count * elem_size, _Alignof(max_align_t));
}

void *arena_realloc(Arena *arena, void *ptr, size_t old_size, size_t new_size) {
    if (!ptr) return arena_alloc(arena, new_size);
    if (new_size == 0) return NULL;
    if (old_size >= new_size) return ptr;  // 缩小，无需操作
    
    // 检查是否是最新分配
    ArenaBlock *block = arena->current_block;
    char *block_end = block->current;
    char *old_end = (char *)ptr + old_size;
    
    if (old_end == block_end) {
        // 是最后分配，直接扩展
        if ((char *)ptr + new_size <= block->end) {
            block->current = (char *)ptr + new_size;
            arena->total_used += (new_size - old_size);
            memset((char *)ptr + old_size, 0, new_size - old_size);
            return ptr;
        }
    }
    
    // Fallback：新分配+拷贝
    void *new_ptr = arena_alloc(arena, new_size);
    memcpy(new_ptr, ptr, old_size);
    return new_ptr;
}

char *arena_strdup(Arena *arena, const char *str) {
    if (!str) return NULL;
    size_t len = strlen(str);
    char *copy = (char *)arena_alloc(arena, len + 1);
    memcpy(copy, str, len + 1);
    return copy;
}

char *arena_strndup(Arena *arena, const char *str, size_t n) {
    if (!str) return NULL;
    size_t len = strlen(str);
    if (len > n) len = n;
    char *copy = (char *)arena_alloc(arena, len + 1);
    memcpy(copy, str, len);
    copy[len] = '\0';
    return copy;
}

void arena_reset(Arena *arena) {
    if (!arena) return;
    
    // 释放除第一个块外的所有块
    ArenaBlock *block = arena->head->next;
    while (block) {
        ArenaBlock *next = block->next;
        free(block);
        block = next;
    }
    
    // 重置第一个块
    arena->head->next = NULL;
    arena->head->current = arena->head->data;
    arena->current_block = arena->head;
    arena->total_used = 0;
    
    // 重新计算总分配量
    size_t total = arena->block_size;
    arena->total_allocated = total;
}

size_t arena_total_allocated(const Arena *arena) {
    return arena ? arena->total_allocated : 0;
}

size_t arena_total_used(const Arena *arena) {
    return arena ? arena->total_used : 0;
}

size_t arena_block_count(const Arena *arena) {
    if (!arena) return 0;
    size_t count = 0;
    ArenaBlock *block = arena->head;
    while (block) {
        count++;
        block = block->next;
    }
    return count;
}

ArenaMarker arena_mark(Arena *arena) {
    ArenaMarker marker;
    if (arena && arena->current_block) {
        marker.block = arena->current_block;
        marker.position = arena->current_block->current;
    } else {
        marker.block = NULL;
        marker.position = NULL;
    }
    return marker;
}

void arena_rollback(Arena *arena, ArenaMarker marker) {
    if (!arena || !marker.block) return;
    
    // 释放标记之后的块
    ArenaBlock *block = marker.block->next;
    while (block) {
        ArenaBlock *next = block->next;
        arena->total_allocated -= (block->end - block->data);
        free(block);
        block = next;
    }
    
    // 回滚当前块
    marker.block->next = NULL;
    size_t rollback_size = (marker.block->current - marker.position);
    marker.block->current = marker.position;
    arena->current_block = marker.block;
    arena->total_used -= rollback_size;
}