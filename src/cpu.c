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
