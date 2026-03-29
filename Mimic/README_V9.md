# Mimic v9 - COMPATIBLE CON ANDROID/TERMUX

## 🎯 Problema resuelto

**v8 fallaba en Android/Termux:**
```
[Mimic] Error al mapear rango gigante 0x0-0x1000000: Operation not permitted
```

**Causa:** Android/Termux prohíbe mapear memoria en la dirección 0x0

## ✅ Solución v9: Offset Base Seguro

### 1. **Base Virtual Global: G_MEM_BASE = 0x10000000**

Definida en `include/globals.h`:
```c
#define G_MEM_BASE 0x10000000UL  // 256 MB de dirección segura
#define G_MEM_END (G_MEM_BASE + 0x1000000UL)  // Hasta 272 MB
```

**Flujo de traducción de direcciones:**
```
ELF dice:     "Código en 0x400000"
v9 mapea:     "Contenido en G_MEM_BASE + 0x400000" = 0x10400000
Host accede:  0x10400000 (sin problemas en Android)
```

### 2. **Traductor de Direcciones Automático (globals.h)**

```c
static inline uint64_t translate_addr(uint64_t elf_addr) {
    return G_MEM_BASE + elf_addr;  // Suma automática
}

static inline int is_valid_elf_addr(uint64_t elf_addr) {
    return elf_addr < 0x1000000;   // Válido si < 16MB
}

static inline int is_valid_host_addr(uint64_t host_addr) {
    return host_addr >= G_MEM_BASE && host_addr < G_MEM_END;
}
```

### 3. **Loader Actualizado (loader.c v9)**

```c
// NUNCA intenta mapear en 0x0 (Android lo prohíbe)
void *addr = mmap((void *)G_MEM_BASE, 0x1000000,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);

// Cargar TODO el archivo en G_MEM_BASE + 0
read(fd, (void *)G_MEM_BASE, load_size);

// Entry point traducido automáticamente
loaded.entry_point = G_MEM_BASE + header.e_entry;
loaded.vaddr_offset = G_MEM_BASE;
```

### 4. **Safe Fetch Adaptado (executor.c v9)**

```c
// Validación usa G_MEM_BASE y G_MEM_END directamente
if (cpu->rip < G_MEM_BASE || cpu->rip >= G_MEM_END) {
    fprintf(stderr, "[Mimic] ERROR CRÍTICO: RIP 0x%lx fuera de rango\n", cpu->rip);
    exit(1);
}
```

## 📊 Comparación v8 vs v9

| Aspecto | v8 | v9 |
|---------|----|----|
| Mapeo en 0x0 | ✅ Linux | ❌ Android falla |
| Base segura | N/A | 0x10000000 ✅ |
| Traducción | Suma manual | Automática |
| Safe Fetch | Verificación local | G_MEM_BASE/END |
| Compatibilidad | Linux | Linux + Android ✅ |

## 🔧 Compilación

```bash
cd Mimic-v9
make clean
make
```

## 🚀 Uso

```bash
./mimic <binario_elf_64bits>
```

### Log esperado (v9):

```
[Mimic] Tamaño del archivo: 0x5f9630 bytes
[Mimic] Rango de direcciones virtuales del ELF: 0x0 - 0x4baae0
[Mimic] G_MEM_BASE: 0x10000000 (dirección base segura)
[Mimic] Mapeando 16MB: 0x10000000 - 0x11000000
[Mimic] Mapeo exitoso en G_MEM_BASE=0x10000000
[Mimic] Cargando archivo (0x5f9630 bytes) en offset 0x0 dentro de G_MEM_BASE
[Mimic] Contenido del archivo cargado exitosamente en G_MEM_BASE
[Mimic] ELF cargado: Entry Point @ 0x10000000 (ELF:0x0 + G_MEM_BASE:0x10000000)
[Mimic] Stack @ 0x...
[Mimic] Rango válido de memoria: 0x10000000 - 0x11000000
[Mimic] Rango válido de código: 0x10000000 - 0x11000000
[Mimic] Empezando interpretación...
```

## 📝 Detalles técnicos

### Traducción de direcciones (manual si fuera necesario):
- **Dirección ELF:** 0x3997d2
- **Dirección host:** 0x10000000 + 0x3997d2 = 0x103997d2 ✅

### Flujo completo:
1. **loader.c**: Mapea en G_MEM_BASE, carga archivo, suma G_MEM_BASE al entry point
2. **main.c**: Pasa G_MEM_BASE y G_MEM_END como rango válido
3. **executor.c**: Safe Fetch valida RIP contra G_MEM_BASE/G_MEM_END
4. **globals.h**: Proporciona constantes y funciones de traducción

## ✨ Ventajas v9

✅ **Compatible con Android/Termux**  
✅ **Sin "Operation not permitted"**  
✅ **Traducción automática de direcciones**  
✅ **Safe Fetch adaptado**  
✅ **Retiene todas las mejoras de v8** (CALL rel32, direcciones bajas, etc.)

## 🔍 Troubleshooting

### "Operation not permitted" en mapeo
→ **RESUELTO en v9**: Se usa G_MEM_BASE (0x10000000) en lugar de 0x0

### Direcciones de logs muestran 0x10...
→ **NORMAL en v9**: Son direcciones reales del host (G_MEM_BASE + offset ELF)

### RIP fuera de rango 0x10000000-0x11000000
→ **ERROR CRÍTICO**: Ocurrió un salto inválido (ver error anterior)
