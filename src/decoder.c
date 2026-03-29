#include <stdio.h>
#include "../include/decoder.h"

DecodedInstruction decode(uint8_t *code, uint64_t rip) {
    DecodedInstruction inst = {0};
    uint8_t opcode = code[0];

    // ENDBR64 (F3 0F 1E FA)
    if (code[0] == 0xF3 && code[1] == 0x0F && code[2] == 0x1E && code[3] == 0xFA) {
        inst.type = INST_NOP;
        inst.size = 4;
        return inst;
    }

    // Check for REX prefix (0x40 - 0x4F)
    uint8_t rex = 0;
    if (opcode >= 0x40 && opcode <= 0x4F) {
        rex = opcode;
        code++;
        opcode = code[0];
        inst.size++;
    }

    // B8 +rd id : MOV rd, imm32
    if (opcode >= 0xB8 && opcode <= 0xBF) {
        inst.type = INST_MOV_IMM;
        inst.reg = opcode - 0xB8;
        if (rex & 0x01) inst.reg += 8; // REX.B

        if (rex & 0x08) { // REX.W -> 64-bit imm
            inst.imm = *(uint64_t *)&code[1];
            inst.size += 9;
        } else {
            inst.imm = *(uint32_t *)&code[1];
            inst.size += 5;
        }
    }
    // E8 cd : CALL rel32
    else if (opcode == 0xE8) {
        inst.type = INST_CALL;
        inst.imm = *(uint32_t *)&code[1];
        inst.size += 5;
    }
    // C3 : RET
    else if (opcode == 0xC3) {
        inst.type = INST_RET;
        inst.size += 1;
    }
    // 0F 05 : SYSCALL
    else if (opcode == 0x0F && code[1] == 0x05) {
        inst.type = INST_SYSCALL;
        inst.size += 2;
    }
    // 50-57 : PUSH reg
    else if (opcode >= 0x50 && opcode <= 0x57) {
        inst.type = INST_PUSH;
        inst.reg = opcode - 0x50;
        if (rex & 0x01) inst.reg += 8;
        inst.size += 1;
    }
    // 58-5F : POP reg
    else if (opcode >= 0x58 && opcode <= 0x5F) {
        inst.type = INST_POP;
        inst.reg = opcode - 0x58;
        if (rex & 0x01) inst.reg += 8;
        inst.size += 1;
    }
    // EB cb : JMP rel8
    else if (opcode == 0xEB) {
        inst.type = INST_JMP;
        inst.imm = (int8_t)code[1];
        inst.size += 2;
    }
    // E9 cd : JMP rel32
    else if (opcode == 0xE9) {
        inst.type = INST_JMP_REL32;
        inst.imm = *(uint32_t *)&code[1];
        inst.size += 5;
    }
    // 74 cb : JZ rel8
    else if (opcode == 0x74) {
        inst.type = INST_JZ;
        inst.imm = (int8_t)code[1];
        inst.size += 2;
    }
    // 75 cb : JNZ rel8
    else if (opcode == 0x75) {
        inst.type = INST_JNZ;
        inst.imm = (int8_t)code[1];
        inst.size += 2;
    }
    // 8D /r : LEA r64, m
    else if (opcode == 0x8D) {
        inst.type = INST_LEA;
        uint8_t modrm = code[1];
        inst.reg = (modrm >> 3) & 0x07;
        if (rex & 0x04) inst.reg += 8; // REX.R

        // Simplified: only support [rip + disp32] or [reg]
        if ((modrm & 0xC7) == 0x05) { // RIP-relative
            inst.imm = rip + inst.size + 5 + *(int32_t *)&code[2];
            inst.size += 5;
        } else {
            inst.type = INST_UNKNOWN; // ModRM logic is complex
        }
    }
    // 83 /7 ib : CMP r/m64, imm8
    else if (opcode == 0x83 && (code[1] & 0x38) == 0x38) {
        inst.type = INST_CMP_IMM;
        inst.reg = code[1] & 0x07;
        if (rex & 0x01) inst.reg += 8;
        inst.imm = (int8_t)code[2];
        inst.size += 3;
    }
    // 31 /r : XOR r/m32, r32
    else if (opcode == 0x31) {
        inst.type = INST_XOR;
        uint8_t modrm = code[1];
        inst.reg = modrm & 0x07;
        inst.reg2 = (modrm >> 3) & 0x07;
        if (rex & 0x01) inst.reg += 8;
        if (rex & 0x04) inst.reg2 += 8;
        inst.size += 2;
    }
    else {
        inst.type = INST_UNKNOWN;
        inst.size = 1;
    }

    return inst;
}
