/*
 * hal_smoke_test.c — Tests unitaires HAL (étape 5 — voir hal/tests/tests.md pour
 * le plan complet). Vérifie le CONTRAT que tout port doit respecter, pas le
 * comportement matériel réel (impossible à tester ici, en hébergé).
 *
 * Ce fichier tourne contre le port sélectionné par AFROS_PORT au moment du
 * build (cmake -DAFROS_PORT=<x>) : le même fichier valide donc les 4 ports
 * sans modification, un par exécution de build.
 *
 * Hôte uniquement (libc stdio/stdlib) — désactivé en build freestanding où le
 * runtime hébergé (exit, fprintf, main() retournant à l'OS) n'existe pas.
 */
#ifndef AFROS_FREESTANDING

#include "afros_hal.h"
#include <stdio.h>
#include <stdlib.h>

static int tests_run = 0;

/* Volontairement pas assert() : assert() est compilée à vide si NDEBUG est
 * défini (build Release), ce qui ferait "passer" silencieusement des tests
 * qui ne vérifient plus rien. CHECK() reste actif dans tous les modes. */
#define CHECK(cond) do { \
        if (!(cond)) { \
            fprintf(stderr, "[TEST] ECHEC : %s (%s:%d)\n", #cond, __FILE__, __LINE__); \
            exit(1); \
        } \
        tests_run++; \
    } while (0)

/* Vérifie que le port actif a rempli les champs obligatoires de chaque
 * table d'ops - contrat minimal que TOUT port doit respecter (voir
 * ports/README.md). */
static void test_port_contract(void) {
    CHECK(arch_cpu_ops.init != NULL);
    CHECK(arch_cpu_ops.get_info != NULL);
    CHECK(arch_memory_ops.init != NULL);
    CHECK(arch_memory_ops.alloc != NULL);
    CHECK(arch_interrupt_ops.init != NULL);
    CHECK(arch_interrupt_ops.enable != NULL);
    CHECK(arch_interrupt_ops.disable != NULL);
    CHECK(arch_timer_ops.init != NULL);
    CHECK(arch_timer_ops.get_ticks != NULL);
    CHECK(arch_console_ops.init != NULL);
    CHECK(arch_console_ops.puts != NULL);
    CHECK(arch_storage_ops.init != NULL);
    printf("[TEST] port_contract: OK\n");
}

/* hal_init_impl() doit réussir avec n'importe quel port : c'est la séquence
 * exacte que Kernel/afros/main.c exécute au démarrage (console -> CPU ->
 * mémoire -> IRQ -> timer, voir hal/src/hal_init.c). */
static void test_hal_init_succeeds(void) {
    afros_status_t status = afros_hal_ops.init();
    CHECK(status == AFROS_SUCCESS);
    printf("[TEST] hal_init: OK\n");
}

/* cpu_id 0 doit toujours être une entrée valide, y compris sur le port mcu
 * (mono-coeur, où c'est le SEUL id valide). */
static void test_cpu_get_info_core0(void) {
    afros_cpu_info_t info;
    afros_status_t status = arch_cpu_ops.get_info(0, &info);
    CHECK(status == AFROS_SUCCESS);
    CHECK(info.core_id == 0);
    printf("[TEST] cpu_get_info(0): OK\n");
}

/* Le timer doit être monotone, même en simulation (contrat, pas de valeur
 * matérielle réelle attendue ici). */
static void test_timer_ticks_monotonic(void) {
    uint64_t t1, t2;
    CHECK(arch_timer_ops.get_ticks(&t1) == AFROS_SUCCESS);
    CHECK(arch_timer_ops.get_ticks(&t2) == AFROS_SUCCESS);
    CHECK(t2 > t1);
    printf("[TEST] timer_ticks_monotonic: OK\n");
}

/* device_manager : enregistrement et rejet de doublon (hal/src/device_manager.c,
 * étape 4). N'exerce aucun port, teste la couche générique. */
static void test_device_manager_register_reject_duplicate(void) {
    device_ops_t dummy_a = { .device_id = 999, .name = "test-dummy-a" };
    device_ops_t dummy_b = { .device_id = 999, .name = "test-dummy-b" };

    CHECK(arch_device_manager_ops.register_device(&dummy_a) == AFROS_SUCCESS);
    CHECK(arch_device_manager_ops.register_device(&dummy_b) != AFROS_SUCCESS); /* id déjà pris */
    CHECK(arch_device_manager_ops.unregister_device(999) == AFROS_SUCCESS);
    CHECK(arch_device_manager_ops.unregister_device(999) != AFROS_SUCCESS); /* déjà retiré */

    printf("[TEST] device_manager_register_reject_duplicate: OK\n");
}

int main(void) {
    test_port_contract();
    test_hal_init_succeeds();
    test_cpu_get_info_core0();
    test_timer_ticks_monotonic();
    test_device_manager_register_reject_duplicate();

    printf("[TEST] %d assertions passees.\n", tests_run);
    return 0;
}

#else  /* AFROS_FREESTANDING — pas de runtime hébergé pour exécuter un binaire
        * de test. La cible CMake afros-hal-tests n'est de toute façon pas
        * construite pour AFROS_PORT=mcu (voir Kernel/CMakeLists.txt). */
typedef int afros_freestanding_hal_smoke_test_empty_translation_unit;
#endif /* AFROS_FREESTANDING */
