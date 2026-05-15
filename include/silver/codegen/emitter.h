#ifndef SILVER_CODEGEN_EMITTER_H
#define SILVER_CODEGEN_EMITTER_H

#include "silver/codegen/machine.h"
#include "silver/support/buffer.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    SilverBuffer *primary;
    SilverBuffer *secondary;
    SilverBuffer *active;
    
    uint32_t total_emitted;
    uint32_t num_flushes;
    
    uint8_t *fast_ptr;
    uint8_t *fast_end;
    uint32_t fast_remaining;
    
    void (*flush_callback)(const uint8_t *data, uint32_t length, void *user);
    void *user_data;
} CodeEmitter;

CodeEmitter *emitter_create(void);
void emitter_destroy(CodeEmitter *emitter);

bool emitter_emit_u8(CodeEmitter *emitter, uint8_t byte);
bool emitter_emit_bytes(CodeEmitter *emitter, const uint8_t *bytes, uint32_t length);
bool emitter_emit_inst(CodeEmitter *emitter, const MachineInstExt *inst);
bool emitter_align(CodeEmitter *emitter, uint32_t alignment, uint8_t fill);
bool emitter_flush(CodeEmitter *emitter);
uint32_t emitter_offset(const CodeEmitter *emitter);

void emitter_set_flush_callback(CodeEmitter *emitter,
                                 void (*callback)(const uint8_t *data,
                                                 uint32_t length, void *user),
                                 void *user_data);

// ============================================================
// 快速路径 —— 所有平台统一使用
// ============================================================

// 确保有 needed 字节可用，返回写入起始位置
static inline uint8_t *emitter_reserve_fast(CodeEmitter *emitter, uint32_t needed) {
    if (emitter->fast_remaining < needed) {
        emitter_flush(emitter);
    }
    uint8_t *ptr = emitter->fast_ptr;
    emitter->fast_ptr += needed;
    emitter->fast_remaining -= needed;
    emitter->total_emitted += needed;
    return ptr;
}

// 同步快速指针到 SilverBuffer
static inline void emitter_sync_fast(CodeEmitter *emitter) {
    emitter->active->length = (uint32_t)(emitter->fast_ptr - emitter->active->data);
}

// 内联快速写入（用于已知空间足够时，比如编码器内部）
static inline void fast_emit_u8(uint8_t **ptr, uint8_t byte) {
    *(*ptr)++ = byte;
}

static inline void fast_emit_u16_le(uint8_t **ptr, uint16_t val) {
    (*ptr)[0] = (uint8_t)(val & 0xFF);
    (*ptr)[1] = (uint8_t)((val >> 8) & 0xFF);
    *ptr += 2;
}

static inline void fast_emit_u32_le(uint8_t **ptr, uint32_t val) {
    (*ptr)[0] = (uint8_t)(val & 0xFF);
    (*ptr)[1] = (uint8_t)((val >> 8) & 0xFF);
    (*ptr)[2] = (uint8_t)((val >> 16) & 0xFF);
    (*ptr)[3] = (uint8_t)((val >> 24) & 0xFF);
    *ptr += 4;
}

static inline void fast_emit_u64_le(uint8_t **ptr, uint64_t val) {
    fast_emit_u32_le(ptr, (uint32_t)(val & 0xFFFFFFFF));
    fast_emit_u32_le(ptr, (uint32_t)(val >> 32));
}

static inline void fast_emit_bytes(uint8_t **ptr, const uint8_t *data, uint32_t len) {
    memcpy(*ptr, data, len);
    *ptr += len;
}

#ifdef __cplusplus
}
#endif

#endif // SILVER_CODEGEN_EMITTER_H