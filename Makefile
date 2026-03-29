CC = gcc
CFLAGS = -Wall -Iinclude
SRC = src/main.c src/loader.c src/cpu.c src/decoder.c src/executor.c src/syscall.c
OBJ = $(SRC:.c=.o)
TARGET = mimic

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(OBJ) -o $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(TARGET)
