#include "silver/codegen/emitter.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define EMITTER_BUFFER_SIZE 65536

CodeEmitter *emitter_create(void) {
    CodeEmitter *emitter = (CodeEmitter *)calloc(1, sizeof(CodeEmitter));
    if (!emitter) return NULL;
    
    emitter->primary = silver_buffer_create(EMITTER_BUFFER_SIZE);
    emitter->secondary = silver_buffer_create(EMITTER_BUFFER_SIZE);
    
    if (!emitter->primary || !emitter->secondary) {
        emitter_destroy(emitter);
        return NULL;
    }
    
    emitter->active = emitter->primary;
    emitter->total_emitted = 0;
    emitter->num_flushes = 0;
    
    emitter->fast_ptr = emitter->active->data;
    emitter->fast_end = emitter->active->data + emitter->active->size;
    emitter->fast_remaining = emitter->active->size;
    
    return emitter;
}

void emitter_destroy(CodeEmitter *emitter) {
    if (!emitter) return;
    silver_buffer_destroy(emitter->primary);
    silver_buffer_destroy(emitter->secondary);
    free(emitter);
}

bool emitter_flush(CodeEmitter *emitter) {
    if (!emitter) return false;
    
    emitter_sync_fast(emitter);
    
    SilverBuffer *buffer = emitter->active;
    if (buffer->length == 0) return true;
    
    if (emitter->flush_callback) {
        emitter->flush_callback(buffer->data, buffer->length, emitter->user_data);
    }
    
    if (emitter->active == emitter->primary) {
        emitter->active = emitter->secondary;
    } else {
        emitter->active = emitter->primary;
    }
    
    silver_buffer_reset(emitter->active);
    
    emitter->fast_ptr = emitter->active->data;
    emitter->fast_end = emitter->active->data + emitter->active->size;
    emitter->fast_remaining = emitter->active->size;
    
    emitter->num_flushes++;
    return true;
}

bool emitter_emit_bytes(CodeEmitter *emitter, const uint8_t *bytes, uint32_t length) {
    if (!emitter || !bytes || length == 0) return false;
    if (emitter->fast_remaining < length) {
        if (!emitter_flush(emitter)) return false;
    }
    memcpy(emitter->fast_ptr, bytes, length);
    emitter->fast_ptr += length;
    emitter->fast_remaining -= length;
    emitter->total_emitted += length;
    return true;
}

bool emitter_emit_u8(CodeEmitter *emitter, uint8_t byte) {
    return emitter_emit_bytes(emitter, &byte, 1);
}

bool emitter_emit_inst(CodeEmitter *emitter, const MachineInstExt *inst) {
    if (!emitter || !inst) return false;
    // 委托给上层 codegen_generate_function 中的快速路径
    return true;
}

bool emitter_align(CodeEmitter *emitter, uint32_t alignment, uint8_t fill) {
    if (!emitter || alignment == 0) return false;
    emitter_sync_fast(emitter);
    silver_buffer_align(emitter->active, alignment, fill);
    emitter->fast_ptr = emitter->active->data + emitter->active->length;
    emitter->fast_remaining = emitter->active->size - emitter->active->length;
    return true;
}

uint32_t emitter_offset(const CodeEmitter *emitter) {
    return emitter ? emitter->total_emitted : 0;
}

void emitter_set_flush_callback(CodeEmitter *emitter,
                                 void (*callback)(const uint8_t *data,
                                                 uint32_t length, void *user),
                                 void *user_data) {
    if (!emitter) return;
    emitter->flush_callback = callback;
    emitter->user_data = user_data;
}