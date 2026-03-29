#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <elf.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "../include/loader.h"
#include "../include/globals.h"

#define PAGE_SIZE 4096
#define PAGE_ALIGN(addr) ((addr) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

LoadedELF load_elf(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("[Mimic] Error al abrir el binario");
        exit(1);
    }

    // Obtener tamaño del archivo
    off_t file_size = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    
    if (file_size <= 0) {
        fprintf(stderr, "[Mimic] Error: archivo vacío o inválido\n");
        exit(1);
    }
    
    printf("[Mimic] Tamaño del archivo: 0x%lx bytes\n", (uint64_t)file_size);

    Elf64_Ehdr header;
    if (read(fd, &header, sizeof(header)) != sizeof(header)) {
        perror("[Mimic] Error al leer la cabecera ELF");
        exit(1);
    }

    if (memcmp(header.e_ident, ELFMAG, SELFMAG) != 0) {
        fprintf(stderr, "[Mimic] El archivo no es un ELF válido\n");
        exit(1);
    }

    if (header.e_ident[EI_CLASS] != ELFCLASS64) {
        fprintf(stderr, "[Mimic] Solo se soportan binarios de 64 bits\n");
        exit(1);
    }

    lseek(fd, header.e_phoff, SEEK_SET);
    Elf64_Phdr *phdrs = malloc(sizeof(Elf64_Phdr) * header.e_phnum);
    if (read(fd, phdrs, sizeof(Elf64_Phdr) * header.e_phnum) != (ssize_t)(sizeof(Elf64_Phdr) * header.e_phnum)) {
        perror("[Mimic] Error al leer los program headers");
        exit(1);
    }

    uint64_t min_vaddr = UINT64_MAX;
    uint64_t max_vaddr = 0;

    // 1. Encontrar el rango de direcciones virtuales del ELF
    for (int i = 0; i < header.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            uint64_t vaddr = phdrs[i].p_vaddr;
            uint64_t memsz = phdrs[i].p_memsz;
            
            if (vaddr < min_vaddr) min_vaddr = vaddr;
            if (vaddr + memsz > max_vaddr) max_vaddr = vaddr + memsz;
        }
    }

    printf("[Mimic] Rango de direcciones virtuales del ELF: 0x%lx - 0x%lx\n", min_vaddr, max_vaddr);

    // 2. V9: MAPEO CON OFFSET SEGURO
    // Nunca intentamos mapear en 0x0 (Android/Termux lo prohíbe)
    // En su lugar, usamos G_MEM_BASE como base segura
    
    printf("[Mimic] G_MEM_BASE: 0x%lx (dirección base segura)\n", G_MEM_BASE);
    printf("[Mimic] Mapeando 16MB: 0x%lx - 0x%lx\n", G_MEM_BASE, G_MEM_END);

    // 3. Mapear 16MB en G_MEM_BASE (dirección segura en Android/Termux)
    void *addr = mmap((void *)G_MEM_BASE, 0x1000000,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);

    if (addr == MAP_FAILED) {
        fprintf(stderr, "[Mimic] Error al mapear rango en 0x%lx: %s\n",
                G_MEM_BASE, strerror(errno));
        exit(1);
    }
    
    printf("[Mimic] Mapeo exitoso en G_MEM_BASE=0x%lx\n", G_MEM_BASE);

    // 4. Cargar TODO el contenido del archivo en G_MEM_BASE
    // El ELF dice: "código en 0x400000"
    // Nosotros cargamos: "contenido en G_MEM_BASE + 0x400000"
    lseek(fd, 0, SEEK_SET);
    
    uint64_t load_size = (file_size < 0x1000000) ? file_size : 0x1000000;
    
    printf("[Mimic] Cargando archivo (0x%lx bytes) en offset 0x0 dentro de G_MEM_BASE\n", load_size);
    
    // Leer desde inicio del archivo y escribir en G_MEM_BASE + 0
    ssize_t bytes_read = read(fd, (void *)G_MEM_BASE, load_size);
    if (bytes_read != (ssize_t)load_size) {
        fprintf(stderr, "[Mimic] ADVERTENCIA: Se leyeron 0x%lx bytes de 0x%lx esperados\n", 
                (uint64_t)bytes_read, load_size);
    } else {
        printf("[Mimic] Contenido del archivo cargado exitosamente en G_MEM_BASE\n");
    }

    // 5. Procesar segmentos PT_LOAD para rellenar bss
    for (int i = 0; i < header.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            uint64_t vaddr = phdrs[i].p_vaddr;
            uint64_t memsz = phdrs[i].p_memsz;
            uint64_t filesz = phdrs[i].p_filesz;

            // Rellenar con ceros lo que no está en el archivo pero sí en memoria (bss)
            if (memsz > filesz) {
                uint64_t host_bss_addr = G_MEM_BASE + vaddr + filesz;
                memset((void *)host_bss_addr, 0, memsz - filesz);
                printf("[Mimic] BSS rellenado en ELF:0x%lx (host:0x%lx, 0x%lx bytes)\n", 
                       vaddr + filesz, host_bss_addr, memsz - filesz);
            }

            printf("[Mimic] PT_LOAD ELF:0x%lx (mem: 0x%lx, archivo: 0x%lx)\n",
                   vaddr, memsz, filesz);
        }
    }

    // 6. Mapear la pila (en dirección independiente, no en G_MEM_BASE)
    uint64_t stack_size = 0x800000;
    void *stack_addr = mmap(NULL, stack_size, PROT_READ | PROT_WRITE,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack_addr == MAP_FAILED) {
        perror("[Mimic] Error al mapear la pila");
        exit(1);
    }

    free(phdrs);
    close(fd);

    LoadedELF loaded;
    // Entry point: el ELF dice 0x400000 + algo, nosotros lo traducimos a G_MEM_BASE + 0x400000 + algo
    loaded.entry_point = G_MEM_BASE + header.e_entry;
    loaded.vaddr_offset = G_MEM_BASE;  // ✅ Guardamos G_MEM_BASE como offset
    
    // Calcular el TOP del stack
    uintptr_t stack_top = (uintptr_t)stack_addr + stack_size;
    
    // Alineación a 16 bytes
    uintptr_t aligned_stack = stack_top & ~0xFULL;
    
    // Inicializar stack con argc
    aligned_stack -= 8;
    *(uint64_t *)aligned_stack = 1; // argc = 1
    
    loaded.stack_base = (uint64_t)stack_addr;
    loaded.stack_ptr = (uint64_t)aligned_stack;

    printf("[Mimic] ELF cargado: Entry Point @ 0x%lx (ELF:0x%lx + G_MEM_BASE:0x%lx)\n", 
           loaded.entry_point, header.e_entry, G_MEM_BASE);
    printf("[Mimic] Stack @ 0x%lx\n", loaded.stack_ptr);
    printf("[Mimic] Rango válido de memoria: 0x%lx - 0x%lx\n", G_MEM_BASE, G_MEM_END);
    
    return loaded;
}
