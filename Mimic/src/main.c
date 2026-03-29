#include <stdio.h>
#include <stdlib.h>
#include "../include/loader.h"
#include "../include/cpu.h"
#include "../include/executor.h"
#include "../include/globals.h"

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Uso: %s <binario_x86_64>\n", argv[0]);
        return 1;
    }

    printf("[Mimic] Cargando %s...\n", argv[1]);

    LoadedELF elf = load_elf(argv[1]);

    CPUState cpu;
    cpu_init(&cpu, elf.entry_point, elf.stack_ptr);

    printf("[Mimic] Empezando interpretación...\n");
    
    // V9: Rango de código es exactamente G_MEM_BASE a G_MEM_END
    uint64_t code_start = G_MEM_BASE;
    uint64_t code_end = G_MEM_END;
    
    printf("[Mimic] Rango válido de código: 0x%lx - 0x%lx\n", code_start, code_end);
    
    execute(&cpu, elf.stack_base, elf.stack_ptr + 0x800000, code_start, code_end);

    return 0;
}


