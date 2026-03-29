#include <stdio.h>
#include "../include/decoder.h"

DecodedInstruction decode(uint8_t *code, uint64_t rip) {
    DecodedInstruction inst = {0};
    uint8_t opcode = code[0];

    // B8 +rd id : MOV rd, imm32
    if (opcode >= 0xB8 && opcode <= 0xBF) {
        inst.type = INST_MOV_IMM;
        inst.reg = opcode - 0xB8;
        inst.imm = *(uint32_t *)&code[1];
        inst.size = 5;
    }
    // 48 B8 +rd io : MOV r64, imm64 (REX.W prefix)
    else if (code[0] == 0x48 && code[1] >= 0xB8 && code[1] <= 0xBF) {
        inst.type = INST_MOV_IMM;
        inst.reg = code[1] - 0xB8;
        inst.imm = *(uint64_t *)&code[2];
        inst.size = 10;
    }
    // E8 cd : CALL rel32
    else if (opcode == 0xE8) {
        inst.type = INST_CALL;
        inst.imm = *(uint32_t *)&code[1];
        inst.size = 5;
    }
    // C3 : RET
    else if (opcode == 0xC3) {
        inst.type = INST_RET;
        inst.size = 1;
    }
    // 0F 05 : SYSCALL
    else if (opcode == 0x0F && code[1] == 0x05) {
        inst.type = INST_SYSCALL;
        inst.size = 2;
    }
    // 50-57 : PUSH reg
    else if (opcode >= 0x50 && opcode <= 0x57) {
        inst.type = INST_PUSH;
        inst.reg = opcode - 0x50;
        inst.size = 1;
    }
    // 58-5F : POP reg
    else if (opcode >= 0x58 && opcode <= 0x5F) {
        inst.type = INST_POP;
        inst.reg = opcode - 0x58;
        inst.size = 1;
    }
    // EB cb : JMP rel8
    else if (opcode == 0xEB) {
        inst.type = INST_JMP;
        inst.imm = (int8_t)code[1];
        inst.size = 2;
    }
    else {
        inst.type = INST_UNKNOWN;
        inst.size = 1;
    }

    return inst;
}
