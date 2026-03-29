CC=clang
CFLAGS=-O2 -Iinclude

SRC=src/main.c src/decoder.c src/translator.c src/executor.c

all:
	$(CC) $(CFLAGS) $(SRC) -o mimic

clean:
	rm -f mimic
