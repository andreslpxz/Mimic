#ifndef GLOBALS_H
#define GLOBALS_H

#include <stdint.h>

/**
 * G_MEM_BASE: Dirección base segura para mapear memoria en sistemas restringidos
 * 
 * En Linux normal: 0x0 (pero algunos sistemas lo prohíben)
 * En Android/Termux: 0x10000000 (16 MB de offset seguro)
 * 
 * El ELF dirá que el código está en 0x400000, 0x51a8a0, etc.
 * Nosotros lo mapearemos en G_MEM_BASE + 0x400000, G_MEM_BASE + 0x51a8a0, etc.
 */
#define G_MEM_BASE 0x10000000UL

/**
 * Rango válido de memoria virtual en el emulador:
 * [G_MEM_BASE, G_MEM_BASE + 0x1000000)
 */
#define G_MEM_END (G_MEM_BASE + 0x1000000UL)

/**
 * Traducir una dirección virtual del ELF a dirección real del host
 * 
 * Ejemplo:
 *   - ELF dice: código en 0x400000
 *   - Host mapea: G_MEM_BASE + 0x400000
 *   - Esta función suma G_MEM_BASE automáticamente
 */
static inline uint64_t translate_addr(uint64_t elf_addr) {
    return G_MEM_BASE + elf_addr;
}

/**
 * Verificar si una dirección del ELF es válida dentro de nuestro rango
 * 
 * Rango válido del ELF: [0x0, 0x1000000)
 */
static inline int is_valid_elf_addr(uint64_t elf_addr) {
    return elf_addr < 0x1000000;
}

/**
 * Verificar si una dirección del host (ya traducida) es válida
 * 
 * Rango válido del host: [G_MEM_BASE, G_MEM_END)
 */
static inline int is_valid_host_addr(uint64_t host_addr) {
    return host_addr >= G_MEM_BASE && host_addr < G_MEM_END;
}

#endif // GLOBALS_H
