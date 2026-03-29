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
    INST_CALL,
    INST_RET,
    INST_SYSCALL
} InstructionType;

typedef struct {
    InstructionType type;
    uint8_t size;
    uint64_t imm;
    uint8_t reg;
} DecodedInstruction;

DecodedInstruction decode(uint8_t *code, uint64_t rip);

#endif
