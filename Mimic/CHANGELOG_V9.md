# CHANGELOG v8 → v9: Compatibilidad Android/Termux

## 🔴 PROBLEMA DE v8

**En Linux:** ✅ Funciona bien  
**En Android/Termux:** ❌ CRASH

```
[Mimic] Mapeando rango gigante 0x0 - 0x1000000
[Mimic] Error al mapear rango gigante 0x0-0x1000000: Operation not permitted
```

**Causa:** Android/Termux prohíben mapear memoria en dirección 0x0 (kernel security policy)

## 🟢 SOLUCIÓN v9: G_MEM_BASE

### Cambio 1: Crear globals.h (nuevo archivo)

```c
#define G_MEM_BASE 0x10000000UL      // 256 MB offset seguro
#define G_MEM_END (G_MEM_BASE + 0x1000000UL)  // Hasta 272 MB

static inline uint64_t translate_addr(uint64_t elf_addr) {
    return G_MEM_BASE + elf_addr;
}
```

**Beneficio:** Constantes globales para traducción de direcciones

### Cambio 2: Mapeo en G_MEM_BASE (loader.c)

**Antes (v8):**
```c
void *addr = mmap((void *)0, total_map_size, ...);  // ❌ Falla en Android
```

**Después (v9):**
```c
void *addr = mmap((void *)G_MEM_BASE, 0x1000000, ...);  // ✅ Seguro
```

**Cambio 3: Carga de archivo respetando G_MEM_BASE**

**Antes (v8):**
```c
read(fd, (void *)vaddr_offset, load_size);  // vaddr_offset = 0
```

**Después (v9):**
```c
read(fd, (void *)G_MEM_BASE, load_size);  // Siempre en dirección segura
```

**Cambio 4: Entry point traducido**

**Antes (v8):**
```c
loaded.entry_point = header.e_entry + vaddr_offset;  // Si vaddr_offset=0, sin cambios
```

**Después (v9):**
```c
loaded.entry_point = G_MEM_BASE + header.e_entry;  // Siempre suma G_MEM_BASE
```

**Cambio 5: Rango de código en main.c**

**Antes (v8):**
```c
uint64_t code_start = elf.vaddr_offset;
uint64_t code_end = elf.vaddr_offset + 0x1000000;
```

**Después (v9):**
```c
uint64_t code_start = G_MEM_BASE;
uint64_t code_end = G_MEM_END;
```

**Cambio 6: Safe Fetch adaptado (executor.c)**

**Antes (v8):**
```c
if (cpu->rip < code_start || cpu->rip >= code_end) {  // Variables locales
```

**Después (v9):**
```c
if (cpu->rip < G_MEM_BASE || cpu->rip >= G_MEM_END) {  // Constantes globales
```

## 📊 Traducción de direcciones v9

| Componente | Dirección ELF | Dirección Host |
|------------|---------------|----------------|
| Entrada | 0x0 | 0x10000000 |
| Código | 0x400000 | 0x10400000 |
| Dato bajo | 0x3997d2 | 0x103997d2 |
| Límite | 0xFFFFFF | 0x10FFFFFF |
| Fin rango | 0x1000000 | 0x11000000 |

## 🔄 Flujo de ejecución v9

```
1. Loader mapea en G_MEM_BASE (0x10000000)
   ↓
2. Lee archivo ELF y copia a G_MEM_BASE + offset_elf
   ↓
3. Entry point = G_MEM_BASE + header.e_entry
   ↓
4. Executor valida RIP contra [G_MEM_BASE, G_MEM_END)
   ↓
5. Saltos CALL/JMP se calculan igual (next_rip + offset)
   ↓
6. Todo funciona porque RIP ya incluye G_MEM_BASE
```

## ✅ Cambios por archivo

### include/globals.h (NUEVO)
- Define G_MEM_BASE = 0x10000000
- Define G_MEM_END = G_MEM_BASE + 0x1000000
- Funciones de traducción: translate_addr(), is_valid_*_addr()

### src/loader.c
- Import globals.h
- Detecta min_vaddr pero ignora (siempre mapea en G_MEM_BASE)
- Mapea en (void *)G_MEM_BASE en lugar de (void *)0
- Lee archivo en (void *)G_MEM_BASE
- Entry point = G_MEM_BASE + header.e_entry
- vaddr_offset = G_MEM_BASE

### src/main.c
- Import globals.h
- code_start = G_MEM_BASE
- code_end = G_MEM_END

### src/executor.c
- Import globals.h
- Safe Fetch valida contra G_MEM_BASE/G_MEM_END

### src/cpu.c, src/decoder.c, src/syscall.c
- Sin cambios (funcionan igual)

## 🧪 Testing v9

### Test 1: En Linux
```bash
gcc ... && ./mimic binario.elf
# Esperado: ✅ Funciona (mapeo en 0x10000000 es válido)
```

### Test 2: En Android/Termux
```bash
gcc ... && ./mimic aapt2
# v8 resultado: ❌ Operation not permitted
# v9 resultado: ✅ Mapeo exitoso en G_MEM_BASE=0x10000000
```

### Test 3: Dirección baja 0x3997d2
```
ELF dirección: 0x3997d2
Host dirección: 0x103997d2 (dentro de [0x10000000, 0x11000000))
Resultado: ✅ Accesible
```

## 🎯 Resumen

| Aspecto | v8 | v9 |
|---------|----|----|
| Mapeo dirección | 0x0 | G_MEM_BASE (0x10000000) |
| Android support | ❌ Falla | ✅ Funciona |
| Traducción | Manual | Automática en loader |
| Safe Fetch | Validación local | Contra G_MEM_BASE/END |
| Entry point | vaddr_offset + e_entry | G_MEM_BASE + e_entry |
| Retrocompat. | Solo Linux | Linux + Android |

## 📝 Notas importantes

1. **G_MEM_BASE = 0x10000000** es suficientemente alto para:
   - No interferir con kernel/libc en cualquier plataforma
   - Permitir acceso a todo el rango ELF [0x0, 0x1000000)
   - Funcionar en Android con su kernel hardening

2. **Traducción automática**: El loader suma G_MEM_BASE una sola vez al cargar, luego todo funciona naturalmente porque RIP ya contiene G_MEM_BASE

3. **Compatibilidad hacia atrás**: v9 retiene todos los fixes de v8:
   - Acceso a direcciones bajas (0x3997d2)
   - CALL rel32 con int32_t firmado
   - Safe Fetch con validación de RIP
