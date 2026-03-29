#include <stdio.h>
#include <stdlib.h>
#include "../include/executor.h"
#include "../include/decoder.h"
#include "../include/syscall.h"
#include "../include/cpu.h"
#include "../include/globals.h"

#define ZF (1 << 6) // Zero Flag
#define SF (1 << 7) // Sign Flag

// Funciones seguras de acceso a memoria
// Una dirección es válida si está en el stack O en la región de código/datos del binario
static int is_valid_addr(uint64_t addr, uint64_t stack_base, uint64_t stack_top, uint64_t code_start, uint64_t code_end) {
    // Validar que esté en el stack
    if (addr >= stack_base && addr < stack_top) return 1;
    
    // Validar que esté en el rango de código/datos del binario
    if (addr >= code_start && addr < code_end) return 1;
    
    return 0;
}

static uint64_t read_mem64_safe(uint64_t addr, CPUState *cpu, uint64_t stack_base, uint64_t stack_top, uint64_t code_start, uint64_t code_end) {
    if (!is_valid_addr(addr, stack_base, stack_top, code_start, code_end)) {
        fprintf(stderr, "[Mimic] WARN: Lectura de dirección inválida 0x%lx\n", addr);
        return 0;
    }
    return *(uint64_t *)addr;
}

static void write_mem64_safe(uint64_t addr, uint64_t value, CPUState *cpu, uint64_t stack_base, uint64_t stack_top, uint64_t code_start, uint64_t code_end) {
    if (!is_valid_addr(addr, stack_base, stack_top, code_start, code_end)) {
        fprintf(stderr, "[Mimic] WARN: Escritura en dirección inválida 0x%lx\n", addr);
        return;
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

void execute(CPUState *cpu, uint64_t stack_base, uint64_t stack_top, uint64_t code_start, uint64_t code_end) {
    while (1) {
        // V9: SAFE FETCH ADAPTADO - Verificar que RIP está dentro de [G_MEM_BASE, G_MEM_END)
        if (cpu->rip < G_MEM_BASE || cpu->rip >= G_MEM_END) {
            fprintf(stderr, "[Mimic] ERROR CRÍTICO: RIP 0x%lx fuera del rango válido 0x%lx-0x%lx\n",
                    cpu->rip, G_MEM_BASE, G_MEM_END);
            cpu_dump(cpu);
            exit(1);
        }
        
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
                    write_mem64_safe(addr, *src, cpu, stack_base, stack_top, code_start, code_end);
                }
                cpu->rip = next_rip;
                break;
            }
            case INST_CALL: {
                cpu->rsp -= 8;
                write_mem64_safe(cpu->rsp, next_rip, cpu, stack_base, stack_top, code_start, code_end);
                
                // ESTRICTO v8: VERIFY CALL rel32 - Tratar como int32_t (con signo)
                int32_t signed_offset = (int32_t)inst.imm;
                uint64_t target = next_rip + signed_offset;
                
                // Validar que el salto sea a una dirección válida
                if (target < code_start || target >= code_end) {
                    fprintf(stderr, "[Mimic] ERROR: CALL a 0x%lx inválido (rango: 0x%lx-0x%lx, offset relativo: %d)\n", 
                            target, code_start, code_end, signed_offset);
                    fprintf(stderr, "[Mimic] Contexto: RIP anterior=0x%lx, next_rip=0x%lx\n", cpu->rip, next_rip);
                    cpu_dump(cpu);
                    exit(1);
                } else {
                    cpu->rip = target;
                    fprintf(stderr, "[Mimic] CALL a 0x%lx (desde 0x%lx, offset=%d)\n", cpu->rip, next_rip, signed_offset);
                }
                break;
            }
            case INST_RET: {
                cpu->rip = read_mem64_safe(cpu->rsp, cpu, stack_base, stack_top, code_start, code_end);
                cpu->rsp += 8;
                fprintf(stderr, "[Mimic] RET a 0x%lx\n", cpu->rip);
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
                if (reg) write_mem64_safe(cpu->rsp, *reg, cpu, stack_base, stack_top, code_start, code_end);
                cpu->rip = next_rip;
                break;
            }
            case INST_POP: {
                uint64_t *reg = get_reg(cpu, inst.reg);
                if (reg) *reg = read_mem64_safe(cpu->rsp, cpu, stack_base, stack_top, code_start, code_end);
                cpu->rsp += 8;
                cpu->rip = next_rip;
                break;
            }
            case INST_JMP: {
                cpu->rip = next_rip + (int8_t)inst.imm;
                break;
            }
            case INST_JMP_REL32: {
                // ESTRICTO v8: Verificar JMP rel32 también
                int32_t signed_offset = (int32_t)inst.imm;
                uint64_t target = next_rip + signed_offset;
                
                if (target < code_start || target >= code_end) {
                    fprintf(stderr, "[Mimic] ERROR: JMP rel32 a 0x%lx inválido (offset=%d)\n", target, signed_offset);
                    exit(1);
                }
                cpu->rip = target;
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
            case INST_AND_IMM: {
                uint64_t *reg = get_reg(cpu, inst.reg);
                if (reg) {
                    *reg &= inst.imm;
                    if (*reg == 0) cpu->rflags |= ZF;
                    else cpu->rflags &= ~ZF;
                }
                cpu->rip = next_rip;
                break;
            }
            case INST_OR_IMM: {
                uint64_t *reg = get_reg(cpu, inst.reg);
                if (reg) {
                    *reg |= inst.imm;
                    if (*reg == 0) cpu->rflags |= ZF;
                    else cpu->rflags &= ~ZF;
                }
                cpu->rip = next_rip;
                break;
            }
            case INST_SUB_REG: {
                uint64_t *src = get_reg(cpu, inst.reg);
                uint64_t *dest = get_reg(cpu, inst.reg2);
                if (src && dest) {
                    uint64_t res = *dest - *src;
                    *dest = res;
                    if (res == 0) cpu->rflags |= ZF;
                    else cpu->rflags &= ~ZF;
                    if ((int64_t)res < 0) cpu->rflags |= SF;
                    else cpu->rflags &= ~SF;
                }
                cpu->rip = next_rip;
                break;
            }
            case INST_ADD_REG: {
                uint64_t *src = get_reg(cpu, inst.reg);
                uint64_t *dest = get_reg(cpu, inst.reg2);
                if (src && dest) {
                    uint64_t res = *dest + *src;
                    *dest = res;
                    if (res == 0) cpu->rflags |= ZF;
                    else cpu->rflags &= ~ZF;
                    if ((int64_t)res < 0) cpu->rflags |= SF;
                    else cpu->rflags &= ~SF;
                }
                cpu->rip = next_rip;
                break;
            }
            case INST_TEST: {
                uint64_t *reg1 = get_reg(cpu, inst.reg);
                uint64_t *reg2 = get_reg(cpu, inst.reg2);
                if (reg1 && reg2) {
                    uint64_t res = *reg1 & *reg2;
                    if (res == 0) cpu->rflags |= ZF;
                    else cpu->rflags &= ~ZF;
                    if ((int64_t)res < 0) cpu->rflags |= SF;
                    else cpu->rflags &= ~SF;
                }
                cpu->rip = next_rip;
                break;
            }
            case INST_CMP_REG: {
                uint64_t *src = get_reg(cpu, inst.reg);
                uint64_t *dest = get_reg(cpu, inst.reg2);
                if (src && dest) {
                    uint64_t res = *dest - *src;
                    if (res == 0) cpu->rflags |= ZF;
                    else cpu->rflags &= ~ZF;
                    if ((int64_t)res < 0) cpu->rflags |= SF;
                    else cpu->rflags &= ~SF;
                }
                cpu->rip = next_rip;
                break;
            }
            case INST_MOV_LOAD: {
                uint64_t *dest = get_reg(cpu, inst.reg);
                uint64_t *base = get_reg(cpu, inst.reg2);
                if (dest && base) {
                    uint64_t addr = *base + (int64_t)inst.imm;
                    *dest = read_mem64_safe(addr, cpu, stack_base, stack_top, code_start, code_end);
                }
                cpu->rip = next_rip;
                break;
            }
            case INST_NOP_EXTENDED: {
                cpu->rip = next_rip;
                break;
            }
            case INST_JMP_INDIRECT: {
                // JMP r/m64 - salto indirecto
                uint64_t target = 0;
                uint64_t *reg = get_reg(cpu, inst.reg);
                
                // Caso 1: Salto a registro directo (mod=3)
                if (inst.reg2 == 0 && inst.imm == 0 && reg) {
                    target = *reg;
                } else {
                    // Caso 2: Salto indirecto [dirección] - necesita leer de memoria
                    uint64_t addr = 0;
                    
                    if (reg) {
                        addr = *reg + (int64_t)inst.imm;
                    } else {
                        uint64_t *base = get_reg(cpu, inst.reg2);
                        if (base) {
                            addr = *base + (int64_t)inst.imm;
                        }
                    }
                    
                    // Leer la dirección de salto desde la memoria
                    target = read_mem64_safe(addr, cpu, stack_base, stack_top, code_start, code_end);
                }
                
                // Validar que el salto sea a una dirección válida
                if (target < code_start || target >= code_end) {
                    fprintf(stderr, "[Mimic] ERROR: JMP INDIRECT a 0x%lx inválido (rango: 0x%lx-0x%lx)\n", 
                            target, code_start, code_end);
                    exit(1);
                } else {
                    cpu->rip = target;
                    fprintf(stderr, "[Mimic] JMP INDIRECT a 0x%lx\n", target);
                }
                break;
            }
            case INST_PUSH_IMM: {
                // PUSH imm32
                cpu->rsp -= 8;
                write_mem64_safe(cpu->rsp, inst.imm, cpu, stack_base, stack_top, code_start, code_end);
                cpu->rip = next_rip;
                break;
            }
            case INST_CMP_AL_IMM: {
                // CMP AL, imm8 - compara byte bajo de RAX con inmediato
                uint8_t al = (uint8_t)(cpu->rax & 0xFF);
                uint8_t imm = (uint8_t)inst.imm;
                uint8_t res = al - imm;
                
                if (res == 0) cpu->rflags |= ZF;
                else cpu->rflags &= ~ZF;
                if ((int8_t)res < 0) cpu->rflags |= SF;
                else cpu->rflags &= ~SF;
                
                cpu->rip = next_rip;
                break;
            }
            case INST_MOV_AL_IMM: {
                // MOV r8, imm8 - mueve a registro de 8 bits
                uint64_t *reg = get_reg(cpu, inst.reg);
                
                if (reg) {
                    // Modificar solo el byte bajo, mantener el resto de RAX intacto
                    *reg = (*reg & 0xFFFFFFFFFFFFFF00ULL) | ((uint8_t)inst.imm);
                }
                
                cpu->rip = next_rip;
                break;
            }
            case INST_CMP_BYTE_IMM: {
                // CMP r/m8, imm8 - compara byte con inmediato
                uint64_t *reg = get_reg(cpu, inst.reg);
                if (reg) {
                    uint8_t val = (uint8_t)(*reg & 0xFF);
                    uint8_t imm = (uint8_t)inst.imm;
                    uint8_t res = val - imm;
                    
                    if (res == 0) cpu->rflags |= ZF;
                    else cpu->rflags &= ~ZF;
                    if ((int8_t)res < 0) cpu->rflags |= SF;
                    else cpu->rflags &= ~SF;
                }
                
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
