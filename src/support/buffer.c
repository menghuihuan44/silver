#include "silver/support/buffer.h"
#include <stdlib.h>
#include <string.h>

#define SILVER_BUFFER_DEFAULT_SIZE 4096
#define SILVER_BUFFER_GROW_SIZE 4096

SilverBuffer *silver_buffer_create(size_t initial_size) {
    if (initial_size == 0) {
        initial_size = SILVER_BUFFER_DEFAULT_SIZE;
    }
    
    SilverBuffer *buffer = (SilverBuffer *)calloc(1, sizeof(SilverBuffer));
    if (!buffer) return NULL;
    
    buffer->data = (uint8_t *)malloc(initial_size);
    if (!buffer->data) {
        free(buffer);
        return NULL;
    }
    
    buffer->size = initial_size;
    buffer->length = 0;
    buffer->owns_data = true;
    buffer->grow_size = SILVER_BUFFER_GROW_SIZE;
    
    return buffer;
}

SilverBuffer *silver_buffer_create_view(uint8_t *data, size_t size) {
    if (!data) return NULL;
    
    SilverBuffer *buffer = (SilverBuffer *)calloc(1, sizeof(SilverBuffer));
    if (!buffer) return NULL;
    
    buffer->data = data;
    buffer->size = size;
    buffer->length = 0;
    buffer->owns_data = false;
    buffer->grow_size = 0;  // 视图不可增长
    
    return buffer;
}

SilverBuffer *silver_buffer_create_static(const uint8_t *data, size_t size) {
    if (!data) return NULL;
    
    SilverBuffer *buffer = (SilverBuffer *)calloc(1, sizeof(SilverBuffer));
    if (!buffer) return NULL;
    
    buffer->data = (uint8_t *)data;
    buffer->size = size;
    buffer->length = size;  // 静态数据视为已满
    buffer->owns_data = false;
    buffer->grow_size = 0;  // 静态不可增长
    
    return buffer;
}

void silver_buffer_destroy(SilverBuffer *buffer) {
    if (!buffer) return;
    
    if (buffer->owns_data && buffer->data) {
        free(buffer->data);
    }
    free(buffer);
}

bool silver_buffer_reserve(SilverBuffer *buffer, size_t capacity) {
    if (!buffer) return false;
    
    // 视图和静态缓冲区不可增长
    if (!buffer->owns_data || buffer->grow_size == 0) {
        return capacity <= buffer->size;
    }
    
    if (capacity <= buffer->size) {
        return true;
    }
    
    // 计算新大小（对齐到增长步长）
    size_t new_size = buffer->size;
    while (new_size < capacity) {
        new_size += buffer->grow_size;
    }
    
    uint8_t *new_data = (uint8_t *)realloc(buffer->data, new_size);
    if (!new_data) {
        return false;
    }
    
    buffer->data = new_data;
    buffer->size = new_size;
    return true;
}

bool silver_buffer_write(SilverBuffer *buffer, const void *data, size_t size) {
    if (!buffer || !data || size == 0) return false;
    
    if (!silver_buffer_reserve(buffer, buffer->length + size)) {
        return false;
    }
    
    memcpy(buffer->data + buffer->length, data, size);
    buffer->length += size;
    return true;
}

bool silver_buffer_write_u8(SilverBuffer *buffer, uint8_t value) {
    return silver_buffer_write(buffer, &value, 1);
}

bool silver_buffer_write_u16_le(SilverBuffer *buffer, uint16_t value) {
    uint8_t data[2];
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    return silver_buffer_write(buffer, data, 2);
}

bool silver_buffer_write_u32_le(SilverBuffer *buffer, uint32_t value) {
    uint8_t data[4];
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
    return silver_buffer_write(buffer, data, 4);
}

bool silver_buffer_write_u64_le(SilverBuffer *buffer, uint64_t value) {
    uint8_t data[8];
    data[0] = value & 0xFF;
    data[1] = (value >> 8) & 0xFF;
    data[2] = (value >> 16) & 0xFF;
    data[3] = (value >> 24) & 0xFF;
    data[4] = (value >> 32) & 0xFF;
    data[5] = (value >> 40) & 0xFF;
    data[6] = (value >> 48) & 0xFF;
    data[7] = (value >> 56) & 0xFF;
    return silver_buffer_write(buffer, data, 8);
}

bool silver_buffer_write_u16_be(SilverBuffer *buffer, uint16_t value) {
    uint8_t data[2];
    data[0] = (value >> 8) & 0xFF;
    data[1] = value & 0xFF;
    return silver_buffer_write(buffer, data, 2);
}

bool silver_buffer_write_u32_be(SilverBuffer *buffer, uint32_t value) {
    uint8_t data[4];
    data[0] = (value >> 24) & 0xFF;
    data[1] = (value >> 16) & 0xFF;
    data[2] = (value >> 8) & 0xFF;
    data[3] = value & 0xFF;
    return silver_buffer_write(buffer, data, 4);
}

bool silver_buffer_write_u64_be(SilverBuffer *buffer, uint64_t value) {
    uint8_t data[8];
    data[0] = (value >> 56) & 0xFF;
    data[1] = (value >> 48) & 0xFF;
    data[2] = (value >> 40) & 0xFF;
    data[3] = (value >> 32) & 0xFF;
    data[4] = (value >> 24) & 0xFF;
    data[5] = (value >> 16) & 0xFF;
    data[6] = (value >> 8) & 0xFF;
    data[7] = value & 0xFF;
    return silver_buffer_write(buffer, data, 8);
}

bool silver_buffer_write_at(SilverBuffer *buffer, size_t offset,
                           const void *data, size_t size) {
    if (!buffer || !data || offset + size > buffer->length) {
        return false;
    }
    
    memcpy(buffer->data + offset, data, size);
    return true;
}

bool silver_buffer_write_u32_le_at(SilverBuffer *buffer, size_t offset, uint32_t value) {
    if (!buffer || offset + 4 > buffer->length) {
        return false;
    }
    
    buffer->data[offset] = value & 0xFF;
    buffer->data[offset + 1] = (value >> 8) & 0xFF;
    buffer->data[offset + 2] = (value >> 16) & 0xFF;
    buffer->data[offset + 3] = (value >> 24) & 0xFF;
    return true;
}

uint8_t *silver_buffer_allocate(SilverBuffer *buffer, size_t size) {
    if (!buffer || size == 0) return NULL;
    
    if (!silver_buffer_reserve(buffer, buffer->length + size)) {
        return NULL;
    }
    
    uint8_t *ptr = buffer->data + buffer->length;
    buffer->length += size;
    return ptr;
}

void silver_buffer_reset(SilverBuffer *buffer) {
    if (buffer) {
        buffer->length = 0;
    }
}

size_t silver_buffer_tell(const SilverBuffer *buffer) {
    return buffer ? buffer->length : 0;
}

bool silver_buffer_seek(SilverBuffer *buffer, size_t offset) {
    if (!buffer || offset > buffer->size) {
        return false;
    }
    buffer->length = offset;
    return true;
}

bool silver_buffer_align(SilverBuffer *buffer, size_t alignment, uint8_t fill) {
    if (!buffer || alignment == 0) return false;
    
    size_t misalignment = buffer->length % alignment;
    if (misalignment == 0) return true;
    
    size_t padding = alignment - misalignment;
    if (!silver_buffer_reserve(buffer, buffer->length + padding)) {
        return false;
    }
    
    memset(buffer->data + buffer->length, fill, padding);
    buffer->length += padding;
    return true;
}

SilverBuffer *silver_buffer_clone(const SilverBuffer *src) {
    if (!src) return NULL;
    
    SilverBuffer *clone = silver_buffer_create(src->size);
    if (!clone) return NULL;
    
    memcpy(clone->data, src->data, src->length);
    clone->length = src->length;
    return clone;
}

bool silver_buffer_append(SilverBuffer *dest, const SilverBuffer *src) {
    if (!dest || !src) return false;
    return silver_buffer_write(dest, src->data, src->length);
}

int silver_buffer_compare(const SilverBuffer *a, const SilverBuffer *b) {
    if (a == b) return 0;
    if (!a) return -1;
    if (!b) return 1;
    
    size_t min_len = a->length < b->length ? a->length : b->length;
    int cmp = memcmp(a->data, b->data, min_len);
    
    if (cmp != 0) return cmp;
    if (a->length < b->length) return -1;
    if (a->length > b->length) return 1;
    return 0;
}

uint32_t silver_buffer_checksum(const SilverBuffer *buffer) {
    if (!buffer || !buffer->data) return 0;
    
    uint32_t checksum = 0;
    for (size_t i = 0; i < buffer->length; i++) {
        checksum = checksum * 31 + buffer->data[i];
    }
    return checksum;
}