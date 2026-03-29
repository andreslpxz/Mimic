#ifndef DECODER_H
#define DECODER_H

#include <stdint.h>
#include "cpu.h"

typedef enum {
    INST_UNKNOWN,
    INST_MOV_IMM,
    INST_ADD_IMM,
    INST_SUB_IMM,
    INST_PUSH,
    INST_POP,
    INST_JMP,
    INST_JMP_REL32,
    INST_JZ,
    INST_JNZ,
    INST_CALL,
    INST_RET,
    INST_SYSCALL,
    INST_LEA,
    INST_CMP_IMM,
    INST_MOV_REG,
    INST_MOV_STORE,
    INST_XOR,
    INST_NOP
} InstructionType;

typedef struct {
    InstructionType type;
    uint8_t size;
    uint64_t imm;
    uint8_t reg;
    uint8_t reg2;
} DecodedInstruction;

DecodedInstruction decode(uint8_t *code, uint64_t rip);

#endif
