# CMakeLists.txt for afros-core kernel (noyau générique, indépendant de l'arch)
#
# Logique de haut niveau (ordonnanceur, mémoire, réseau, énergie, fs,
# sécurité). Ne touche jamais le matériel directement : tout accès passe par
# afros-hal (voir hal/include/*_abstraction.h).

add_library(afros-kernel STATIC
    scheduler/afros_cfs.c
    scheduler/afros_scheduler.c
    scheduler/big_little.c
    scheduler/big_little_support.c
    scheduler/power_aware.c
    scheduler/power_aware_sched.c
    scheduler/task_migration.c

    memory/adaptive_reclaim.c
    memory/adaptive_swap.c
    memory/compression.c
    memory/numa_aware.c
    memory/numa_balancer.c

    network/intelligent_routing.c
    network/intermittent_net.c
    network/protocol_compression.c
    network/protocol_optimization.c
    network/tcp_acceleration.c
    network/tcp_westwood_africa.c

    power/brownout_protection.c
    power/opportunistic_compute.c
    power/opportunistic_sleep.c
    power/solar_aware.c

    fs/afrosfs/file.c
    fs/afrosfs/journal.c

    security/secure_boot.c
)

target_include_directories(afros-kernel PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(afros-kernel PUBLIC afros-hal)

# Simulateur hébergé (mode actuel : printf/stdio, tourne en espace
# utilisateur). Formalise ce que faisait déjà `afros-core/Makefile` — reste
# le point d'entrée `make run` tant que le noyau n'est pas freestanding.
#
# Port mcu exclu (étape 4) : c'est le seul port bare-metal/freestanding —
# lier un exécutable "hébergé" échouerait (pas de _start ni de runtime libc
# fournis par un toolchain arm-none-eabi nu) ou induirait en erreur (succès
# apparent qui ne tourne sur aucun matériel réel). Le vrai point d'entrée
# bare-metal (vecteur d'interruption, crt0, Kernel/hal/scripts/linker.ld
# actuellement non câblé) reste un travail dédié — voir docs/porting_guide.md.
if(NOT AFROS_PORT STREQUAL "mcu")
    add_executable(afros-kernel-sim main.c)
    target_link_libraries(afros-kernel-sim PRIVATE afros-kernel afros-hal afros-drivers afros-port)
else()
    message(STATUS "AFROS_PORT=mcu : afros-kernel-sim non construit (port bare-metal, pas de runtime hébergé).")
endif()
