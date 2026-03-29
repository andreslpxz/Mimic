#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include "../include/decoder.h"
#include "../include/cpu.h"
#include "../include/globals.h"
#include "../include/executor.h"

int main() {
    printf("[Test] Probando Executor logic Compleja (LEA/SIB/RIP-rel)...\n");

    void *mem = mmap((void *)G_MEM_BASE, 0x1000000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap fixed failed");
        exit(1);
    }

    CPUState cpu;
    memset(&cpu, 0, sizeof(cpu));
    cpu.rip = 0x400000;
    cpu.rsp = 0x800000;
    cpu.rbx = 0x1000;
    cpu.rcx = 0x5;

    // Stack return addr for RET:
    *(uint64_t *)(G_MEM_BASE + 0x800000) = 0ULL;

    uint8_t *host_code_ptr = (uint8_t *)(G_MEM_BASE + 0x400000);
    // LEA RAX, [RBX + RCX*8 + 0x100] (48 8d 84 cb 00 01 00 00)
    //rax = 0x1000 + 0x5*8 + 0x100 = 0x1128
    uint8_t code[] = {0x48, 0x8d, 0x84, 0xcb, 0x00, 0x01, 0x00, 0x00, 0xC3};
    memcpy(host_code_ptr, code, sizeof(code));

    execute(&cpu, G_MEM_BASE + 0x500000, G_MEM_BASE + 0x900000, G_MEM_BASE, G_MEM_END);

    printf("  [RESULT] RAX = 0x%lx (Expected 0x1128)\n", cpu.rax);
    assert(cpu.rax == 0x1128);
    printf("  [OK] LEA con SIB calculado correctamente.\n");

    // RIP-relative Test
    cpu.rip = 0x401000;
    // LEA RDI, [RIP + 0x2000] (48 8d 3d 00 20 00 00)
    // next_rip = 0x401007. rdi = 0x401007 + 0x2000 = 0x403007
    uint8_t code_rip[] = {0x48, 0x8d, 0x3d, 0x00, 0x20, 0x00, 0x00, 0xC3};
    memcpy((uint8_t *)(G_MEM_BASE + 0x401000), code_rip, sizeof(code_rip));

    execute(&cpu, G_MEM_BASE + 0x500000, G_MEM_BASE + 0x900000, G_MEM_BASE, G_MEM_END);
    printf("  [RESULT] RDI = 0x%lx (Expected 0x403007)\n", cpu.rdi);
    assert(cpu.rdi == 0x403007);
    printf("  [OK] LEA RIP-relativo calculado correctamente.\n");

    return 0;
}
