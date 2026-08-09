# Toolchain de cross-compilation : port arm64.
#
# Cible un GCC croisé "hosted" (glibc) : le noyau AfriOS est aujourd'hui un
# simulateur hébergé (printf/stdio.h, voir hal/docs/architecture.md), pas
# encore un binaire freestanding. Ce toolchain permet donc de croiser
# afros-kernel-sim pour tourner sous QEMU user-mode ARM64 ou sur une carte
# ARM64 sous Linux dès aujourd'hui, sans changement de code.
#
# Pour un vrai firmware bare-metal (sans OS hôte), il faudra un toolchain
# freestanding (aarch64-none-elf-gcc) ET remplacer les appels printf/stdio
# du noyau par arch_console_ops (déjà défini dans console_abstraction.h,
# déjà implémenté par ports/port-arm64/src/console_port.c) — étape suivante
# documentée dans docs/porting_guide.md (FirmwareHybride).
#
# Usage :
#   cmake -B build-arm64 -DAFROS_PORT=arm64 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/toolchain-arm64.cmake

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(AFROS_CROSS_PREFIX "aarch64-linux-gnu-" CACHE STRING "Préfixe du toolchain croisé arm64")

find_program(CMAKE_C_COMPILER   ${AFROS_CROSS_PREFIX}gcc)
find_program(CMAKE_CXX_COMPILER ${AFROS_CROSS_PREFIX}g++)

if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR
        "Compilateur croisé introuvable : ${AFROS_CROSS_PREFIX}gcc\n"
        "Installer gcc-aarch64-linux-gnu (Debian/Ubuntu) ou équivalent, "
        "ou surcharger -DAFROS_CROSS_PREFIX=<autre-préfixe>.")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
