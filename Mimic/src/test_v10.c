#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include "../include/decoder.h"
#include "../include/cpu.h"
#include "../include/globals.h"

void test_decoder() {
    printf("[Test] Probando Decoder...\n");

    // Case 1: MOV RBP, [RSP+8] (48 8b 6c 24 08)
    uint8_t code1[] = {0x48, 0x8b, 0x6c, 0x24, 0x08};
    DecodedInstruction inst1 = decode(code1, 0x1011a8a0);
    assert(inst1.type == INST_MOV);
    assert(inst1.op1.type == OP_REG);
    assert(inst1.op1.reg == 5); // RBP
    assert(inst1.op2.type == OP_MEM);
    assert(inst1.op2.mem.base_reg == 4); // RSP
    assert(inst1.op2.mem.disp == 8);
    printf("  [OK] MOV RBP, [RSP+8]\n");

    // Case 2: LEA RAX, [RBX + RCX*4 + 0x100] (48 8d 84 8b 00 01 00 00)
    uint8_t code2[] = {0x48, 0x8d, 0x84, 0x8b, 0x00, 0x01, 0x00, 0x00};
    DecodedInstruction inst2 = decode(code2, 0);
    assert(inst2.type == INST_LEA);
    assert(inst2.op1.reg == 0); // RAX
    assert(inst2.op2.mem.base_reg == 3); // RBX
    assert(inst2.op2.mem.index_reg == 1); // RCX
    assert(inst2.op2.mem.scale == 4);
    assert(inst2.op2.mem.disp == 0x100);
    printf("  [OK] LEA RAX, [RBX + RCX*4 + 0x100]\n");

    // Case 3: ADD RAX, 0x10 (48 83 c0 10)
    uint8_t code3[] = {0x48, 0x83, 0xc0, 0x10};
    DecodedInstruction inst3 = decode(code3, 0);
    assert(inst3.type == INST_ADD);
    assert(inst3.op1.reg == 0); // RAX
    assert(inst3.op2.type == OP_IMM);
    assert(inst3.op2.imm == 0x10);
    printf("  [OK] ADD RAX, 0x10\n");

    // Case 4: RIP-relative LEA RAX, [RIP + 0x100] (48 8d 05 00 01 00 00)
    uint8_t code4[] = {0x48, 0x8d, 0x05, 0x00, 0x01, 0x00, 0x00};
    DecodedInstruction inst4 = decode(code4, 0x1000);
    assert(inst4.type == INST_LEA);
    assert(inst4.op2.mem.rip_rel == 1);
    assert(inst4.op2.mem.disp == 0x100);
    printf("  [OK] RIP-relative LEA RAX, [RIP+0x100]\n");

    // Case 5: 32-bit XOR EAX, ECX (31 c8) - Rex.W is 0
    uint8_t code5[] = {0x31, 0xc8};
    DecodedInstruction inst5 = decode(code5, 0);
    assert(inst5.type == INST_XOR);
    assert(inst5.op1.reg == 0); // EAX
    assert(inst5.op2.reg == 1); // ECX
    assert((inst5.rex & 0x08) == 0);
    printf("  [OK] XOR EAX, ECX (32-bit)\n");
}

int main() {
    test_decoder();
    printf("[Test] Todos los tests del decoder pasaron.\n");
    return 0;
}
