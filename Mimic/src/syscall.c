#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include "../include/syscall.h"
#include "../include/globals.h"

// x86_64 Syscall Numbers
#define X86_64_NR_READ       0
#define X86_64_NR_WRITE      1
#define X86_64_NR_MMAP       9
#define X86_64_NR_BRK        12
#define X86_64_NR_EXIT       60
#define X86_64_NR_ARCH_PRCTL 158

static uint64_t current_brk = 0;

void handle_syscall(CPUState *cpu) {
    uint64_t nr = cpu->rax;

    switch (nr) {
        case X86_64_NR_READ: {
            cpu->rax = read((int)cpu->rdi, (void *)translate_addr(cpu->rsi), (size_t)cpu->rdx);
            break;
        }
        case X86_64_NR_WRITE: {
            cpu->rax = write((int)cpu->rdi, (void *)translate_addr(cpu->rsi), (size_t)cpu->rdx);
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
                current_brk = 0x40000000; // Start of heap (emulated)
            }
            if (new_brk == 0) {
                cpu->rax = current_brk;
            } else if (new_brk >= current_brk) {
                current_brk = new_brk;
                cpu->rax = current_brk;
            } else {
                cpu->rax = current_brk;
            }
            break;
        }
        case X86_64_NR_MMAP: {
            int prot = (int)cpu->rdx;
            prot &= ~PROT_EXEC; // Safety for Android

            void *addr = mmap((void *)translate_addr(cpu->rdi), (size_t)cpu->rsi, prot,
                               (int)cpu->r10, (int)cpu->r8, (off_t)cpu->r9);
            if (addr == MAP_FAILED) {
                perror("[Mimic] Syscall MMAP falló");
                cpu->rax = -1;
            } else {
                uint64_t host_addr = (uint64_t)addr;
                if (host_addr >= G_MEM_BASE && host_addr < G_MEM_END) {
                    cpu->rax = host_addr - G_MEM_BASE;
                } else {
                    cpu->rax = host_addr;
                }
            }
            break;
        }
        case X86_64_NR_ARCH_PRCTL: {
            cpu->rax = 0; // Simular éxito para TLS
            break;
        }
        default:
            fprintf(stderr, "[Mimic] Syscall no soportada: %ld\n", nr);
            exit(1);
    }
}
