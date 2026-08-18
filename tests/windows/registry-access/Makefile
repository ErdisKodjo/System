# Makefile pour registry-access — cross-compile avec mingw-w64 (lié à advapi32).
#
# Fallback host gcc + win32-stubs.h si mingw n'est pas installé.

MINGW_CC ?= x86_64-w64-mingw32-gcc
HOST_CC  ?= gcc
TARGET   = registry-access.exe
SRC      = source.c
WIN_DIR  = ..
LIBS     = -ladvapi32

CROSS_OK := $(shell command -v $(MINGW_CC) 2>/dev/null)

.PHONY: all clean syntax-check

all: $(if $(CROSS_OK),$(TARGET),syntax-check)

$(TARGET): $(SRC) $(WIN_DIR)/win32-stubs.h
	$(MINGW_CC) -O2 -I$(WIN_DIR) -o $@ $(SRC) $(LIBS)

syntax-check: $(SRC) $(WIN_DIR)/win32-stubs.h
	$(HOST_CC) -fsyntax-only -Wall -I$(WIN_DIR) $(SRC)
	@touch $@

clean:
	rm -f $(TARGET) syntax-check
