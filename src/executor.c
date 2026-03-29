#include <stdio.h>
#include <stdlib.h>
#include "../include/executor.h"
#include "../include/decoder.h"
#include "../include/syscall.h"
#include "../include/cpu.h"

#define ZF (1 << 6) // Zero Flag
#define SF (1 << 7) // Sign Flag

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
        case 8: return &cpu->r8;
        case 9: return &cpu->r9;
        case 10: return &cpu->r10;
        case 11: return &cpu->r11;
        case 12: return &cpu->r12;
        case 13: return &cpu->r13;
        case 14: return &cpu->r14;
        case 15: return &cpu->r15;
        default: return NULL;
    }
}

void execute(CPUState *cpu) {
    while (1) {
        uint8_t *code = (uint8_t *)cpu->rip;
        DecodedInstruction inst = decode(code, cpu->rip);

        if (inst.type == INST_UNKNOWN) {
            fprintf(stderr, "\n[Mimic] PANIC: Instrucción desconocida en 0x%lx\n", cpu->rip);
            fprintf(stderr, "[Mimic] Opcode bytes:");
            for (int i = 0; i < 8; i++) {
                fprintf(stderr, " %02x", code[i]);
            }
            fprintf(stderr, "\n");
            cpu_dump(cpu);
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
            case INST_MOV_REG: {
                uint64_t *src = get_reg(cpu, inst.reg);
                uint64_t *dest = get_reg(cpu, inst.reg2);
                if (src && dest) *dest = *src;
                cpu->rip = next_rip;
                break;
            }
            case INST_MOV_STORE: {
                uint64_t *src = get_reg(cpu, inst.reg);
                uint64_t *base = get_reg(cpu, inst.reg2);
                if (src && base) {
                    uint64_t addr = *base + (int64_t)inst.imm;
                    *(uint64_t *)addr = *src;
                }
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
            case INST_JMP_REL32: {
                cpu->rip = next_rip + (int32_t)inst.imm;
                break;
            }
            case INST_JZ: {
                if (cpu->rflags & ZF) cpu->rip = next_rip + (int8_t)inst.imm;
                else cpu->rip = next_rip;
                break;
            }
            case INST_JNZ: {
                if (!(cpu->rflags & ZF)) cpu->rip = next_rip + (int8_t)inst.imm;
                else cpu->rip = next_rip;
                break;
            }
            case INST_LEA: {
                uint64_t *reg = get_reg(cpu, inst.reg);
                if (reg) *reg = inst.imm;
                cpu->rip = next_rip;
                break;
            }
            case INST_CMP_IMM: {
                uint64_t *reg = get_reg(cpu, inst.reg);
                if (reg) {
                    uint64_t val = *reg;
                    uint64_t res = val - inst.imm;
                    if (res == 0) cpu->rflags |= ZF;
                    else cpu->rflags &= ~ZF;
                    if ((int64_t)res < 0) cpu->rflags |= SF;
                    else cpu->rflags &= ~SF;
                }
                cpu->rip = next_rip;
                break;
            }
            case INST_XOR: {
                uint64_t *reg1 = get_reg(cpu, inst.reg);
                uint64_t *reg2 = get_reg(cpu, inst.reg2);
                if (reg1 && reg2) {
                    *reg1 ^= *reg2;
                    if (*reg1 == 0) cpu->rflags |= ZF;
                    else cpu->rflags &= ~ZF;
                }
                cpu->rip = next_rip;
                break;
            }
            case INST_NOP: {
                cpu->rip = next_rip;
                break;
            }
            default:
                cpu->rip = next_rip;
                break;
        }

        if (cpu->rip == 0) break;
    }
}
