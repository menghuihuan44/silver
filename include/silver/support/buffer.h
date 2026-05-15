#ifndef SILVER_BUFFER_H
#define SILVER_BUFFER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 可增长的字节缓冲区 - 用于代码发射等场景

typedef struct {
    uint8_t *data;       // 数据指针
    size_t size;         // 缓冲区总大小
    size_t length;       // 当前使用的长度
    bool owns_data;      // 是否拥有数据（需要释放）
    size_t grow_size;    // 增长步长
} SilverBuffer;

// 创建新缓冲区
SilverBuffer *silver_buffer_create(size_t initial_size);

// 从已有数据创建缓冲区（不拥有数据）
SilverBuffer *silver_buffer_create_view(uint8_t *data, size_t size);

// 从静态数据创建缓冲区（不拥有数据，不可增长）
SilverBuffer *silver_buffer_create_static(const uint8_t *data, size_t size);

// 销毁缓冲区
void silver_buffer_destroy(SilverBuffer *buffer);

// 确保缓冲区容量
bool silver_buffer_reserve(SilverBuffer *buffer, size_t capacity);

// 写入数据
bool silver_buffer_write(SilverBuffer *buffer, const void *data, size_t size);

// 写入单个字节
bool silver_buffer_write_u8(SilverBuffer *buffer, uint8_t value);

// 写入uint16 (小端)
bool silver_buffer_write_u16_le(SilverBuffer *buffer, uint16_t value);

// 写入uint32 (小端)
bool silver_buffer_write_u32_le(SilverBuffer *buffer, uint32_t value);

// 写入uint64 (小端)
bool silver_buffer_write_u64_le(SilverBuffer *buffer, uint64_t value);

// 写入uint16 (大端)
bool silver_buffer_write_u16_be(SilverBuffer *buffer, uint16_t value);

// 写入uint32 (大端)
bool silver_buffer_write_u32_be(SilverBuffer *buffer, uint32_t value);

// 写入uint64 (大端)
bool silver_buffer_write_u64_be(SilverBuffer *buffer, uint64_t value);

// 在指定位置写入（覆盖）
bool silver_buffer_write_at(SilverBuffer *buffer, size_t offset, 
                           const void *data, size_t size);

// 在指定位置写入uint32 (小端) - 常用于修复跳转偏移
bool silver_buffer_write_u32_le_at(SilverBuffer *buffer, size_t offset, uint32_t value);

// 预留空间（增加length但不写入）
uint8_t *silver_buffer_allocate(SilverBuffer *buffer, size_t size);

// 重置缓冲区
void silver_buffer_reset(SilverBuffer *buffer);

// 获取当前写入位置
size_t silver_buffer_tell(const SilverBuffer *buffer);

// 设置写入位置
bool silver_buffer_seek(SilverBuffer *buffer, size_t offset);

// 对齐到指定边界
bool silver_buffer_align(SilverBuffer *buffer, size_t alignment, uint8_t fill);

// 获取缓冲区数据指针
static inline uint8_t *silver_buffer_data(const SilverBuffer *buffer) {
    return buffer ? buffer->data : NULL;
}

// 获取缓冲区大小
static inline size_t silver_buffer_size(const SilverBuffer *buffer) {
    return buffer ? buffer->size : 0;
}

// 获取已使用长度
static inline size_t silver_buffer_length(const SilverBuffer *buffer) {
    return buffer ? buffer->length : 0;
}

// 获取剩余可用空间
static inline size_t silver_buffer_remaining(const SilverBuffer *buffer) {
    return buffer ? buffer->size - buffer->length : 0;
}

// 克隆缓冲区
SilverBuffer *silver_buffer_clone(const SilverBuffer *src);

// 合并两个缓冲区
bool silver_buffer_append(SilverBuffer *dest, const SilverBuffer *src);

// 比较两个缓冲区
int silver_buffer_compare(const SilverBuffer *a, const SilverBuffer *b);

// 计算校验和（简单）
uint32_t silver_buffer_checksum(const SilverBuffer *buffer);

#ifdef __cplusplus
}
#endif

#endif // SILVER_BUFFER_H