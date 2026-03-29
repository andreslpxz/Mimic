#include <stdio.h>
#include "../include/translator.h"

void translate(Instruction inst) {
    switch(inst.opcode) {
        case 0xB8:
            printf("ARM64: mov w0, #imm\n");
            break;
        case 0x05:
            printf("ARM64: add w0, w0, #imm\n");
            break;
        default:
            printf("Instrucción no soportada: 0x%X\n", inst.opcode);
    }
}
