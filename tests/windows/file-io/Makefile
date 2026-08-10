# Makefile pour file-io — cross-compile avec mingw-w64.

CC ?= x86_64-w64-mingw32-gcc
TARGET = file-io.exe
SRC = source.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -O2 -o $@ $(SRC)

clean:
	rm -f $(TARGET) test.txt
