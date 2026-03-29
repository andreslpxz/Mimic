#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "../include/decoder.h"

// Helper to decode ModRM/SIB
static int decode_operand(uint8_t *code, int pos, uint8_t rex, DecodedInstruction *inst, Operand *op, uint8_t modrm, uint64_t rip) {
    uint8_t mod = (modrm >> 6) & 0x03;
    uint8_t rm = modrm & 0x07;

    op->type = OP_MEM;
    op->mem.base_reg = -1;
    op->mem.index_reg = -1;
    op->mem.scale = 1;
    op->mem.disp = 0;
    op->mem.rip_rel = 0;

    if (mod == 3) {
        op->type = OP_REG;
        op->reg = rm + ((rex & 0x01) ? 8 : 0);
        return pos;
    }

    if (mod == 0 && rm == 5) {
        // RIP-relative addressing
        op->mem.rip_rel = 1;
        op->mem.disp = *(int32_t *)&code[pos];
        pos += 4;
        return pos;
    }

    if (rm == 4) {
        // SIB byte
        uint8_t sib = code[pos++];
        uint8_t scale = (sib >> 6) & 0x03;
        uint8_t index = (sib >> 3) & 0x07;
        uint8_t base = sib & 0x07;

        op->mem.scale = 1 << scale;

        if (index != 4 || (rex & 0x02)) {
            op->mem.index_reg = index + ((rex & 0x02) ? 8 : 0);
        }

        if (mod == 0 && base == 5) {
            op->mem.disp = *(int32_t *)&code[pos];
            pos += 4;
        } else {
            op->mem.base_reg = base + ((rex & 0x01) ? 8 : 0);
        }
    } else {
        op->mem.base_reg = rm + ((rex & 0x01) ? 8 : 0);
    }

    if (mod == 1) {
        op->mem.disp = (int8_t)code[pos++];
    } else if (mod == 2) {
        op->mem.disp = *(int32_t *)&code[pos];
        pos += 4;
    }

    return pos;
}

DecodedInstruction decode(uint8_t *code, uint64_t rip) {
    DecodedInstruction inst;
    memset(&inst, 0, sizeof(inst));
    inst.type = INST_UNKNOWN;
    inst.op1.type = OP_NONE;
    inst.op2.type = OP_NONE;

    int pos = 0;
    uint8_t rex = 0;

    // Prefixes
    while (code[pos] >= 0x40 && code[pos] <= 0x4F) {
        rex = code[pos++];
    }
    inst.rex = rex;

    uint8_t opcode = code[pos++];

    // 0x89: MOV r/m64, r64
    if (opcode == 0x89) {
        inst.type = INST_MOV;
        uint8_t modrm = code[pos++];
        uint8_t reg = (modrm >> 3) & 0x07;
        if (rex & 0x04) reg += 8;

        pos = decode_operand(code, pos, rex, &inst, &inst.op1, modrm, rip);
        inst.op2.type = OP_REG;
        inst.op2.reg = reg;
    }
    // 0x8B: MOV r64, r/m64
    else if (opcode == 0x8B) {
        inst.type = INST_MOV;
        uint8_t modrm = code[pos++];
        uint8_t reg = (modrm >> 3) & 0x07;
        if (rex & 0x04) reg += 8;

        inst.op1.type = OP_REG;
        inst.op1.reg = reg;
        pos = decode_operand(code, pos, rex, &inst, &inst.op2, modrm, rip);
    }
    // 0xC7: MOV r/m64, imm32
    else if (opcode == 0xC7) {
        uint8_t modrm = code[pos++];
        if (((modrm >> 3) & 0x07) == 0) {
            inst.type = INST_MOV;
            pos = decode_operand(code, pos, rex, &inst, &inst.op1, modrm, rip);
            inst.op2.type = OP_IMM;
            inst.op2.imm = (int32_t)*(int32_t *)&code[pos];
            pos += 4;
        }
    }
    // 0xB8-0xBF: MOV r64, imm64
    else if (opcode >= 0xB8 && opcode <= 0xBF) {
        inst.type = INST_MOV;
        uint8_t reg = opcode - 0xB8;
        if (rex & 0x01) reg += 8;

        inst.op1.type = OP_REG;
        inst.op1.reg = reg;
        inst.op2.type = OP_IMM;
        if (rex & 0x08) {
            inst.op2.imm = *(uint64_t *)&code[pos];
            pos += 8;
        } else {
            inst.op2.imm = *(uint32_t *)&code[pos];
            pos += 4;
        }
    }
    // 0x8D: LEA r64, m
    else if (opcode == 0x8D) {
        inst.type = INST_LEA;
        uint8_t modrm = code[pos++];
        uint8_t reg = (modrm >> 3) & 0x07;
        if (rex & 0x04) reg += 8;

        inst.op1.type = OP_REG;
        inst.op1.reg = reg;
        pos = decode_operand(code, pos, rex, &inst, &inst.op2, modrm, rip);
    }
    // 0x83: ADD/SUB/CMP/AND/OR/XOR r/m64, imm8
    else if (opcode == 0x83) {
        uint8_t modrm = code[pos++];
        uint8_t sub_op = (modrm >> 3) & 0x07;

        switch (sub_op) {
            case 0: inst.type = INST_ADD; break;
            case 1: inst.type = INST_OR; break;
            case 4: inst.type = INST_AND; break;
            case 5: inst.type = INST_SUB; break;
            case 6: inst.type = INST_XOR; break;
            case 7: inst.type = INST_CMP; break;
            default: inst.type = INST_UNKNOWN; break;
        }

        if (inst.type != INST_UNKNOWN) {
            pos = decode_operand(code, pos, rex, &inst, &inst.op1, modrm, rip);
            inst.op2.type = OP_IMM;
            inst.op2.imm = (int8_t)code[pos++];
        }
    }
    // 0x81: ADD/SUB/CMP/AND/OR/XOR r/m64, imm32
    else if (opcode == 0x81) {
        uint8_t modrm = code[pos++];
        uint8_t sub_op = (modrm >> 3) & 0x07;

        switch (sub_op) {
            case 0: inst.type = INST_ADD; break;
            case 1: inst.type = INST_OR; break;
            case 4: inst.type = INST_AND; break;
            case 5: inst.type = INST_SUB; break;
            case 6: inst.type = INST_XOR; break;
            case 7: inst.type = INST_CMP; break;
            default: inst.type = INST_UNKNOWN; break;
        }

        if (inst.type != INST_UNKNOWN) {
            pos = decode_operand(code, pos, rex, &inst, &inst.op1, modrm, rip);
            inst.op2.type = OP_IMM;
            inst.op2.imm = (int32_t)*(int32_t *)&code[pos];
            pos += 4;
        }
    }
    // 0x01: ADD r/m64, r64
    else if (opcode == 0x01) {
        inst.type = INST_ADD;
        uint8_t modrm = code[pos++];
        uint8_t reg = (modrm >> 3) & 0x07;
        if (rex & 0x04) reg += 8;

        pos = decode_operand(code, pos, rex, &inst, &inst.op1, modrm, rip);
        inst.op2.type = OP_REG;
        inst.op2.reg = reg;
    }
    // 0x03: ADD r64, r/m64
    else if (opcode == 0x03) {
        inst.type = INST_ADD;
        uint8_t modrm = code[pos++];
        uint8_t reg = (modrm >> 3) & 0x07;
        if (rex & 0x04) reg += 8;

        inst.op1.type = OP_REG;
        inst.op1.reg = reg;
        pos = decode_operand(code, pos, rex, &inst, &inst.op2, modrm, rip);
    }
    // 0x29: SUB r/m64, r64
    else if (opcode == 0x29) {
        inst.type = INST_SUB;
        uint8_t modrm = code[pos++];
        uint8_t reg = (modrm >> 3) & 0x07;
        if (rex & 0x04) reg += 8;

        pos = decode_operand(code, pos, rex, &inst, &inst.op1, modrm, rip);
        inst.op2.type = OP_REG;
        inst.op2.reg = reg;
    }
    // 0x2B: SUB r64, r/m64
    else if (opcode == 0x2B) {
        inst.type = INST_SUB;
        uint8_t modrm = code[pos++];
        uint8_t reg = (modrm >> 3) & 0x07;
        if (rex & 0x04) reg += 8;

        inst.op1.type = OP_REG;
        inst.op1.reg = reg;
        pos = decode_operand(code, pos, rex, &inst, &inst.op2, modrm, rip);
    }
    // 0x31: XOR r/m32, r32
    else if (opcode == 0x31) {
        inst.type = INST_XOR;
        uint8_t modrm = code[pos++];
        uint8_t reg = (modrm >> 3) & 0x07;
        if (rex & 0x04) reg += 8;

        pos = decode_operand(code, pos, rex, &inst, &inst.op1, modrm, rip);
        inst.op2.type = OP_REG;
        inst.op2.reg = reg;
    }
    // 0x85: TEST r/m64, r64
    else if (opcode == 0x85) {
        inst.type = INST_TEST;
        uint8_t modrm = code[pos++];
        uint8_t reg = (modrm >> 3) & 0x07;
        if (rex & 0x04) reg += 8;

        pos = decode_operand(code, pos, rex, &inst, &inst.op1, modrm, rip);
        inst.op2.type = OP_REG;
        inst.op2.reg = reg;
    }
    // 0x39: CMP r/m64, r64
    else if (opcode == 0x39) {
        inst.type = INST_CMP;
        uint8_t modrm = code[pos++];
        uint8_t reg = (modrm >> 3) & 0x07;
        if (rex & 0x04) reg += 8;

        pos = decode_operand(code, pos, rex, &inst, &inst.op1, modrm, rip);
        inst.op2.type = OP_REG;
        inst.op2.reg = reg;
    }
    // 0x50-0x57: PUSH reg
    else if (opcode >= 0x50 && opcode <= 0x57) {
        inst.type = INST_PUSH;
        inst.op1.type = OP_REG;
        inst.op1.reg = opcode - 0x50;
        if (rex & 0x01) inst.op1.reg += 8;
    }
    // 0x58-0x5F: POP reg
    else if (opcode >= 0x58 && opcode <= 0x5F) {
        inst.type = INST_POP;
        inst.op1.type = OP_REG;
        inst.op1.reg = opcode - 0x58;
        if (rex & 0x01) inst.op1.reg += 8;
    }
    // 0x74: JZ rel8
    else if (opcode == 0x74) {
        inst.type = INST_JZ;
        inst.op1.type = OP_IMM;
        inst.op1.imm = (int8_t)code[pos++];
    }
    // 0x75: JNZ rel8
    else if (opcode == 0x75) {
        inst.type = INST_JNZ;
        inst.op1.type = OP_IMM;
        inst.op1.imm = (int8_t)code[pos++];
    }
    // E8: CALL rel32
    else if (opcode == 0xE8) {
        inst.type = INST_CALL;
        inst.op1.type = OP_IMM;
        inst.op1.imm = *(int32_t *)&code[pos];
        pos += 4;
    }
    // C3: RET
    else if (opcode == 0xC3) {
        inst.type = INST_RET;
    }
    // EB: JMP rel8
    else if (opcode == 0xEB) {
        inst.type = INST_JMP;
        inst.op1.type = OP_IMM;
        inst.op1.imm = (int8_t)code[pos++];
    }
    // E9: JMP rel32
    else if (opcode == 0xE9) {
        inst.type = INST_JMP_REL32;
        inst.op1.type = OP_IMM;
        inst.op1.imm = *(int32_t *)&code[pos];
        pos += 4;
    }
    // FF /4: JMP r/m64 (indirect)
    else if (opcode == 0xFF) {
        uint8_t modrm = code[pos++];
        uint8_t sub_op = (modrm >> 3) & 0x07;
        if (sub_op == 4) {
            inst.type = INST_JMP_INDIRECT;
            pos = decode_operand(code, pos, rex, &inst, &inst.op1, modrm, rip);
        } else if (sub_op == 6) {
            inst.type = INST_PUSH;
            pos = decode_operand(code, pos, rex, &inst, &inst.op1, modrm, rip);
        }
    }
    // 0F 05: SYSCALL
    else if (opcode == 0x0F && code[pos] == 0x05) {
        inst.type = INST_SYSCALL;
        pos++;
    }
    // 90: NOP
    else if (opcode == 0x90) {
        inst.type = INST_NOP;
    }

    inst.size = pos;
    return inst;
}
