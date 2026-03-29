#include <stdio.h>
#include <stdlib.h>
#include "../include/executor.h"
#include "../include/decoder.h"
#include "../include/syscall.h"

uint64_t *get_reg(CPUState *cpu, uint8_t reg) {
    switch (reg) {
        case 0: return &cpu->rax;
        case 1: return &cpu->rcx;
        case 2: return &cpu->rdx;
        case 3: return &cpu->rbx;
        case 4: return &cpu->rsp;
        case 5: return &cpu->rbp;
        case 6: return &cpu->rsi;
        case 7: return &cpu->rdi;
        default: return NULL;
    }
}

void execute(CPUState *cpu) {
    while (1) {
        uint8_t *code = (uint8_t *)cpu->rip;
        DecodedInstruction inst = decode(code, cpu->rip);

        if (inst.type == INST_UNKNOWN) {
            fprintf(stderr, "[Mimic] Instrucción desconocida en 0x%lx: 0x%x\n", cpu->rip, *code);
            exit(1);
        }

        uint64_t next_rip = cpu->rip + inst.size;

        switch (inst.type) {
            case INST_MOV_IMM: {
                uint64_t *reg = get_reg(cpu, inst.reg);
                if (reg) *reg = inst.imm;
                cpu->rip = next_rip;
                break;
            }
            case INST_CALL: {
                cpu->rsp -= 8;
                *(uint64_t *)cpu->rsp = next_rip;
                cpu->rip = next_rip + (int32_t)inst.imm;
                break;
            }
            case INST_RET: {
                cpu->rip = *(uint64_t *)cpu->rsp;
                cpu->rsp += 8;
                break;
            }
            case INST_SYSCALL: {
                handle_syscall(cpu);
                cpu->rip = next_rip;
                break;
            }
            case INST_PUSH: {
                uint64_t *reg = get_reg(cpu, inst.reg);
                cpu->rsp -= 8;
                if (reg) *(uint64_t *)cpu->rsp = *reg;
                cpu->rip = next_rip;
                break;
            }
            case INST_POP: {
                uint64_t *reg = get_reg(cpu, inst.reg);
                if (reg) *reg = *(uint64_t *)cpu->rsp;
                cpu->rsp += 8;
                cpu->rip = next_rip;
                break;
            }
            case INST_JMP: {
                cpu->rip = next_rip + (int8_t)inst.imm;
                break;
            }
            default:
                cpu->rip = next_rip;
                break;
        }

        if (cpu->rip == 0) break;
    }
}
