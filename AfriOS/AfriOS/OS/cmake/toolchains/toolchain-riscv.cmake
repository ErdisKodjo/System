# Toolchain de cross-compilation : port riscv (RV64GC, hosted/Linux).
#
# Voir toolchain-arm64.cmake pour la note "hosted vs freestanding" — elle
# s'applique de façon identique ici. Cible glibc RV64 (ex: exécution sous
# QEMU riscv64 avec un rootfs Linux), pas un firmware SBI bare-metal.
#
# Usage :
#   cmake -B build-riscv -DAFROS_PORT=riscv \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/toolchain-riscv.cmake

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR riscv64)

set(AFROS_CROSS_PREFIX "riscv64-linux-gnu-" CACHE STRING "Préfixe du toolchain croisé riscv64")

find_program(CMAKE_C_COMPILER   ${AFROS_CROSS_PREFIX}gcc)
find_program(CMAKE_CXX_COMPILER ${AFROS_CROSS_PREFIX}g++)

if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR
        "Compilateur croisé introuvable : ${AFROS_CROSS_PREFIX}gcc\n"
        "Installer gcc-riscv64-linux-gnu (Debian/Ubuntu) ou équivalent, "
        "ou surcharger -DAFROS_CROSS_PREFIX=<autre-préfixe>.")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
