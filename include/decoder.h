#ifndef DECODER_H
#define DECODER_H

typedef struct {
    unsigned char opcode;
    int operand;
} Instruction;

Instruction decode(unsigned char byte);

#endif
