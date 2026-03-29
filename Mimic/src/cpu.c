#include "../include/cpu.h"
#include <string.h>

void cpu_init(CPUState *cpu, uint64_t entry_point, uint64_t stack_ptr) {
    memset(cpu, 0, sizeof(CPUState));
    cpu->rip = entry_point;
    cpu->rsp = stack_ptr;
    cpu->rax = 0;
    cpu->rbx = 0;
    cpu->rcx = 0;
    cpu->rdx = 0;
    cpu->rsi = 0;
    cpu->rdi = 0;
    cpu->rbp = 0;
    cpu->rflags = 0x2; // Standard initial RFLAGS value
}

#include <stdio.h>

void cpu_dump(CPUState *cpu) {
    printf("\n--- CPU Dump at RIP: 0x%016lx ---\n", cpu->rip);
    printf("RAX: 0x%016lx  RBX: 0x%016lx\n", cpu->rax, cpu->rbx);
    printf("RCX: 0x%016lx  RDX: 0x%016lx\n", cpu->rcx, cpu->rdx);
    printf("RSI: 0x%016lx  RDI: 0x%016lx\n", cpu->rsi, cpu->rdi);
    printf("RBP: 0x%016lx  RSP: 0x%016lx\n", cpu->rbp, cpu->rsp);
    printf("R8:  0x%016lx  R9:  0x%016lx\n", cpu->r8,  cpu->r9);
    printf("R10: 0x%016lx  R11: 0x%016lx\n", cpu->r10, cpu->r11);
    printf("R12: 0x%016lx  R13: 0x%016lx\n", cpu->r12, cpu->r13);
    printf("R14: 0x%016lx  R15: 0x%016lx\n", cpu->r14, cpu->r15);
    printf("RFLAGS: 0x%08x\n", cpu->rflags);
    printf("---------------------------------------------\n");
}
