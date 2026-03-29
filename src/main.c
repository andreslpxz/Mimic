#include <stdio.h>
#include <stdlib.h>
#include "../include/loader.h"
#include "../include/cpu.h"
#include "../include/executor.h"

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
    execute(&cpu);

    return 0;
}
