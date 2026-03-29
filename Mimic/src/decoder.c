#include <stdio.h>
#include "../include/decoder.h"

DecodedInstruction decode(uint8_t *code, uint64_t rip) {
    DecodedInstruction inst = {0};
    int pos = 0;

    // ENDBR64 (F3 0F 1E FA)
    if (code[0] == 0xF3 && code[1] == 0x0F && code[2] == 0x1E && code[3] == 0xFA) {
        inst.type = INST_NOP;
        inst.size = 4;
        return inst;
    }

    uint8_t rex = 0;
    if (code[pos] >= 0x40 && code[pos] <= 0x4F) {
        rex = code[pos];
        pos++;
    }

    uint8_t opcode = code[pos];
    pos++;

    // B8 +rd id : MOV rd, imm32
    if (opcode >= 0xB8 && opcode <= 0xBF) {
        inst.type = INST_MOV_IMM;
        inst.reg = opcode - 0xB8;
        if (rex & 0x01) inst.reg += 8; // REX.B

        if (rex & 0x08) { // REX.W -> 64-bit imm
            inst.imm = *(uint64_t *)&code[pos];
            pos += 8;
        } else {
            inst.imm = *(uint32_t *)&code[pos];
            pos += 4;
        }
    }
    // 89 /r : MOV r/m64, r64
    else if (opcode == 0x89) {
        uint8_t modrm = code[pos];
        pos++;
        uint8_t mod = (modrm >> 6) & 0x03;
        uint8_t reg_f = (modrm >> 3) & 0x07;
        uint8_t rm_f = modrm & 0x07;

        if (rex & 0x04) reg_f += 8; // REX.R

        // reg: source, reg2: destination
        inst.reg = reg_f;

        if (mod == 3) {
            inst.type = INST_MOV_REG;
            if (rex & 0x01) rm_f += 8; // REX.B
            inst.reg2 = rm_f;
        } else {
            inst.type = INST_MOV_STORE;
            uint8_t base = rm_f;
            if (rm_f == 4) { // SIB
                uint8_t sib = code[pos];
                pos++;
                base = sib & 0x07;
                if (rex & 0x01) base += 8;
            } else {
                if (rex & 0x01) base += 8;
            }
            inst.reg2 = base;

            if (mod == 1) { // disp8
                inst.imm = (int8_t)code[pos];
                pos++;
            } else if (mod == 2 || (mod == 0 && rm_f == 5)) { // disp32
                inst.imm = *(int32_t *)&code[pos];
                pos += 4;
            }
        }
    }
    // E8 cd : CALL rel32
    else if (opcode == 0xE8) {
        inst.type = INST_CALL;
        inst.imm = *(uint32_t *)&code[pos];
        pos += 4;
    }
    // C3 : RET
    else if (opcode == 0xC3) {
        inst.type = INST_RET;
    }
    // 0F 05 : SYSCALL
    else if (opcode == 0x0F && code[pos] == 0x05) {
        inst.type = INST_SYSCALL;
        pos += 1;
    }
    // 50-57 : PUSH reg
    else if (opcode >= 0x50 && opcode <= 0x57) {
        inst.type = INST_PUSH;
        inst.reg = opcode - 0x50;
        if (rex & 0x01) inst.reg += 8;
    }
    // 58-5F : POP reg
    else if (opcode >= 0x58 && opcode <= 0x5F) {
        inst.type = INST_POP;
        inst.reg = opcode - 0x58;
        if (rex & 0x01) inst.reg += 8;
    }
    // EB cb : JMP rel8
    else if (opcode == 0xEB) {
        inst.type = INST_JMP;
        inst.imm = (int8_t)code[pos];
        pos += 1;
    }
    // E9 cd : JMP rel32
    else if (opcode == 0xE9) {
        inst.type = INST_JMP_REL32;
        inst.imm = *(uint32_t *)&code[pos];
        pos += 4;
    }
    // 74 cb : JZ rel8
    else if (opcode == 0x74) {
        inst.type = INST_JZ;
        inst.imm = (int8_t)code[pos];
        pos += 1;
    }
    // 75 cb : JNZ rel8
    else if (opcode == 0x75) {
        inst.type = INST_JNZ;
        inst.imm = (int8_t)code[pos];
        pos += 1;
    }
    // 8D /r : LEA r64, m
    else if (opcode == 0x8D) {
        inst.type = INST_LEA;
        uint8_t modrm = code[pos];
        pos++;
        inst.reg = (modrm >> 3) & 0x07;
        if (rex & 0x04) inst.reg += 8; // REX.R

        if ((modrm & 0xC7) == 0x05) { // RIP-relative
            inst.imm = rip + pos + 4 + *(int32_t *)&code[pos];
            pos += 4;
        } else {
            inst.type = INST_UNKNOWN;
        }
    }
    // 83 /7 ib : CMP r/m64, imm8
    else if (opcode == 0x83 && (code[pos] & 0x38) == 0x38) {
        inst.type = INST_CMP_IMM;
        uint8_t modrm = code[pos];
        pos++;
        inst.reg = modrm & 0x07;
        if (rex & 0x01) inst.reg += 8;
        inst.imm = (int8_t)code[pos];
        pos++;
    }
    // 31 /r : XOR r/m32, r32
    else if (opcode == 0x31) {
        inst.type = INST_XOR;
        uint8_t modrm = code[pos];
        pos++;
        inst.reg = modrm & 0x07;
        inst.reg2 = (modrm >> 3) & 0x07;
        if (rex & 0x01) inst.reg += 8;
        if (rex & 0x04) inst.reg2 += 8;
    }
    // 83 /4 ib : AND r/m64, imm8
    else if (opcode == 0x83 && (code[pos] & 0x38) == 0x20) {
        inst.type = INST_AND_IMM;
        uint8_t modrm = code[pos];
        pos++;
        inst.reg = modrm & 0x07;
        if (rex & 0x01) inst.reg += 8;
        inst.imm = (int8_t)code[pos];
        pos++;
    }
    // 83 /1 ib : OR r/m64, imm8
    else if (opcode == 0x83 && (code[pos] & 0x38) == 0x08) {
        inst.type = INST_OR_IMM;
        uint8_t modrm = code[pos];
        pos++;
        inst.reg = modrm & 0x07;
        if (rex & 0x01) inst.reg += 8;
        inst.imm = (int8_t)code[pos];
        pos++;
    }
    // 81 /4 id : AND r/m64, imm32
    else if (opcode == 0x81 && (code[pos] & 0x38) == 0x20) {
        inst.type = INST_AND_IMM;
        uint8_t modrm = code[pos];
        pos++;
        inst.reg = modrm & 0x07;
        if (rex & 0x01) inst.reg += 8;
        inst.imm = *(int32_t *)&code[pos];
        pos += 4;
    }
    // 29 /r : SUB r/m64, r64
    else if (opcode == 0x29) {
        inst.type = INST_SUB_REG;
        uint8_t modrm = code[pos];
        pos++;
        inst.reg = (modrm >> 3) & 0x07;
        inst.reg2 = modrm & 0x07;
        if (rex & 0x04) inst.reg += 8;
        if (rex & 0x01) inst.reg2 += 8;
    }
    // 01 /r : ADD r/m64, r64
    else if (opcode == 0x01) {
        inst.type = INST_ADD_REG;
        uint8_t modrm = code[pos];
        pos++;
        inst.reg = (modrm >> 3) & 0x07;
        inst.reg2 = modrm & 0x07;
        if (rex & 0x04) inst.reg += 8;
        if (rex & 0x01) inst.reg2 += 8;
    }
    // 85 /r : TEST r/m64, r64
    else if (opcode == 0x85) {
        inst.type = INST_TEST;
        uint8_t modrm = code[pos];
        pos++;
        inst.reg = modrm & 0x07;
        inst.reg2 = (modrm >> 3) & 0x07;
        if (rex & 0x01) inst.reg += 8;
        if (rex & 0x04) inst.reg2 += 8;
    }
    // 39 /r : CMP r/m64, r64
    else if (opcode == 0x39) {
        inst.type = INST_CMP_REG;
        uint8_t modrm = code[pos];
        pos++;
        inst.reg = (modrm >> 3) & 0x07;
        inst.reg2 = modrm & 0x07;
        if (rex & 0x04) inst.reg += 8;
        if (rex & 0x01) inst.reg2 += 8;
    }
    // 8B /r : MOV r64, r/m64
    else if (opcode == 0x8B) {
        inst.type = INST_MOV_LOAD;
        uint8_t modrm = code[pos];
        pos++;
        uint8_t mod = (modrm >> 6) & 0x03;
        uint8_t reg_f = (modrm >> 3) & 0x07;
        uint8_t rm_f = modrm & 0x07;
        
        inst.reg = reg_f;
        if (rex & 0x04) inst.reg += 8;
        
        if (mod == 3) {
            inst.type = INST_MOV_REG;
            if (rex & 0x01) rm_f += 8;
            inst.reg2 = rm_f;
        } else {
            uint8_t base = rm_f;
            if (rm_f == 4) {
                uint8_t sib = code[pos];
                pos++;
                base = sib & 0x07;
                if (rex & 0x01) base += 8;
            } else {
                if (rex & 0x01) base += 8;
            }
            inst.reg2 = base;
            
            if (mod == 1) {
                inst.imm = (int8_t)code[pos];
                pos++;
            } else if (mod == 2 || (mod == 0 && rm_f == 5)) {
                inst.imm = *(int32_t *)&code[pos];
                pos += 4;
            }
        }
    }
    // 90 : NOP (1-byte)
    else if (opcode == 0x90) {
        inst.type = INST_NOP;
        inst.size = 1;
    }
    // 0F 1F /0 : NOP (multi-byte)
    else if (opcode == 0x0F && code[pos] == 0x1F) {
        inst.type = INST_NOP_EXTENDED;
        pos++;
        uint8_t modrm = code[pos];
        pos++;
        if ((modrm & 0xC0) != 0xC0) {
            uint8_t rm = modrm & 0x07;
            if (rm == 4) pos++; // SIB
            if ((modrm & 0xC0) == 0x40) pos++; // disp8
            else if ((modrm & 0xC0) == 0x80) pos += 4; // disp32
        }
    }
    // FF /4 : JMP r/m64 (indirecto)
    else if (opcode == 0xFF && (code[pos] & 0x38) == 0x20) {
        inst.type = INST_JMP_INDIRECT;
        uint8_t modrm = code[pos];
        pos++;
        uint8_t mod = (modrm >> 6) & 0x03;
        uint8_t rm = modrm & 0x07;
        
        if (mod == 3) {
            // Salto indirecto a registro
            inst.reg = rm;
            if (rex & 0x01) inst.reg += 8;
        } else {
            // Salto indirecto con direccionamiento
            inst.reg = rm;
            if (rex & 0x01) inst.reg += 8;
            
            if (rm == 4) { // SIB
                uint8_t sib = code[pos];
                pos++;
                inst.reg2 = sib & 0x07;
                if (rex & 0x01) inst.reg2 += 8;
            }
            
            if (mod == 1) {
                inst.imm = (int8_t)code[pos];
                pos++;
            } else if (mod == 2) {
                inst.imm = *(int32_t *)&code[pos];
                pos += 4;
            } else if (mod == 0 && rm == 5) { // RIP-relativo
                inst.imm = rip + pos + 4 + *(int32_t *)&code[pos];
                pos += 4;
            }
        }
    }
    // 68 id : PUSH imm32
    else if (opcode == 0x68) {
        inst.type = INST_PUSH_IMM;
        inst.imm = *(int32_t *)&code[pos];
        pos += 4;
    }
    // 3C ib : CMP AL, imm8
    else if (opcode == 0x3C) {
        inst.type = INST_CMP_AL_IMM;
        inst.reg = 0; // AL es el registro 0 (RAX)
        inst.imm = (uint8_t)code[pos];
        pos++;
    }
    // B0-B7 ib : MOV r8, imm8 (movimiento de 8 bits a registros de 8 bits)
    else if (opcode >= 0xB0 && opcode <= 0xB7) {
        inst.type = INST_MOV_AL_IMM;
        inst.reg = opcode - 0xB0;
        if (rex & 0x01) inst.reg += 8;
        inst.imm = (uint8_t)code[pos];
        pos++;
    }
    // 80 /7 ib : CMP r/m8, imm8
    else if (opcode == 0x80 && (code[pos] & 0x38) == 0x38) {
        inst.type = INST_CMP_BYTE_IMM;
        uint8_t modrm = code[pos];
        pos++;
        inst.reg = modrm & 0x07;
        if (rex & 0x01) inst.reg += 8;
        inst.imm = (uint8_t)code[pos];
        pos++;
    }
    else {
        inst.type = INST_UNKNOWN;
    }

    inst.size = pos;
    return inst;
}
