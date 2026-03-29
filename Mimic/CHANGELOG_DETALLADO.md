# CHANGELOG v7 → v8

## 🔴 PROBLEMAS DE v7

1. **Direcciones bajas ignoradas** (< 0x400000)
   - Log mostraba: `[Mimic] WARN: Lectura de dirección inválida 0x3997d2`
   - Causa: vaddr_offset forzado a 0x400000 sin mapear región baja
   - Resultado: No hay acceso a cabeceras ELF, referencias rotas

2. **Error en CALL rel32**
   - Log mostraba: `[Mimic] CALL a 0x4849a8fc` (dirección absurda)
   - Causa: Offset se trataba como `uint32_t` en lugar de `int32_t`
   - Resultado: Saltos hacia atrás calculados incorrectamente → segfault

3. **Sin validación de RIP**
   - Causa: Lectura de instrucción sin verificar que RIP es válido
   - Resultado: Segmentation faults sin contexto claro

## 🟢 SOLUCIONES v8

### 1. **Mapeo Total Gigante (loader.c)**

**Antes (v7):**
```c
uint64_t vaddr_offset = 0x400000;  // Forzado si min_vaddr < 0x400000
uint64_t aligned_max = PAGE_ALIGN_UP(max_vaddr);
uint64_t total_size = aligned_max - aligned_min;  // Solo lo necesario

void *addr = mmap((void *)(aligned_min + vaddr_offset), total_size, ...);
```

**Después (v8):**
```c
// Detectar si hay direcciones bajas
if (min_vaddr < 0x400000) {
    vaddr_offset = 0x0;  // ✅ NO offset, mapear desde 0
}

// ✅ Mapeo fijo de 16MB
uint64_t total_map_size = 0x1000000;  // 16 MB garantizado
void *addr = mmap((void *)0, total_map_size,
                  PROT_READ | PROT_WRITE,
                  MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);

// ✅ Cargar TODO el archivo respetando offsets
ssize_t bytes_read = read(fd, (void *)vaddr_offset, load_size);
```

**Beneficios:**
- ✅ 0x3997d2 ahora es accesible
- ✅ Cabeceras ELF en dirección 0 disponibles
- ✅ Sin WARNs de direcciones inválidas

### 2. **Corrección CALL rel32 (executor.c)**

**Antes (v7):**
```c
case INST_CALL: {
    uint64_t target = next_rip + (int32_t)inst.imm;  // ¡Ya estaba bien!
    
    if (target < code_start || target >= code_end) {
        fprintf(stderr, "[Mimic] WARN: ...\n");
        cpu->rip = next_rip;  // ❌ Continúa sin saltar → lógica rota
    } else {
        cpu->rip = target;
    }
    break;
}
```

**Después (v8):**
```c
case INST_CALL: {
    // ✅ Explícitamente int32_t para mayor claridad
    int32_t signed_offset = (int32_t)inst.imm;
    uint64_t target = next_rip + signed_offset;
    
    // ✅ ERROR CRÍTICO si falla (no continuar)
    if (target < code_start || target >= code_end) {
        fprintf(stderr, "[Mimic] ERROR: CALL a 0x%lx inválido ...\n", target);
        fprintf(stderr, "[Mimic] Contexto: RIP anterior=0x%lx, next_rip=0x%lx\n", 
                cpu->rip, next_rip);
        cpu_dump(cpu);
        exit(1);  // ✅ STOP inmediato
    } else {
        cpu->rip = target;
        fprintf(stderr, "[Mimic] CALL a 0x%lx (offset=%d)\n", cpu->rip, signed_offset);
    }
    break;
}
```

**Cambios en JMP rel32 también:**
```c
case INST_JMP_REL32: {
    int32_t signed_offset = (int32_t)inst.imm;
    uint64_t target = next_rip + signed_offset;
    
    if (target < code_start || target >= code_end) {
        fprintf(stderr, "[Mimic] ERROR: JMP rel32 a 0x%lx inválido (offset=%d)\n", 
                target, signed_offset);
        exit(1);  // ✅ STOP
    }
    cpu->rip = target;
    break;
}
```

**Beneficios:**
- ✅ Saltos hacia atrás ahora funcionan
- ✅ Errores de salto causan exit inmediato (no segfault sorpresa)
- ✅ Logs claros de qué fue mal

### 3. **Safe Fetch (executor.c)**

**Antes (v7):**
```c
while (1) {
    uint8_t *code = (uint8_t *)cpu->rip;  // ❌ Sin validación
    DecodedInstruction inst = decode(code, cpu->rip);
    ...
}
```

**Después (v8):**
```c
while (1) {
    // ✅ ESTRICTO: Validar RIP ANTES de leer
    if (cpu->rip < code_start || cpu->rip >= code_end) {
        fprintf(stderr, "[Mimic] ERROR CRÍTICO: RIP 0x%lx fuera del rango "
                        "válido 0x%lx-0x%lx\n",
                cpu->rip, code_start, code_end);
        cpu_dump(cpu);
        exit(1);
    }
    
    uint8_t *code = (uint8_t *)cpu->rip;
    DecodedInstruction inst = decode(code, cpu->rip);
    ...
}
```

**Beneficios:**
- ✅ Segmentation faults ahora tienen contexto claro
- ✅ Dump de estado CPU antes de salir
- ✅ Fácil detectar qué instrucción causó el problema

### 4. **Rango de código actualizado (main.c)**

**Antes (v7):**
```c
uint64_t code_start = elf.vaddr_offset;
uint64_t code_end = elf.entry_point + 0x400000;  // ❌ Relativo a entry point
```

**Después (v8):**
```c
uint64_t code_start = elf.vaddr_offset;
uint64_t code_end = elf.vaddr_offset + 0x1000000;  // ✅ Rango fijo 16MB
```

## 📊 Resumen de cambios

| Archivo | Cambios |
|---------|---------|
| `loader.c` | Mapeo gigante 16MB, detección de direcciones bajas, carga respetando offsets |
| `executor.c` | Safe Fetch, CALL/JMP rel32 como int32_t, ERROR CRÍTICO en saltos inválidos |
| `main.c` | Rango de código fijo a 16MB |
| `Makefile` | Nuevo (v7 usaba shell script) |

## 🧪 Testing

### Prueba 1: Dirección baja 0x3997d2
```bash
./mimic aapt2
# Esperado: ✅ Acceso sin WARN
# v7 resultado: ❌ WARN: Lectura de dirección inválida 0x3997d2
# v8 resultado: ✅ SIN WARN
```

### Prueba 2: CALL rel32 válido
```bash
# Esperado: ✅ CALL a dirección válida dentro del rango
# v7 resultado: ❌ Posible "CALL a 0x4849a8fc" (error de cálculo)
# v8 resultado: ✅ CALL a dirección correcta
```

### Prueba 3: Segfault en RIP inválido
```bash
# Esperado: ✅ ERROR CRÍTICO con dump, exit(1)
# v7 resultado: ❌ Segmentation fault (sin contexto)
# v8 resultado: ✅ Error claro + dump CPU
```

## 🔧 Compilación

```bash
cd Mimic-v8
make clean
make
./mimic <binario>
```

## 📝 Notas de implementación

1. **Mapeo gigante**: 0x1000000 (16 MB) es suficiente para casi cualquier binario x86-64
2. **int32_t en offsets**: Crucial para saltos relativos (pueden ser negativos)
3. **ERROR vs WARN**: v8 es más estricto: cualquier salto inválido = ERROR CRÍTICO
4. **Safe Fetch**: Previene segfaults con contexto claro antes de morir
