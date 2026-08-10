# Makefile pour hello-world — cross-compile avec mingw-w64.
#
# Si x86_64-w64-mingw32-gcc n'est pas installé, le build échoue
# silencieusement et le harness marquera le test SKIP (binaire manquant).

CC ?= x86_64-w64-mingw32-gcc
TARGET = hello.exe
SRC = source.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -O2 -o $@ $(SRC)

clean:
	rm -f $(TARGET)
