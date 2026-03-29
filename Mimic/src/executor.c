#include <stdio.h>
#include <stdlib.h>
#include "../include/executor.h"
#include "../include/decoder.h"
#include "../include/syscall.h"
#include "../include/cpu.h"
#include "../include/globals.h"

#define ZF (1 << 6) // Zero Flag
#define SF (1 << 7) // Sign Flag

static int is_valid_addr(uint64_t addr, uint64_t stack_base, uint64_t stack_top, uint64_t code_start, uint64_t code_end) {
    if (addr >= stack_base && addr < stack_top) return 1;
    if (addr >= code_start && addr < code_end) return 1;
    return 0;
}

static uint64_t read_mem64_safe(uint64_t addr, uint64_t stack_base, uint64_t stack_top, uint64_t code_start, uint64_t code_end) {
    if (!is_valid_addr(addr, stack_base, stack_top, code_start, code_end)) {
        fprintf(stderr, "[Mimic] FATAL: Lectura en 0x%lx fuera de rango.\n", addr);
        exit(1);
    }
    return *(uint64_t *)addr;
}

static void write_mem64_safe(uint64_t addr, uint64_t value, uint64_t stack_base, uint64_t stack_top, uint64_t code_start, uint64_t code_end) {
    if (!is_valid_addr(addr, stack_base, stack_top, code_start, code_end)) {
        fprintf(stderr, "[Mimic] FATAL: Escritura en 0x%lx fuera de rango.\n", addr);
        exit(1);
    }
    *(uint64_t *)addr = value;
}

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

static uint64_t calculate_ea(CPUState *cpu, Operand *op, uint64_t next_rip) {
    if (op->type != OP_MEM) return 0;

    uint64_t addr = 0;
    if (op->mem.rip_rel) {
        addr = next_rip + op->mem.disp;
    } else {
        if (op->mem.base_reg != -1) {
            uint64_t *base = get_reg(cpu, op->mem.base_reg);
            if (base) addr += *base;
        }
        if (op->mem.index_reg != -1) {
            uint64_t *index = get_reg(cpu, op->mem.index_reg);
            if (index) addr += (*index * op->mem.scale);
        }
        addr += op->mem.disp;
    }
    return addr;
}

static uint64_t get_operand_val(CPUState *cpu, Operand *op, uint64_t next_rip, uint64_t sb, uint64_t st, uint64_t cs, uint64_t ce, uint8_t rex_w) {
    if (op->type == OP_REG) {
        uint64_t *reg = get_reg(cpu, op->reg);
        if (rex_w) return *reg;
        return (uint32_t)*reg;
    } else if (op->type == OP_IMM) {
        return op->imm;
    } else if (op->type == OP_MEM) {
        uint64_t addr = calculate_ea(cpu, op, next_rip);
        uint64_t host_addr = translate_addr(addr);
        return read_mem64_safe(host_addr, sb, st, cs, ce);
    }
    return 0;
}

static void set_operand_val(CPUState *cpu, Operand *op, uint64_t val, uint64_t next_rip, uint64_t sb, uint64_t st, uint64_t cs, uint64_t ce, uint8_t rex_w) {
    if (op->type == OP_REG) {
        uint64_t *reg = get_reg(cpu, op->reg);
        if (rex_w) {
            *reg = val;
        } else {
            *reg = (uint32_t)val; // Zero-extension in x86_64 for 32-bit writes
        }
    } else if (op->type == OP_MEM) {
        uint64_t addr = calculate_ea(cpu, op, next_rip);
        uint64_t host_addr = translate_addr(addr);
        write_mem64_safe(host_addr, val, sb, st, cs, ce);
    }
}

static void update_flags(CPUState *cpu, uint64_t res) {
    if (res == 0) cpu->rflags |= ZF;
    else cpu->rflags &= ~ZF;
    if ((int64_t)res < 0) cpu->rflags |= SF;
    else cpu->rflags &= ~SF;
}

void execute(CPUState *cpu, uint64_t stack_base, uint64_t stack_top, uint64_t code_start, uint64_t code_end) {
    while (1) {
        uint64_t host_rip = translate_addr(cpu->rip);
        if (!is_valid_host_addr(host_rip)) {
            fprintf(stderr, "[Mimic] PANIC: RIP fuera de rango emulado: 0x%lx\n", cpu->rip);
            exit(1);
        }

        DecodedInstruction inst = decode((uint8_t *)host_rip, cpu->rip);
        if (inst.type == INST_UNKNOWN) {
            fprintf(stderr, "[Mimic] PANIC: Instrucción desconocida en 0x%lx (host: 0x%lx)\n", cpu->rip, host_rip);
            uint8_t *p = (uint8_t *)host_rip;
            fprintf(stderr, "[Mimic] Opcode dump: %02x %02x %02x %02x %02x\n", p[0], p[1], p[2], p[3], p[4]);
            exit(1);
        }

        uint64_t next_rip = cpu->rip + inst.size;
        uint8_t rex_w = (inst.rex & 0x08) != 0;

        switch (inst.type) {
            case INST_MOV: {
                uint64_t val = get_operand_val(cpu, &inst.op2, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                set_operand_val(cpu, &inst.op1, val, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                cpu->rip = next_rip;
                break;
            }
            case INST_ADD: {
                uint64_t v1 = get_operand_val(cpu, &inst.op1, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                uint64_t v2 = get_operand_val(cpu, &inst.op2, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                uint64_t res = v1 + v2;
                set_operand_val(cpu, &inst.op1, res, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                update_flags(cpu, res);
                cpu->rip = next_rip;
                break;
            }
            case INST_SUB: {
                uint64_t v1 = get_operand_val(cpu, &inst.op1, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                uint64_t v2 = get_operand_val(cpu, &inst.op2, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                uint64_t res = v1 - v2;
                set_operand_val(cpu, &inst.op1, res, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                update_flags(cpu, res);
                cpu->rip = next_rip;
                break;
            }
            case INST_CMP: {
                uint64_t v1 = get_operand_val(cpu, &inst.op1, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                uint64_t v2 = get_operand_val(cpu, &inst.op2, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                update_flags(cpu, v1 - v2);
                cpu->rip = next_rip;
                break;
            }
            case INST_LEA: {
                uint64_t addr = calculate_ea(cpu, &inst.op2, next_rip);
                set_operand_val(cpu, &inst.op1, addr, next_rip, stack_base, stack_top, code_start, code_end, 1); // LEA is always 64-bit in long mode
                cpu->rip = next_rip;
                break;
            }
            case INST_PUSH: {
                uint64_t val = get_operand_val(cpu, &inst.op1, next_rip, stack_base, stack_top, code_start, code_end, 1);
                cpu->rsp -= 8;
                write_mem64_safe(translate_addr(cpu->rsp), val, stack_base, stack_top, code_start, code_end);
                cpu->rip = next_rip;
                break;
            }
            case INST_POP: {
                uint64_t val = read_mem64_safe(translate_addr(cpu->rsp), stack_base, stack_top, code_start, code_end);
                cpu->rsp += 8;
                set_operand_val(cpu, &inst.op1, val, next_rip, stack_base, stack_top, code_start, code_end, 1);
                cpu->rip = next_rip;
                break;
            }
            case INST_XOR: {
                uint64_t v1 = get_operand_val(cpu, &inst.op1, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                uint64_t v2 = get_operand_val(cpu, &inst.op2, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                uint64_t res = v1 ^ v2;
                set_operand_val(cpu, &inst.op1, res, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                update_flags(cpu, res);
                cpu->rip = next_rip;
                break;
            }
            case INST_AND: {
                uint64_t v1 = get_operand_val(cpu, &inst.op1, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                uint64_t v2 = get_operand_val(cpu, &inst.op2, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                uint64_t res = v1 & v2;
                set_operand_val(cpu, &inst.op1, res, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                update_flags(cpu, res);
                cpu->rip = next_rip;
                break;
            }
            case INST_OR: {
                uint64_t v1 = get_operand_val(cpu, &inst.op1, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                uint64_t v2 = get_operand_val(cpu, &inst.op2, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                uint64_t res = v1 | v2;
                set_operand_val(cpu, &inst.op1, res, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                update_flags(cpu, res);
                cpu->rip = next_rip;
                break;
            }
            case INST_TEST: {
                uint64_t v1 = get_operand_val(cpu, &inst.op1, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                uint64_t v2 = get_operand_val(cpu, &inst.op2, next_rip, stack_base, stack_top, code_start, code_end, rex_w);
                update_flags(cpu, v1 & v2);
                cpu->rip = next_rip;
                break;
            }
            case INST_JMP:
            case INST_JMP_REL32: {
                cpu->rip = next_rip + (int64_t)inst.op1.imm;
                break;
            }
            case INST_JMP_INDIRECT: {
                cpu->rip = get_operand_val(cpu, &inst.op1, next_rip, stack_base, stack_top, code_start, code_end, 1);
                break;
            }
            case INST_JZ: {
                if (cpu->rflags & ZF) cpu->rip = next_rip + (int64_t)inst.op1.imm;
                else cpu->rip = next_rip;
                break;
            }
            case INST_JNZ: {
                if (!(cpu->rflags & ZF)) cpu->rip = next_rip + (int64_t)inst.op1.imm;
                else cpu->rip = next_rip;
                break;
            }
            case INST_CALL: {
                cpu->rsp -= 8;
                write_mem64_safe(translate_addr(cpu->rsp), next_rip, stack_base, stack_top, code_start, code_end);
                cpu->rip = next_rip + (int64_t)inst.op1.imm;
                break;
            }
            case INST_RET: {
                cpu->rip = read_mem64_safe(translate_addr(cpu->rsp), stack_base, stack_top, code_start, code_end);
                cpu->rsp += 8;
                break;
            }
            case INST_SYSCALL: {
                handle_syscall(cpu);
                cpu->rip = next_rip;
                break;
            }
            case INST_NOP: {
                cpu->rip = next_rip;
                break;
            }
            default:
                fprintf(stderr, "[Mimic] Instrucción soportada pero no implementada en executor: %d\n", inst.type);
                exit(1);
        }

        if (cpu->rip == 0) break;
    }
}
