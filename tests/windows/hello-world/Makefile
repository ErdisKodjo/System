# Makefile pour hello-world — cross-compile avec mingw-w64.
#
# Si x86_64-w64-mingw32-gcc n'est pas installé, on retombe sur `gcc` avec
# les stubs `tests/windows/win32-stubs.h` pour faire uniquement un
# -fsyntax-only (le harness marquera le test SKIP car le .exe manque).
#
# NB: on n'utilise pas `CC`/`CXX` (noms réservés par GNU Make pour ses
# règles implicites — `CC ?= ...` ne les surcharge pas) mais `MINGW_CC` /
# `HOST_CC`, surchargeables depuis la ligne de commande.

MINGW_CC ?= x86_64-w64-mingw32-gcc
HOST_CC  ?= gcc
TARGET   = hello.exe
SRC      = source.c
WIN_DIR  = ..

# Détection du cross-compiler mingw.
CROSS_OK := $(shell command -v $(MINGW_CC) 2>/dev/null)

.PHONY: all clean syntax-check

# Si mingw est dispo → on build le .exe ; sinon → on ne fait qu'un
# syntax-check stamp (avec gcc + stubs) pour ne pas casser le `make all`.
all: $(if $(CROSS_OK),$(TARGET),syntax-check)

$(TARGET): $(SRC) $(WIN_DIR)/win32-stubs.h
	$(MINGW_CC) -O2 -I$(WIN_DIR) -o $@ $(SRC)

syntax-check: $(SRC) $(WIN_DIR)/win32-stubs.h
	$(HOST_CC) -fsyntax-only -Wall -I$(WIN_DIR) $(SRC)
	@touch $@

clean:
	rm -f $(TARGET) syntax-check
