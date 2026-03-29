#include <stdio.h>
#include "../include/translator.h"

void translate(DecodedInstruction inst) {
    // Usamos inst.type porque es lo que definiste en decoder.h
    switch(inst.type) {
        case INST_MOV_IMM:
            printf("[Mimic-Trans] x86_64 MOV -> ARM64: mov x%d, #0x%lx\n", inst.reg, inst.imm);
            break;

        case INST_ADD_IMM:
            printf("[Mimic-Trans] x86_64 ADD -> ARM64: add x%d, x%d, #%ld\n", inst.reg, inst.reg, inst.imm);
            break;

        case INST_SYSCALL:
            printf("[Mimic-Trans] x86_64 SYSCALL -> ARM64: svc #0\n");
            break;

        case INST_NOP:
            printf("[Mimic-Trans] x86_64 NOP -> ARM64: nop\n");
            break;

        case INST_UNKNOWN:
        default:
            // Como no tenemos el campo 'opcode' en la struct, 
            // imprimimos el tipo del enum para depurar.
            printf("[Mimic-Trans] Instrucción tipo %d no soportada aún.\n", inst.type);
            break;
    }
}

