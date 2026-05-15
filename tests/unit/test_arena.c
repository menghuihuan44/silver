#include "silver/support/arena.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>

static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) static void test_##name(void)
#define RUN_TEST(name) do { \
    tests_run++; \
    printf("  Running %s... ", #name); \
    test_##name(); \
    tests_passed++; \
    printf("PASSED\n"); \
} while(0)

#define ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAILED at %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        printf("FAILED at %s:%d: %s != %s (%lld != %lld)\n", \
               __FILE__, __LINE__, #a, #b, (long long)(a), (long long)(b)); \
        tests_failed++; \
        return; \
    } \
} while(0)

#define ASSERT_STR_EQ(a, b) do { \
    const char *_a = (a); \
    const char *_b = (b); \
    if (_a == NULL || _b == NULL || strcmp(_a, _b) != 0) { \
        printf("FAILED at %s:%d: %s != %s ('%s' != '%s')\n", \
               __FILE__, __LINE__, #a, #b, \
               _a ? _a : "(null)", _b ? _b : "(null)"); \
        tests_failed++; \
        return; \
    } \
} while(0)

TEST(arena_create_destroy) {
    Arena *arena = arena_create();
    ASSERT(arena != NULL);
    ASSERT(arena->head != NULL);
    ASSERT_EQ(arena->num_blocks, 1);
    
    arena_destroy(arena);
}

TEST(arena_basic_alloc) {
    Arena *arena = arena_create();
    ASSERT(arena != NULL);
    
    void *ptr1 = arena_alloc(arena, 100);
    ASSERT(ptr1 != NULL);
    
    void *ptr2 = arena_alloc(arena, 200);
    ASSERT(ptr2 != NULL);
    ASSERT(ptr2 > ptr1);  // 应该在ptr1之后
    
    // 分配应该在同一块内
    ASSERT_EQ(arena->num_blocks, 1);
    
    arena_destroy(arena);
}

TEST(arena_aligned_alloc) {
    Arena *arena = arena_create();
    ASSERT(arena != NULL);
    
    // 分配一些非对齐的数据
    arena_alloc(arena, 1);
    
    // 分配16字节对齐的数据
    void *ptr = arena_alloc_aligned(arena, 64, 16);
    ASSERT(ptr != NULL);
    ASSERT(((uintptr_t)ptr & 15) == 0);  // 16字节对齐
    
    arena_destroy(arena);
}

TEST(arena_large_alloc) {
    Arena *arena = arena_create();
    ASSERT(arena != NULL);
    
    // 分配超过默认块大小的内存
    void *ptr = arena_alloc(arena, 100000);
    ASSERT(ptr != NULL);
    ASSERT_EQ(arena->num_blocks, 2);  // 应该创建了新块
    
    arena_destroy(arena);
}

TEST(arena_calloc) {
    Arena *arena = arena_create();
    ASSERT(arena != NULL);
    
    int *array = (int *)arena_calloc(arena, 10 * sizeof(int));
    ASSERT(array != NULL);
    
    for (int i = 0; i < 10; i++) {
        ASSERT_EQ(array[i], 0);  // 所有元素应该是0
    }
    
    arena_destroy(arena);
}

TEST(arena_reset) {
    Arena *arena = arena_create();
    ASSERT(arena != NULL);
    
    void *ptr1 = arena_alloc(arena, 100);
    ASSERT(ptr1 != NULL);
    
    arena_reset(arena);
    
    // 重置后，下一个分配应该从开头开始
    void *ptr2 = arena_alloc(arena, 100);
    ASSERT(ptr2 != NULL);
    ASSERT_EQ(ptr1, ptr2);  // 应该是同一个位置
    
    arena_destroy(arena);
}

TEST(arena_marker) {
    Arena *arena = arena_create();
    ASSERT(arena != NULL);
    
    ArenaMarker marker = arena_mark(arena);
    
    void *ptr1 = arena_alloc(arena, 100);
    ASSERT(ptr1 != NULL);
    
    // 回滚
    arena_rollback(arena, marker);
    
    // 回滚后的分配应该和ptr1在同一位置
    void *ptr2 = arena_alloc(arena, 100);
    ASSERT(ptr2 != NULL);
    ASSERT_EQ(ptr1, ptr2);
    
    arena_destroy(arena);
}

TEST(arena_strdup) {
    Arena *arena = arena_create();
    ASSERT(arena != NULL);
    
    const char *original = "Hello, Silver!";
    char *copy = arena_strdup(arena, original);
    ASSERT(copy != NULL);
    ASSERT_STR_EQ(copy, original);
    ASSERT(copy != original);  // 应该是不同的指针
    
    arena_destroy(arena);
}

TEST(arena_sprintf) {
    Arena *arena = arena_create();
    ASSERT(arena != NULL);
    
    char *str = arena_sprintf(arena, "Value: %d, Name: %s", 42, "Silver");
    ASSERT(str != NULL);
    ASSERT_STR_EQ(str, "Value: 42, Name: Silver");
    
    arena_destroy(arena);
}

TEST(arena_stats) {
    Arena *arena = arena_create();
    ASSERT(arena != NULL);
    
    ArenaStats stats = arena_get_stats(arena);
    ASSERT_EQ(stats.num_blocks, 1);
    ASSERT_EQ(stats.total_used, 0);
    
    arena_alloc(arena, 1024);
    stats = arena_get_stats(arena);
    ASSERT(stats.total_used >= 1024);
    
    arena_destroy(arena);
}

int main(void) {
    printf("=== Silver Arena Allocator Unit Tests ===\n\n");
    
    RUN_TEST(arena_create_destroy);
    RUN_TEST(arena_basic_alloc);
    RUN_TEST(arena_aligned_alloc);
    RUN_TEST(arena_large_alloc);
    RUN_TEST(arena_calloc);
    RUN_TEST(arena_reset);
    RUN_TEST(arena_marker);
    RUN_TEST(arena_strdup);
    RUN_TEST(arena_sprintf);
    RUN_TEST(arena_stats);
    
    printf("\n=== Results: %d/%d passed, %d failed ===\n",
           tests_passed, tests_run, tests_failed);
    
    return tests_failed > 0 ? 1 : 0;
}