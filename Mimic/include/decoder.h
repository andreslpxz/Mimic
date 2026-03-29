#ifndef DECODER_H
#define DECODER_H

#include <stdint.h>
#include "cpu.h"

typedef enum {
    INST_UNKNOWN,
    INST_MOV,
    INST_ADD,
    INST_SUB,
    INST_CMP,
    INST_LEA,
    INST_PUSH,
    INST_POP,
    INST_JMP,
    INST_JMP_REL32,
    INST_JZ,
    INST_JNZ,
    INST_CALL,
    INST_RET,
    INST_SYSCALL,
    INST_XOR,
    INST_NOP,
    INST_AND,
    INST_OR,
    INST_TEST,
    INST_JMP_INDIRECT,
} InstructionType;

typedef enum {
    OP_NONE,
    OP_REG,
    OP_IMM,
    OP_MEM
} OperandType;

typedef struct {
    OperandType type;
    uint8_t reg;       // Register index (0-15) for OP_REG
    uint64_t imm;      // Immediate value for OP_IMM
    struct {
        int8_t base_reg;  // -1 if none, 0-15 otherwise
        int8_t index_reg; // -1 if none, 0-15 otherwise
        uint8_t scale;    // 1, 2, 4, 8
        int32_t disp;     // Displacement
        uint8_t rip_rel;  // 1 if RIP-relative
    } mem;
} Operand;

typedef struct {
    InstructionType type;
    uint8_t size;
    uint8_t rex;       // REX prefix bits (W, R, X, B)
    Operand op1;
    Operand op2;
} DecodedInstruction;

DecodedInstruction decode(uint8_t *code, uint64_t rip);

#endif
