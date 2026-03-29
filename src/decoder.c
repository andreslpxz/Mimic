#include "../include/decoder.h"

Instruction decode(unsigned char byte) {
    Instruction inst;
    inst.opcode = byte;
    inst.operand = 0;
    return inst;
}
