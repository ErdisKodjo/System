# Makefile pour fork-exec — compile en ELF natif avec gcc ou clang.

CC ?= gcc
TARGET = fork-exec
SRC = source.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -O2 -o $@ $(SRC)

clean:
	rm -f $(TARGET)
