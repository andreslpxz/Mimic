#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include "../include/syscall.h"

// x86_64 Syscall Numbers
#define X86_64_NR_READ  0
#define X86_64_NR_WRITE 1
#define X86_64_NR_EXIT  60

void handle_syscall(CPUState *cpu) {
    uint64_t nr = cpu->rax;

    switch (nr) {
        case X86_64_NR_READ: {
            cpu->rax = read((int)cpu->rdi, (void *)cpu->rsi, (size_t)cpu->rdx);
            break;
        }
        case X86_64_NR_WRITE: {
            cpu->rax = write((int)cpu->rdi, (void *)cpu->rsi, (size_t)cpu->rdx);
            break;
        }
        case X86_64_NR_EXIT: {
            printf("[Mimic] Programa finalizado con código %ld\n", cpu->rdi);
            exit((int)cpu->rdi);
            break;
        }
        default:
            fprintf(stderr, "[Mimic] Syscall no soportada: %ld\n", nr);
            exit(1);
    }
}
