# Makefile pour hello-elf — compile en ELF natif avec gcc ou clang.

CC ?= gcc
TARGET = hello-elf
SRC = source.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -O2 -o $@ $(SRC)

clean:
	rm -f $(TARGET)
