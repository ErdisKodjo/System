# Makefile pour registry-access — cross-compile avec mingw-w64 (lié à advapi32).

CC ?= x86_64-w64-mingw32-gcc
TARGET = registry-access.exe
SRC = source.c
LIBS = -ladvapi32

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -O2 -o $@ $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)
