#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include "../include/syscall.h"

// x86_64 Syscall Numbers
#define X86_64_NR_READ  0
#define X86_64_NR_WRITE 1
#define X86_64_NR_MMAP  9
#define X86_64_NR_BRK   12
#define X86_64_NR_EXIT  60

static uint64_t current_brk = 0;

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
        case X86_64_NR_BRK: {
            uint64_t new_brk = cpu->rdi;
            if (current_brk == 0) {
                current_brk = 0x40000000; // Start of heap
            }
            if (new_brk == 0) {
                cpu->rax = current_brk;
            } else if (new_brk >= current_brk) {
                // Simplified brk: just return success
                current_brk = new_brk;
                cpu->rax = current_brk;
            } else {
                cpu->rax = current_brk;
            }
            break;
        }
        case X86_64_NR_MMAP: {
            // Note: mmap args on x86_64 are RDI, RSI, RDX, R10, R8, R9
            // Android/Linux ARM64 mmap args are slightly different but mmap() wrapper handles it
            int prot = (int)cpu->rdx;
            // Always remove PROT_EXEC for Android
            prot &= ~PROT_EXEC;

            void *addr = mmap((void *)cpu->rdi, (size_t)cpu->rsi, prot,
                               (int)cpu->r10, (int)cpu->r8, (off_t)cpu->r9);
            if (addr == MAP_FAILED) {
                perror("[Mimic] Syscall MMAP falló");
                cpu->rax = -1;
            } else {
                cpu->rax = (uint64_t)addr;
            }
            break;
        }
        default:
            fprintf(stderr, "[Mimic] Syscall no soportada: %ld\n", nr);
            exit(1);
    }
}
