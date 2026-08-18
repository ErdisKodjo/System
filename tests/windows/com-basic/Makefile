# Makefile pour com-basic — cross-compile avec mingw-w64 (C++ + ole32).
#
# Fallback host g++ + win32-stubs.h si mingw n'est pas installé.

MINGW_CXX ?= x86_64-w64-mingw32-g++
HOST_CXX  ?= g++
TARGET    = com-basic.exe
SRC       = source.cpp
WIN_DIR   = ..
LIBS      = -lole32 -loleaut32

CROSS_OK := $(shell command -v $(MINGW_CXX) 2>/dev/null)

.PHONY: all clean syntax-check

all: $(if $(CROSS_OK),$(TARGET),syntax-check)

$(TARGET): $(SRC) $(WIN_DIR)/win32-stubs.h
	$(MINGW_CXX) -O2 -I$(WIN_DIR) -o $@ $(SRC) $(LIBS)

syntax-check: $(SRC) $(WIN_DIR)/win32-stubs.h
	$(HOST_CXX) -fsyntax-only -Wall -std=c++17 -I$(WIN_DIR) $(SRC)
	@touch $@

clean:
	rm -f $(TARGET) syntax-check
