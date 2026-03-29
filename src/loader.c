#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <elf.h>
#include <string.h>
#include "../include/loader.h"

#define PAGE_SIZE 4096
#define PAGE_ALIGN(addr) ((addr) & ~(PAGE_SIZE - 1))
#define PAGE_ALIGN_UP(addr) (((addr) + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1))

LoadedELF load_elf(const char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        perror("[Mimic] Error al abrir el binario");
        exit(1);
    }

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

    for (int i = 0; i < header.e_phnum; i++) {
        if (phdrs[i].p_type == PT_LOAD) {
            uint64_t vaddr = phdrs[i].p_vaddr;
            uint64_t memsz = phdrs[i].p_memsz;
            uint64_t filesz = phdrs[i].p_filesz;
            uint64_t offset = phdrs[i].p_offset;

            uint64_t aligned_vaddr = PAGE_ALIGN(vaddr);
            uint64_t aligned_memsz = PAGE_ALIGN_UP(vaddr + memsz) - aligned_vaddr;

            void *addr = mmap((void *)aligned_vaddr, aligned_memsz,
                              PROT_READ | PROT_WRITE | PROT_EXEC,
                              MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);

            if (addr == MAP_FAILED) {
                perror("[Mimic] Error al mapear segmento");
                exit(1);
            }

            lseek(fd, offset, SEEK_SET);
            if (read(fd, (void *)vaddr, filesz) != (ssize_t)filesz) {
                perror("[Mimic] Error al leer segmento");
                exit(1);
            }

            if (memsz > filesz) {
                memset((void *)(vaddr + filesz), 0, memsz - filesz);
            }
        }
    }

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
    loaded.entry_point = header.e_entry;
    loaded.stack_base = (uint64_t)stack_addr;
    loaded.stack_ptr = (uint64_t)stack_addr + stack_size;

    // Push initial stack contents (argc, argv, envp) - simplified for now
    loaded.stack_ptr -= 8;
    *(uint64_t *)(loaded.stack_ptr) = 0; // argc = 0

    printf("[Mimic] ELF cargado: Entry Point @ 0x%lx, Stack @ 0x%lx\n", loaded.entry_point, loaded.stack_ptr);
    return loaded;
}
