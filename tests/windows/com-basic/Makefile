# Makefile pour com-basic — cross-compile avec mingw-w64 (C++ + ole32).

CXX ?= x86_64-w64-mingw32-g++
TARGET = com-basic.exe
SRC = source.cpp
LIBS = -lole32 -loleaut32

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) -O2 -o $@ $(SRC) $(LIBS)

clean:
	rm -f $(TARGET)
