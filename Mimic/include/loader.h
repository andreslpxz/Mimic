#ifndef LOADER_H
#define LOADER_H

#include <stdint.h>
#include <elf.h>

typedef struct {
    uint64_t entry_point;
    uint64_t stack_base;
    uint64_t stack_ptr;
    uint64_t vaddr_offset;  // Offset aplicado a las direcciones virtuales
} LoadedELF;

LoadedELF load_elf(const char *filename);

#endif
