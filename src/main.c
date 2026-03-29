#include <stdio.h>
#include "../include/decoder.h"
#include "../include/translator.h"
#include "../include/executor.h"

int main() {
    unsigned char program[] = {
        0xB8, // mov eax, imm
        0x05  // add eax, imm
    };

    int size = sizeof(program);

    printf("[Mimic] Traductor x86 -> ARM64\n");

    for(int i = 0; i < size; i++) {
        Instruction inst = decode(program[i]);
        translate(inst);
    }

    execute();

    return 0;
}
