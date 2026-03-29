CC = gcc
CFLAGS = -Wall -Iinclude -O2
SOURCES = src/main.c src/loader.c src/cpu.c src/decoder.c src/executor.c src/syscall.c
OBJECTS = $(SOURCES:.c=.o)
TARGET = mimic

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

rebuild: clean all

.PHONY: all clean rebuild
