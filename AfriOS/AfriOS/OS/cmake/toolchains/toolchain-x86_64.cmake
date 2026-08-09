# Toolchain de cross-compilation : port x86_64.
#
# Voir toolchain-arm64.cmake pour la note "hosted vs freestanding" — elle
# s'applique de façon identique ici.
#
# Usage :
#   cmake -B build-x86_64 -DAFROS_PORT=x86_64 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/toolchain-x86_64.cmake

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(AFROS_CROSS_PREFIX "x86_64-linux-gnu-" CACHE STRING "Préfixe du toolchain croisé x86_64")

find_program(CMAKE_C_COMPILER   ${AFROS_CROSS_PREFIX}gcc)
find_program(CMAKE_CXX_COMPILER ${AFROS_CROSS_PREFIX}g++)

if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR
        "Compilateur croisé introuvable : ${AFROS_CROSS_PREFIX}gcc\n"
        "Sur un hôte déjà x86_64, gcc natif suffit (pas besoin de ce fichier). "
        "Sinon installer gcc-x86-64-linux-gnu ou surcharger -DAFROS_CROSS_PREFIX=<...>.")
endif()

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
