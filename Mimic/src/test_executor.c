#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <sys/mman.h>
#include "../include/decoder.h"
#include "../include/cpu.h"
#include "../include/globals.h"
#include "../include/executor.h"

// Note: G_MEM_BASE is defined in globals.h (0x10000000UL)
// We need to map this in the host.

int main() {
    printf("[Test] Probando Executor logic...\n");

    void *mem = mmap((void *)G_MEM_BASE, 0x1000000, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | MAP_FIXED, -1, 0);
    if (mem == MAP_FAILED) {
        perror("mmap fixed failed (maybe permission?)");
        // Fallback: allocate anywhere and redefine translate_addr is harder.
        // Let's assume it works in the assistant's sandbox.
        exit(1);
    }

    CPUState cpu;
    memset(&cpu, 0, sizeof(cpu));
    cpu.rip = 0x400000;
    cpu.rsp = 0x800000;

    // Test: MOV RBP, [RSP+8] (48 8b 6c 24 08)
    // RSP = 0x800000 -> emulated addr of [RSP+8] = 0x800008
    // host addr of [RSP+8] = G_MEM_BASE + 0x800008
    uint8_t *host_stack_ptr = (uint8_t *)(G_MEM_BASE + 0x800008);
    *(uint64_t *)host_stack_ptr = 0x123456789ABCDEF0ULL;

    uint8_t *host_code_ptr = (uint8_t *)(G_MEM_BASE + 0x400000);
    uint8_t code[] = {0x48, 0x8b, 0x6c, 0x24, 0x08, 0xC3}; // MOV RBP, [RSP+8] then RET (C3 to exit execute)
    memcpy(host_code_ptr, code, sizeof(code));

    // Stack limits for executor: [0x500000, 0x900000)
    // Code limits: [G_MEM_BASE, G_MEM_END)
    // Note: execute uses emulated addresses for limits? Let's check executor.c
    // static int is_valid_addr(addr, sb, st, cs, ce)
    // wait, execute calls is_valid_addr with addr which is usually translated host addr.

    // Let's call execute.
    // It will continue until RIP=0 or some exit. RET at 0x400005 will read stack[rsp] and jump.
    // Let's set stack[0x800000] = 0 to stop.
    *(uint64_t *)(G_MEM_BASE + 0x800000) = 0ULL;

    printf("  [INFO] Starting execution at RIP=0x%lx\n", cpu.rip);
    execute(&cpu, G_MEM_BASE + 0x500000, G_MEM_BASE + 0x900000, G_MEM_BASE, G_MEM_END);

    printf("  [RESULT] RBP = 0x%lx\n", cpu.rbp);
    assert(cpu.rbp == 0x123456789ABCDEF0ULL);
    printf("  [OK] Executor correctly moved data from emulated stack to RBP.\n");

    return 0;
}
