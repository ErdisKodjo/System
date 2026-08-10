# Toolchain de cross-compilation : port mcu (Cortex-M, bare-metal).
#
# Contrairement aux 3 autres ports, celui-ci ne peut pas cibler un GCC
# "hosted" : un Cortex-M n'a pas de noyau Linux dessous. arm-none-eabi-gcc
# est donc freestanding par nature (-ffreestanding, pas de _start hébergé).
#
# CE QUE CE TOOLCHAIN PERMET AUJOURD'HUI : compiler les fichiers de
# ports/port-mcu/src/*.c (code C standard, portable) et Kernel/hal, afros,
# drivers en objets .o valides pour Cortex-M.
#
# CE QU'IL NE FAIT PAS (gap connu, voir docs/porting_guide.md) :
# afros-kernel-sim n'est plus construit pour ce port (voir
# Kernel/afros/CMakeLists.txt) car il manque encore, pour obtenir un binaire
# qui démarre réellement sur un Cortex-M :
#   - une table de vecteurs d'interruption (.vector_table) et un handler
#     Reset_Handler en assembleur,
#   - la copie .data (FLASH->RAM) et l'initialisation .bss avant kernel_main,
#   - le câblage de Kernel/hal/scripts/linker.ld (actuellement écrit mais non
#     référencé par aucun CMakeLists.txt ni Makefile - `-T linker.ld` manquant).
#
# Usage (compilation des bibliothèques seules, pas d'exécutable final) :
#   cmake -B build-mcu -DAFROS_PORT=mcu \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/toolchain-mcu.cmake

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(AFROS_CROSS_PREFIX "arm-none-eabi-" CACHE STRING "Préfixe du toolchain croisé Cortex-M")

find_program(CMAKE_C_COMPILER   ${AFROS_CROSS_PREFIX}gcc)
find_program(CMAKE_CXX_COMPILER ${AFROS_CROSS_PREFIX}g++)

if(NOT CMAKE_C_COMPILER)
    message(FATAL_ERROR
        "Compilateur croisé introuvable : ${AFROS_CROSS_PREFIX}gcc\n"
        "Installer gcc-arm-none-eabi (Debian/Ubuntu, ou distribution ARM "
        "officielle), ou surcharger -DAFROS_CROSS_PREFIX=<autre-préfixe>.")
endif()

# CMAKE_SYSTEM_NAME=Generic : CMake ne tente pas de tester le compilateur en
# liant un exécutable complet (impossible sans crt0/linker script dédiés).
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-m4 -mthumb -ffreestanding")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
