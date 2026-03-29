#!/data/data/com.termux/files/usr/bin/bash

echo "[*] Compilando Mimic..."

clang -O2 -Iinclude \
src/main.c \
src/decoder.c \
src/translator.c \
src/executor.c \
-o mimic

echo "[+] Build completado: ./mimic"
