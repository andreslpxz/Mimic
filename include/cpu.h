#ifndef CPU_H
#define CPU_H

#include <stdint.h>

typedef struct {
    uint64_t rax, rbx, rcx, rdx;
    uint64_t rsi, rdi, rbp, rsp;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rip;
    uint32_t rflags;
} CPUState;

void cpu_init(CPUState *cpu, uint64_t entry_point, uint64_t stack_ptr);

#endif
