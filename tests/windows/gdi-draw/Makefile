# Makefile pour gdi-draw — cross-compile avec mingw-w64 (lié à gdi32).

CC ?= x86_64-w64-mingw32-gcc
TARGET = gdi-draw.exe
SRC = source.c
LIBS = -lgdi32 -luser32

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -O2 -o $@ $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)
