# CMakeLists.txt for afros-core HAL (Hardware Abstraction Layer)
#
# Frontière unique entre le noyau générique (afros/) et le matériel. Expose
# les contrats (include/) et le dispatch générique (src/) qui délègue vers le
# port actif (arch/plateforme) — voir hal/docs/architecture.md.
#
# IMPORTANT: les autres modules (afros-network, afros-storage,
# afros-power-management, ...) référencent aujourd'hui ce dossier par chemin
# relatif (../afros-core/Kernel/hal/include). Ne pas déplacer hal/ sans
# d'abord les faire pointer vers la cible afros-hal ci-dessous.

# cpu_manager.c et memory_manager.c ont été déplacés vers
# ports/port-arm64/{cpu_port.c,memory_port.c} (étape 3) : ils codaient en dur
# des hypothèses ARM64 big.LITTLE alors que hal/ doit rester indépendante de
# toute architecture. arch_cpu_ops / arch_memory_ops / arch_interrupt_ops /
# arch_timer_ops / arch_console_ops / arch_storage_ops sont désormais fournis
# par le port sélectionné via AFROS_PORT (voir Kernel/CMakeLists.txt).
#
# device_manager.c (étape 4) : registre de device_ops_t, générique et
# indépendant de l'architecture (contrairement aux 6 tables ci-dessus, ce
# n'est PAS un composant par-port — voir ports/README.md).
add_library(afros-hal STATIC
    src/hal_init.c
    src/device_manager.c
    src/network_manager.c
    src/power_manager.c
    src/gpu_manager.c
    src/io_subsystem.c
    src/virtualization.c
    src/kprintf.c
)

target_include_directories(afros-hal PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Propagate AFROS_FREESTANDING to every consumer of afros-hal so kprintf.c,
# main.c (gate of `int main()`), hal_smoke_test.c (host-only test) and any
# other freestanding-aware source can branch consistently with the build mode.
if(AFROS_FREESTANDING)
    target_compile_definitions(afros-hal PUBLIC AFROS_FREESTANDING)
endif()
