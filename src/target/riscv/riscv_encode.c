#include "silver/target/riscv/riscv_encode.h"
#include <string.h>

void riscv_encoder_init(RISCVEncoder *enc, uint8_t *buffer, uint32_t capacity) {
    if (!enc) return;
    enc->buffer = buffer;
    enc->capacity = capacity;
    enc->length = 0;
}

bool riscv_emit_word(RISCVEncoder *enc, uint32_t word) {
    if (!enc || enc->length + 4 > enc->capacity) return false;
    
    // RISC-V是小端序
    enc->buffer[enc->length + 0] = (word >> 0)  & 0xFF;
    enc->buffer[enc->length + 1] = (word >> 8)  & 0xFF;
    enc->buffer[enc->length + 2] = (word >> 16) & 0xFF;
    enc->buffer[enc->length + 3] = (word >> 24) & 0xFF;
    enc->length += 4;
    
    return true;
}

// 编码imm12字段（12位有符号立即数）
static bool encode_imm12(int32_t imm, uint32_t *encoded) {
    if (imm < -2048 || imm > 2047) return false;
    *encoded = (uint32_t)(imm & 0xFFF);
    return true;
}

// R-type指令编码
bool riscv_encode_rtype(RISCVEncoder *enc, uint8_t opcode, uint8_t rd,
                        uint8_t funct3, uint8_t rs1, uint8_t rs2, uint8_t funct7) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)(opcode & 0x7F);
    encoding |= (uint32_t)(rd & 0x1F) << 7;
    encoding |= (uint32_t)(funct3 & 0x7) << 12;
    encoding |= (uint32_t)(rs1 & 0x1F) << 15;
    encoding |= (uint32_t)(rs2 & 0x1F) << 20;
    encoding |= (uint32_t)(funct7 & 0x7F) << 25;
    
    return riscv_emit_word(enc, encoding);
}

// 算术指令
bool riscv_encode_add(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return riscv_encode_rtype(enc, RISCV_OP_ALU, rd, RISCV_F3_ADD_SUB, 
                               rs1, rs2, RISCV_F7_ADD);
}

bool riscv_encode_sub(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return riscv_encode_rtype(enc, RISCV_OP_ALU, rd, RISCV_F3_ADD_SUB, 
                               rs1, rs2, RISCV_F7_SUB);
}

bool riscv_encode_sll(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return riscv_encode_rtype(enc, RISCV_OP_ALU, rd, RISCV_F3_SLL, 
                               rs1, rs2, 0);
}

bool riscv_encode_slt(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return riscv_encode_rtype(enc, RISCV_OP_ALU, rd, RISCV_F3_SLT, 
                               rs1, rs2, 0);
}

bool riscv_encode_sltu(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return riscv_encode_rtype(enc, RISCV_OP_ALU, rd, RISCV_F3_SLTU, 
                               rs1, rs2, 0);
}

bool riscv_encode_xor(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return riscv_encode_rtype(enc, RISCV_OP_ALU, rd, RISCV_F3_XOR, 
                               rs1, rs2, 0);
}

bool riscv_encode_srl(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return riscv_encode_rtype(enc, RISCV_OP_ALU, rd, RISCV_F3_SRL_SRA, 
                               rs1, rs2, RISCV_F7_SRL);
}

bool riscv_encode_sra(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return riscv_encode_rtype(enc, RISCV_OP_ALU, rd, RISCV_F3_SRL_SRA, 
                               rs1, rs2, RISCV_F7_SRA);
}

bool riscv_encode_or(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return riscv_encode_rtype(enc, RISCV_OP_ALU, rd, RISCV_F3_OR, 
                               rs1, rs2, 0);
}

bool riscv_encode_and(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return riscv_encode_rtype(enc, RISCV_OP_ALU, rd, RISCV_F3_AND, 
                               rs1, rs2, 0);
}

// 乘除法指令 (RV64M)
bool riscv_encode_mul(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return riscv_encode_rtype(enc, RISCV_OP_ALU, rd, RISCV_F3_MUL, 
                               rs1, rs2, RISCV_F7_MUL);
}

bool riscv_encode_div(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return riscv_encode_rtype(enc, RISCV_OP_ALU, rd, RISCV_F3_DIV, 
                               rs1, rs2, RISCV_F7_DIV);
}

bool riscv_encode_divu(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return riscv_encode_rtype(enc, RISCV_OP_ALU, rd, RISCV_F3_DIVU, 
                               rs1, rs2, RISCV_F7_DIVU);
}

bool riscv_encode_rem(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return riscv_encode_rtype(enc, RISCV_OP_ALU, rd, RISCV_F3_REM, 
                               rs1, rs2, RISCV_F7_REM);
}

bool riscv_encode_remu(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    return riscv_encode_rtype(enc, RISCV_OP_ALU, rd, RISCV_F3_REMU, 
                               rs1, rs2, RISCV_F7_REMU);
}

// I-type指令编码
bool riscv_encode_itype(RISCVEncoder *enc, uint8_t opcode, uint8_t rd,
                        uint8_t funct3, uint8_t rs1, int32_t imm) {
    if (!enc) return false;
    
    uint32_t imm_encoded;
    if (!encode_imm12(imm, &imm_encoded)) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)(opcode & 0x7F);
    encoding |= (uint32_t)(rd & 0x1F) << 7;
    encoding |= (uint32_t)(funct3 & 0x7) << 12;
    encoding |= (uint32_t)(rs1 & 0x1F) << 15;
    encoding |= imm_encoded << 20;
    
    return riscv_emit_word(enc, encoding);
}

bool riscv_encode_addi(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t imm) {
    return riscv_encode_itype(enc, RISCV_OP_ALU_IMM, rd, RISCV_F3_ADD_SUB, rs1, imm);
}

bool riscv_encode_slti(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t imm) {
    return riscv_encode_itype(enc, RISCV_OP_ALU_IMM, rd, RISCV_F3_SLT, rs1, imm);
}

bool riscv_encode_sltiu(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t imm) {
    return riscv_encode_itype(enc, RISCV_OP_ALU_IMM, rd, RISCV_F3_SLTU, rs1, imm);
}

bool riscv_encode_xori(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t imm) {
    return riscv_encode_itype(enc, RISCV_OP_ALU_IMM, rd, RISCV_F3_XOR, rs1, imm);
}

bool riscv_encode_ori(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t imm) {
    return riscv_encode_itype(enc, RISCV_OP_ALU_IMM, rd, RISCV_F3_OR, rs1, imm);
}

bool riscv_encode_andi(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t imm) {
    return riscv_encode_itype(enc, RISCV_OP_ALU_IMM, rd, RISCV_F3_AND, rs1, imm);
}

// 移位立即数指令（特殊编码，imm低6位为移位量，高6位为funct6）
bool riscv_encode_slli(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t shamt) {
    if (!enc || shamt > 63) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)RISCV_OP_ALU_IMM;
    encoding |= (uint32_t)(rd & 0x1F) << 7;
    encoding |= (uint32_t)RISCV_F3_SLL << 12;
    encoding |= (uint32_t)(rs1 & 0x1F) << 15;
    encoding |= (uint32_t)(shamt & 0x3F) << 20;  // 仅低6位用于移位量
    // RISC-V 64: SLLI的imm[11:6] = 000000
    
    return riscv_emit_word(enc, encoding);
}

bool riscv_encode_srli(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t shamt) {
    if (!enc || shamt > 63) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)RISCV_OP_ALU_IMM;
    encoding |= (uint32_t)(rd & 0x1F) << 7;
    encoding |= (uint32_t)RISCV_F3_SRL_SRA << 12;
    encoding |= (uint32_t)(rs1 & 0x1F) << 15;
    encoding |= (uint32_t)(shamt & 0x3F) << 20;
    // imm[11:6] = 000000 for SRLI
    
    return riscv_emit_word(enc, encoding);
}

bool riscv_encode_srai(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t shamt) {
    if (!enc || shamt > 63) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)RISCV_OP_ALU_IMM;
    encoding |= (uint32_t)(rd & 0x1F) << 7;
    encoding |= (uint32_t)RISCV_F3_SRL_SRA << 12;
    encoding |= (uint32_t)(rs1 & 0x1F) << 15;
    encoding |= (uint32_t)(shamt & 0x3F) << 20;
    encoding |= 0x10U << 26;  // imm[11:6] = 010000 for SRAI
    
    return riscv_emit_word(enc, encoding);
}

// 加载指令
bool riscv_encode_ld(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset) {
    return riscv_encode_itype(enc, RISCV_OP_LOAD, rd, RISCV_F3_DOUBLE, rs1, offset);
}

bool riscv_encode_lw(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset) {
    return riscv_encode_itype(enc, RISCV_OP_LOAD, rd, RISCV_F3_WORD, rs1, offset);
}

bool riscv_encode_lh(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset) {
    return riscv_encode_itype(enc, RISCV_OP_LOAD, rd, RISCV_F3_HALF, rs1, offset);
}

bool riscv_encode_lb(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset) {
    return riscv_encode_itype(enc, RISCV_OP_LOAD, rd, RISCV_F3_BYTE, rs1, offset);
}

bool riscv_encode_lhu(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset) {
    return riscv_encode_itype(enc, RISCV_OP_LOAD, rd, RISCV_F3_HALFU, rs1, offset);
}

bool riscv_encode_lbu(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset) {
    return riscv_encode_itype(enc, RISCV_OP_LOAD, rd, RISCV_F3_BYTEU, rs1, offset);
}

// 存储指令 (S-type)
static bool riscv_encode_stype(RISCVEncoder *enc, uint8_t opcode, uint8_t funct3,
                                uint8_t rs2, uint8_t rs1, int32_t offset) {
    if (!enc) return false;
    if (offset < -2048 || offset > 2047) return false;
    
    uint32_t imm12 = (uint32_t)(offset & 0xFFF);
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)(opcode & 0x7F);
    encoding |= (uint32_t)(imm12 & 0x1F) << 7;        // imm[4:0]
    encoding |= (uint32_t)(funct3 & 0x7) << 12;
    encoding |= (uint32_t)(rs1 & 0x1F) << 15;
    encoding |= (uint32_t)(rs2 & 0x1F) << 20;
    encoding |= (uint32_t)((imm12 >> 5) & 0x7F) << 25; // imm[11:5]
    
    return riscv_emit_word(enc, encoding);
}

bool riscv_encode_sb(RISCVEncoder *enc, uint8_t rs2, uint8_t rs1, int32_t offset) {
    return riscv_encode_stype(enc, RISCV_OP_STORE, RISCV_F3_BYTE, rs2, rs1, offset);
}

bool riscv_encode_sh(RISCVEncoder *enc, uint8_t rs2, uint8_t rs1, int32_t offset) {
    return riscv_encode_stype(enc, RISCV_OP_STORE, RISCV_F3_HALF, rs2, rs1, offset);
}

bool riscv_encode_sw(RISCVEncoder *enc, uint8_t rs2, uint8_t rs1, int32_t offset) {
    return riscv_encode_stype(enc, RISCV_OP_STORE, RISCV_F3_WORD, rs2, rs1, offset);
}

bool riscv_encode_sd(RISCVEncoder *enc, uint8_t rs2, uint8_t rs1, int32_t offset) {
    return riscv_encode_stype(enc, RISCV_OP_STORE, RISCV_F3_DOUBLE, rs2, rs1, offset);
}

// 分支指令 (B-type)
bool riscv_encode_btype(RISCVEncoder *enc, uint8_t opcode, uint8_t funct3,
                        uint8_t rs1, uint8_t rs2, int32_t offset) {
    if (!enc) return false;
    if (offset < -4096 || offset > 4095) return false;
    if (offset & 1) return false;  // 偏移必须是2字节对齐
    
    uint32_t imm12 = (uint32_t)(offset & 0x1FFE);  // 13位，但仅12位存储
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)(opcode & 0x7F);
    encoding |= (uint32_t)((imm12 >> 11) & 1) << 7;    // imm[11]
    encoding |= (uint32_t)((imm12 >> 1) & 0xF) << 8;   // imm[4:1]
    encoding |= (uint32_t)(funct3 & 0x7) << 12;
    encoding |= (uint32_t)(rs1 & 0x1F) << 15;
    encoding |= (uint32_t)(rs2 & 0x1F) << 20;
    encoding |= (uint32_t)((imm12 >> 5) & 0x3F) << 25;  // imm[10:5]
    encoding |= (uint32_t)((imm12 >> 12) & 1) << 31;    // imm[12]
    
    return riscv_emit_word(enc, encoding);
}

bool riscv_encode_beq(RISCVEncoder *enc, uint8_t rs1, uint8_t rs2, int32_t offset) {
    return riscv_encode_btype(enc, RISCV_OP_BRANCH, RISCV_F3_BEQ, rs1, rs2, offset);
}

bool riscv_encode_bne(RISCVEncoder *enc, uint8_t rs1, uint8_t rs2, int32_t offset) {
    return riscv_encode_btype(enc, RISCV_OP_BRANCH, RISCV_F3_BNE, rs1, rs2, offset);
}

bool riscv_encode_blt(RISCVEncoder *enc, uint8_t rs1, uint8_t rs2, int32_t offset) {
    return riscv_encode_btype(enc, RISCV_OP_BRANCH, RISCV_F3_BLT, rs1, rs2, offset);
}

bool riscv_encode_bge(RISCVEncoder *enc, uint8_t rs1, uint8_t rs2, int32_t offset) {
    return riscv_encode_btype(enc, RISCV_OP_BRANCH, RISCV_F3_BGE, rs1, rs2, offset);
}

bool riscv_encode_bltu(RISCVEncoder *enc, uint8_t rs1, uint8_t rs2, int32_t offset) {
    return riscv_encode_btype(enc, RISCV_OP_BRANCH, RISCV_F3_BLTU, rs1, rs2, offset);
}

bool riscv_encode_bgeu(RISCVEncoder *enc, uint8_t rs1, uint8_t rs2, int32_t offset) {
    return riscv_encode_btype(enc, RISCV_OP_BRANCH, RISCV_F3_BGEU, rs1, rs2, offset);
}

// JALR指令
bool riscv_encode_jalr(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset) {
    return riscv_encode_itype(enc, RISCV_OP_JALR, rd, 0, rs1, offset);
}

// U-type指令
bool riscv_encode_lui(RISCVEncoder *enc, uint8_t rd, int32_t imm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)RISCV_OP_LUI;
    encoding |= (uint32_t)(rd & 0x1F) << 7;
    encoding |= (uint32_t)(imm & 0xFFFFF) << 12;
    
    return riscv_emit_word(enc, encoding);
}

bool riscv_encode_auipc(RISCVEncoder *enc, uint8_t rd, int32_t imm) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)RISCV_OP_AUIPC;
    encoding |= (uint32_t)(rd & 0x1F) << 7;
    encoding |= (uint32_t)(imm & 0xFFFFF) << 12;
    
    return riscv_emit_word(enc, encoding);
}

// J-type指令
bool riscv_encode_jal(RISCVEncoder *enc, uint8_t rd, int32_t offset) {
    if (!enc) return false;
    if (offset < -1048576 || offset > 1048575) return false;
    if (offset & 1) return false;
    
    uint32_t imm21 = (uint32_t)(offset & 0x1FFFFF);
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)RISCV_OP_JAL;
    encoding |= (uint32_t)(rd & 0x1F) << 7;
    encoding |= (uint32_t)((imm21 >> 12) & 0xFF) << 12;   // imm[19:12]
    encoding |= (uint32_t)((imm21 >> 11) & 1) << 20;      // imm[11]
    encoding |= (uint32_t)((imm21 >> 1) & 0x3FF) << 21;   // imm[10:1]
    encoding |= (uint32_t)((imm21 >> 20) & 1) << 31;      // imm[20]
    
    return riscv_emit_word(enc, encoding);
}

// 伪指令
bool riscv_encode_nop(RISCVEncoder *enc) {
    return riscv_encode_addi(enc, 0, 0, 0);  // addi x0, x0, 0
}

bool riscv_encode_mv(RISCVEncoder *enc, uint8_t rd, uint8_t rs) {
    return riscv_encode_addi(enc, rd, rs, 0);  // addi rd, rs, 0
}

bool riscv_encode_not(RISCVEncoder *enc, uint8_t rd, uint8_t rs) {
    return riscv_encode_xori(enc, rd, rs, -1);  // xori rd, rs, -1
}

bool riscv_encode_neg(RISCVEncoder *enc, uint8_t rd, uint8_t rs) {
    return riscv_encode_sub(enc, rd, 0, rs);  // sub rd, x0, rs
}

// LI (加载立即数) - 使用LUI+ADDI组合
bool riscv_encode_li(RISCVEncoder *enc, uint8_t rd, int64_t imm) {
    if (!enc) return false;
    
    if (imm >= -2048 && imm <= 2047) {
        return riscv_encode_addi(enc, rd, 0, (int32_t)imm);
    }
    
    // 使用LUI加载高20位
    int32_t imm32 = (int32_t)imm;
    int32_t upper = (imm32 + 0x800) >> 12;  // 舍入
    int32_t lower = imm32 - (upper << 12);
    
    if (upper != 0) {
        if (!riscv_encode_lui(enc, rd, upper)) return false;
        if (lower != 0) {
            return riscv_encode_addi(enc, rd, rd, lower);
        }
        return true;
    }
    
    return false;
}

bool riscv_encode_call(RISCVEncoder *enc, int32_t offset) {
    // AUIPC + JALR组合用于长调用
    // 简化：使用JAL如果偏移在范围内
    return riscv_encode_jal(enc, 1, offset);  // jal ra, offset (RA = x1)
}

bool riscv_encode_ret(RISCVEncoder *enc) {
    return riscv_encode_jalr(enc, 0, 1, 0);  // jalr x0, x1, 0
}

bool riscv_encode_j(RISCVEncoder *enc, int32_t offset) {
    return riscv_encode_jal(enc, 0, offset);  // jal x0, offset
}

bool riscv_encode_jr(RISCVEncoder *enc, uint8_t rs1) {
    return riscv_encode_jalr(enc, 0, rs1, 0);  // jalr x0, rs1, 0
}

// RISC-V双精度浮点指令
bool riscv_encode_fadd_d(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    if (!enc) return false;
    
    // FADD.D rd, rs1, rs2
    uint32_t encoding = 0;
    encoding |= (uint32_t)RISCV_OP_FP;
    encoding |= (uint32_t)(rd & 0x1F) << 7;
    encoding |= 0U << 12;  // funct3 = 0 (FADD)
    encoding |= (uint32_t)(rs1 & 0x1F) << 15;
    encoding |= (uint32_t)(rs2 & 0x1F) << 20;
    encoding |= 0x01U << 25;  // funct7 for double
    encoding |= 0x01U << 26;  // fmt = D
    
    return riscv_emit_word(enc, encoding);
}

bool riscv_encode_fsub_d(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)RISCV_OP_FP;
    encoding |= (uint32_t)(rd & 0x1F) << 7;
    encoding |= 0U << 12;  // funct3
    encoding |= (uint32_t)(rs1 & 0x1F) << 15;
    encoding |= (uint32_t)(rs2 & 0x1F) << 20;
    encoding |= 0x05U << 25;  // funct7 for FSUB
    encoding |= 0x01U << 26;  // fmt = D
    
    return riscv_emit_word(enc, encoding);
}

bool riscv_encode_fmul_d(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)RISCV_OP_FP;
    encoding |= (uint32_t)(rd & 0x1F) << 7;
    encoding |= 0U << 12;
    encoding |= (uint32_t)(rs1 & 0x1F) << 15;
    encoding |= (uint32_t)(rs2 & 0x1F) << 20;
    encoding |= 0x09U << 25;  // funct7 for FMUL
    encoding |= 0x01U << 26;  // fmt = D
    
    return riscv_emit_word(enc, encoding);
}

bool riscv_encode_fdiv_d(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, uint8_t rs2) {
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)RISCV_OP_FP;
    encoding |= (uint32_t)(rd & 0x1F) << 7;
    encoding |= 0U << 12;
    encoding |= (uint32_t)(rs1 & 0x1F) << 15;
    encoding |= (uint32_t)(rs2 & 0x1F) << 20;
    encoding |= 0x0DU << 25;  // funct7 for FDIV
    encoding |= 0x01U << 26;  // fmt = D
    
    return riscv_emit_word(enc, encoding);
}

// RISC-V浮点加载/存储（双精度）
bool riscv_encode_fld(RISCVEncoder *enc, uint8_t rd, uint8_t rs1, int32_t offset) {
    if (!enc) return false;
    if (offset < -2048 || offset > 2047) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)RISCV_OP_FP_LOAD;
    encoding |= (uint32_t)(rd & 0x1F) << 7;
    encoding |= 3U << 12;  // funct3 = 3 (double)
    encoding |= (uint32_t)(rs1 & 0x1F) << 15;
    encoding |= (uint32_t)(offset & 0xFFF) << 20;
    
    return riscv_emit_word(enc, encoding);
}

bool riscv_encode_fsd(RISCVEncoder *enc, uint8_t rs2, uint8_t rs1, int32_t offset) {
    if (!enc) return false;
    if (offset < -2048 || offset > 2047) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)RISCV_OP_FP_STORE;
    encoding |= (uint32_t)(offset & 0x1F) << 7;
    encoding |= 3U << 12;  // funct3 = 3
    encoding |= (uint32_t)(rs1 & 0x1F) << 15;
    encoding |= (uint32_t)(rs2 & 0x1F) << 20;
    encoding |= (uint32_t)((offset >> 5) & 0x7F) << 25;
    
    return riscv_emit_word(enc, encoding);
}

// RISC-V浮点转换
bool riscv_encode_fcvt_d_l(RISCVEncoder *enc, uint8_t rd, uint8_t rs1) {
    // 有符号64位整数 -> 双精度浮点
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)RISCV_OP_FP;
    encoding |= (uint32_t)(rd & 0x1F) << 7;
    encoding |= 2U << 12;  // funct3
    encoding |= (uint32_t)(rs1 & 0x1F) << 15;
    encoding |= 0x21U << 25;  // FCVT.D.L
    encoding |= 0x01U << 26;  // fmt = D
    
    return riscv_emit_word(enc, encoding);
}

bool riscv_encode_fcvt_l_d(RISCVEncoder *enc, uint8_t rd, uint8_t rs1) {
    // 双精度浮点 -> 有符号64位整数
    if (!enc) return false;
    
    uint32_t encoding = 0;
    encoding |= (uint32_t)RISCV_OP_FP;
    encoding |= (uint32_t)(rd & 0x1F) << 7;
    encoding |= 2U << 12;
    encoding |= (uint32_t)(rs1 & 0x1F) << 15;
    encoding |= 0x21U << 25;  // FCVT.L.D
    encoding |= 0x01U << 26;
    encoding |= 1U << 20;  // 有符号
    encoding |= 0x03U << 21;  // RTZ
    
    return riscv_emit_word(enc, encoding);
}