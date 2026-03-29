# Mimic v8 - ESTRICTO

## Cambios principales respecto a v7

### 1. **Forzar Mapeo Total** (loader.c)
   - ✅ Mapeo gigante de **16MB (0x1000000)** desde la dirección virtual **0**
   - ✅ **Elimina WARNs** en direcciones bajas como 0x3997d2
   - ✅ Carga **TODO el contenido del archivo ELF** respetando offsets
   - ✅ Asegura que cabeceras ELF sean accesibles

### 2. **Verificar CALL rel32** (executor.c)
   - ✅ Trata offsets como **int32_t con signo** (no uint32_t)
   - ✅ Permite saltos hacia **atrás** correctamente
   - ✅ Evita errores como "CALL a 0x4849a8fc" (cálculo incorrecto)
   - ✅ **ERROR CRÍTICO** si salto es inválido (exit(1))

### 3. **Safe Fetch** (executor.c)
   - ✅ Verifica que **RIP esté dentro de 0x0-0x1000000** antes de leer instrucción
   - ✅ Si RIP es inválido: **ERROR CRÍTICO + dump + exit(1)**
   - ✅ Previene segmentation faults

### 4. **Rango de código actualizado** (main.c)
   - ✅ Rango válido: **code_start = vaddr_offset**
   - ✅ Rango válido: **code_end = vaddr_offset + 0x1000000**

## Compilación

```bash
make clean
make
```

## Uso

```bash
./mimic <binario_elf_64bits>
```

## Log esperado (v8)

```
[Mimic] Tamaño del archivo: 0x... bytes
[Mimic] Rango de direcciones virtuales: 0x... - 0x...
[Mimic] Detectadas direcciones bajas: mapeo desde 0x0 (sin offset)
[Mimic] Mapeando rango gigante 0x0 - 0x1000000 (tamaño 0x1000000)
[Mimic] Mapeado rango gigante 0x0-0x1000000 exitosamente
[Mimic] Cargando archivo en offset 0x... (tamaño 0x...)
[Mimic] Contenido del archivo cargado exitosamente
[Mimic] ELF cargado: Entry Point @ 0x..., Stack @ 0x...
[Mimic] Rango válido de código: 0x... - 0x...
[Mimic] Empezando interpretación...
```

## Diferencias clave de v7 → v8

| Aspecto | v7 | v8 |
|---------|----|----|
| Mapeo de memoria | Variable (según ELF) | Fijo 16MB desde 0 |
| Direcciones bajas | ❌ WARN (ignoradas) | ✅ Accesibles |
| CALL rel32 | uint32_t (error en negativos) | int32_t (correcto) |
| Validación RIP | Ninguna | ✅ STRICT (Safe Fetch) |
| Saltos inválidos | WARN + continúa | ❌ ERROR + exit(1) |

## Troubleshooting

### "WARN: Lectura de dirección inválida 0x3997d2"
→ **RESUELTO en v8**: Mapeo gigante cubre todas las direcciones

### "CALL a 0x4849a8fc" → segfault
→ **RESUELTO en v8**: int32_t correctamente maneja offsets negativos

### Segmentation fault en RIP
→ **RESUELTO en v8**: Safe Fetch valida RIP antes de leer código
