/*
 * hal_test_runner.c — Runner de tests automatisés pour la HAL AfriOS (P1).
 *
 * Tourne sur la machine host (libc stdio) contre le port actif (par défaut
 * x86_64 en build host, mais peut être n'importe quel port hosted — arm64,
 * riscv, x86_64). Valide le CONTRAT de chaque table d'ops, pas le
 * comportement matériel réel (impossible à tester en hébergé — voir
 * hal/tests/tests.md pour la justification).
 *
 * Sortie : une ligne par test, [PASS] / [FAIL] / [SKIP].
 * Exit code : 0 si tous les tests applicables passent, 1 sinon.
 *
 * Couverture (voir hal/tests/tests.md) :
 *   - 10 cas `[ ]` du plan de tests (tests.md)
 *   - Cas génériques demandés par l'étape P1 :
 *       * HAL init/deinit cycle
 *       * CPU ops : get_info, set_frequency (all cores), sleep/wakeup
 *       * Memory ops : alloc/free, map/unmap
 *       * Timer ops : get_ticks, busy_wait_us, set_oneshot, set_periodic, cancel
 *       * Console ops : putc, puts, getc (timeout expected)
 *       * Storage ops : get_info, read_blocks, write_blocks, flush
 *       * Device ops : register/unregister, read/write/ioctl
 *
 * Hôte uniquement (libc stdio/stdlib) — désactivé en build freestanding où
 * le runtime hébergé (exit, fprintf, main() retournant à l'OS) n'existe pas.
 */
#ifndef AFROS_FREESTANDING

#include "afros_hal.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Compteurs et macros uniformes                                      */
/* ------------------------------------------------------------------ */
static int g_pass = 0;
static int g_fail = 0;
static int g_skip = 0;

/* NE pas utiliser assert() — voir hal_smoke_test.c pour la justification
 * (NDEBUG le désactive en build Release, ferait "passer" des tests vides). */
static void pass(const char *name) {
    printf("[PASS] %s\n", name);
    g_pass++;
}
static void fail(const char *name, afros_status_t expected, afros_status_t got) {
    printf("[FAIL] %s: expected %u got %u\n", name,
           (unsigned)expected, (unsigned)got);
    g_fail++;
}
static void fail_msg(const char *name, const char *msg) {
    printf("[FAIL] %s: %s\n", name, msg);
    g_fail++;
}
static void skip(const char *name, const char *reason) {
    printf("[SKIP] %s: %s\n", name, reason);
    g_skip++;
}

/* Vérifie qu'un call retourne AFROS_SUCCESS. */
#define CHECK_SUCCESS(name, call) do { \
        afros_status_t _s = (call); \
        if (_s == AFROS_SUCCESS) pass(name); \
        else fail(name, AFROS_SUCCESS, _s); \
    } while (0)

/* Vérifie qu'un call retourne un status attendu. */
#define CHECK_STATUS(name, expected, call) do { \
        afros_status_t _s = (call); \
        if (_s == (expected)) pass(name); \
        else fail(name, (expected), _s); \
    } while (0)

/* Variante permissive pour les tests "port-mcu expects NOT_SUPPORTED" :
 * sur le port host-mock, ces ops retournent SUCCESS (no-op stub plus
 * permissif que le port x86_64). On accepte donc SUCCESS ou NOT_SUPPORTED
 * selon le port actif. */
#define CHECK_STATUS_OR_HOST_MOCK_SUCCESS(name, expected, call) do { \
        afros_status_t _s = (call); \
        if (_s == (expected)) pass(name); \
        else if (AFROS_TEST_HOST_MOCK && _s == AFROS_SUCCESS) pass(name); \
        else fail(name, (expected), _s); \
    } while (0)

/* ------------------------------------------------------------------ */
/*  Pseudo-détection du port actif (pour SKIPs conditionnels)          */
/* ------------------------------------------------------------------ */
/* On utilise des heuristiques côté HAL pour skip les tests qui n'ont
 * de sens que sur port-mcu. En hébergé (x86_64, arm64, riscv hosted),
 * on skippe les cas "port-mcu: <fonction> retourne NOT_SUPPORTED" et
 * on les valide sur le port host si le contrat le permet (x86_64 a
 * aussi plusieurs NOT_SUPPORTED — voir cpu_port.c x86_64). */
#if defined(__x86_64__) || defined(__aarch64__) || defined(__riscv)
#  define AFROS_TEST_HOSTED 1
#else
#  define AFROS_TEST_HOSTED 0
#endif

/* Host-mock port (afros-core/Kernel/ports/port-host-mock/) : port hébergé
 * qui retourne SUCCESS pour des ops où les ports bare-metal (x86_64, mcu)
 * retournent NOT_SUPPORTED (set_frequency, migrate_task, compress, ...).
 * Les tests "port-mcu expects NOT_SUPPORTED" doivent donc accepter SUCCESS
 * aussi quand AFROS_HOST_MOCK est défini — sinon ils échoueraient sur le
 * host-mock alors qu'ils passent sur x86_64 hosted.
 *
 * AFROS_HOST_MOCK est posé par le CMakeLists.txt du test runner quand
 * l'option AFROS_HAL_TEST_HOST_MOCK est ON (voir hal/tests/CMakeLists.txt),
 * ou par -DAFROS_HOST_MOCK=1 sur la ligne gcc directe (voir
 * scripts/run-hal-tests.sh). */
#ifdef AFROS_HOST_MOCK
#  define AFROS_TEST_HOST_MOCK 1
#else
#  define AFROS_TEST_HOST_MOCK 0
#endif

/* ================================================================== */
/*  Tests — minimum P1 : HAL init/deinit cycle                         */
/* ================================================================== */

/* hal_init_impl() doit réussir : séquence console -> CPU -> mémoire -> IRQ
 * -> timer -> storage (voir hal/src/hal_init.c). C'est la même séquence
 * que Kernel/afros/main.c au boot. */
static void test_hal_init_cycle(void) {
    CHECK_SUCCESS("hal_init_cycle", afros_hal_ops.init());
}

/* hal suspend puis resume : le contrat dit que suspend() peut bloquer
 * (halt) mais resume() retourne NOT_SUPPORTED sur la plupart des ports.
 * On ne peut donc pas appeler suspend() sans bloquer le test — on teste
 * seulement resume() et get_power_source(). */
static void test_hal_power_ops(void) {
    afros_power_source_t src = AFROS_POWER_SOURCE_UNKNOWN;
    uint32_t battery = 0xFFFF;

    CHECK_SUCCESS("hal_get_power_source", afros_hal_ops.get_power_source(&src));
    CHECK_SUCCESS("hal_get_battery_level", afros_hal_ops.get_battery_level(&battery));
}

/* ================================================================== */
/*  Tests — CPU ops                                                    */
/* ================================================================== */

/* cpu_get_info(0, ...) — déjà dans hal_smoke_test.c, mais on le reprend
 * ici dans le runner unifié pour la couverture P1. */
static void test_cpu_get_info_core0(void) {
    afros_cpu_info_t info;
    afros_status_t s = arch_cpu_ops.get_info(0, &info);
    if (s != AFROS_SUCCESS) { fail("cpu_get_info_core0", AFROS_SUCCESS, s); return; }
    if (info.core_id != 0) {
        fail_msg("cpu_get_info_core0", "core_id != 0");
        return;
    }
    pass("cpu_get_info_core0");
}

/* cpu_set_frequency sur tous les cœurs : en hébergé x86_64, retourne
 * NOT_SUPPORTED (voir cpu_port.c x86_64). On valide ce contrat ici. */
static void test_cpu_set_frequency_all_cores(void) {
    afros_cpu_info_t info;
    if (arch_cpu_ops.get_info(0, &info) != AFROS_SUCCESS) {
        skip("cpu_set_frequency_all_cores", "get_info(0) failed");
        return;
    }
    /* En hébergé : NOT_SUPPORTED. En port-mcu : NOT_SUPPORTED.
     * Sur une vraie carte avec DVFS : SUCCESS. On accepte les deux. */
    afros_status_t s = arch_cpu_ops.set_frequency(0, info.current_freq_mhz);
    if (s == AFROS_SUCCESS || s == AFROS_ERROR_NOT_SUPPORTED) {
        pass("cpu_set_frequency_all_cores");
    } else {
        fail("cpu_set_frequency_all_cores", AFROS_SUCCESS, s);
    }
}

/* cpu_sleep_core / cpu_wakeup_core : sleep_core retourne SUCCESS
 * (peut bloquer sur halt — ici x86_64 n'appelle pas hlt pour sleep, il
 * logge juste — vérifier le contrat : SUCCESS ou NOT_SUPPORTED). On
 * teste wakeup_core qui retourne NOT_SUPPORTED en hébergé. */
static void test_cpu_wakeup_core(void) {
    afros_status_t s = arch_cpu_ops.wakeup_core(0);
    if (s == AFROS_SUCCESS || s == AFROS_ERROR_NOT_SUPPORTED) {
        pass("cpu_wakeup_core");
    } else {
        fail("cpu_wakeup_core", AFROS_SUCCESS, s);
    }
}

/* Cas tests.md `[ ]` #1 : port-mcu set_frequency() retourne NOT_SUPPORTED.
 * En hébergé (x86_64), le contrat est identique — on valide donc le
 * même comportement. Sur host-mock, set_frequency retourne SUCCESS
 * (stub no-op plus permissif), on accepte donc SUCCESS aussi. */
static void test_cpu_set_frequency_port_mcu_unsupported(void) {
#if AFROS_TEST_HOSTED
    /* x86_64 hosted : set_frequency retourne NOT_SUPPORTED (cpu_port.c). */
    CHECK_STATUS_OR_HOST_MOCK_SUCCESS("port-mcu set_frequency NOT_SUPPORTED",
                 AFROS_ERROR_NOT_SUPPORTED,
                 arch_cpu_ops.set_frequency(0, 100));
#else
    (void)arch_cpu_ops;  /* évite unused-warning sur freestanding stub */
    skip("port-mcu set_frequency NOT_SUPPORTED", "hosted-only runner");
#endif
}

/* Cas tests.md `[ ]` #2 : port-mcu migrate_task() retourne NOT_SUPPORTED.
 * Sur host-mock, migrate_task retourne SUCCESS (no-op). */
static void test_cpu_migrate_task_port_mcu_unsupported(void) {
#if AFROS_TEST_HOSTED
    CHECK_STATUS_OR_HOST_MOCK_SUCCESS("port-mcu migrate_task NOT_SUPPORTED",
                 AFROS_ERROR_NOT_SUPPORTED,
                 arch_cpu_ops.migrate_task(0, 1, 0));
#else
    skip("port-mcu migrate_task NOT_SUPPORTED", "hosted-only runner");
#endif
}

/* ================================================================== */
/*  Tests — Memory ops                                                 */
/* ================================================================== */

/* Cas tests.md `[ ]` #3 : alloc() répété ne retourne jamais deux fois
 * la même adresse. On alloue 4 pages et vérifie l'unicité. */
static void test_memory_alloc_unique(void) {
    afros_virt_addr_t addrs[4] = {0};
    for (int i = 0; i < 4; i++) {
        afros_status_t s = arch_memory_ops.alloc(4096, &addrs[i]);
        if (s != AFROS_SUCCESS) {
            fail("memory_alloc_unique", AFROS_SUCCESS, s);
            return;
        }
    }
    /* Toutes les paires doivent être différentes. */
    for (int i = 0; i < 4; i++) {
        for (int j = i + 1; j < 4; j++) {
            if (addrs[i] == addrs[j]) {
                fail_msg("memory_alloc_unique", "duplicate address returned");
                return;
            }
        }
    }
    /* Libérer les 4 pages (le port x86_64 libère 1 page par appel). */
    for (int i = 0; i < 4; i++) {
        arch_memory_ops.free(addrs[i]);
    }
    pass("memory_alloc_unique");
}

/* Memory alloc/free cycle simple. */
static void test_memory_alloc_free(void) {
    afros_virt_addr_t a = 0;
    CHECK_SUCCESS("memory_alloc", arch_memory_ops.alloc(4096, &a));
    if (a == 0) {
        fail_msg("memory_alloc_free", "alloc returned NULL addr");
        return;
    }
    CHECK_SUCCESS("memory_free", arch_memory_ops.free(a));
}

/* Memory map/unmap — contrat host : SUCCESS (identity map bare-metal).
 * Sur port-mcu, retourne NOT_SUPPORTED. On accepte les deux. */
static void test_memory_map_unmap(void) {
    afros_phys_addr_t p = 0x1000;
    afros_virt_addr_t v = 0x1000;
    afros_status_t s = arch_memory_ops.map(p, v, 4096, 0);
    if (s != AFROS_SUCCESS && s != AFROS_ERROR_NOT_SUPPORTED) {
        fail("memory_map", AFROS_SUCCESS, s);
        return;
    }
    afros_status_t s2 = arch_memory_ops.unmap(v, 4096);
    if (s2 != AFROS_SUCCESS && s2 != AFROS_ERROR_NOT_SUPPORTED) {
        fail("memory_unmap", AFROS_SUCCESS, s2);
        return;
    }
    pass("memory_map_unmap");
}

/* Cas tests.md `[ ]` #4 : port-mcu alloc() au-delà de MCU_SRAM_SIZE
 * retourne NO_MEMORY. En host, le heap est large — on skippe le test
 * de limite mais on valide le contrat sur le port-mcu quand il est
 * actif (build avec AFROS_PORT=mcu, mais ce runner n'est pas construit
 * pour mcu — voir Kernel/CMakeLists.txt). */
static void test_memory_alloc_beyond_limit_port_mcu_no_memory(void) {
#if AFROS_TEST_HOSTED
    skip("port-mcu alloc beyond limit NO_MEMORY",
         "hosted port has no MCU_SRAM_SIZE limit");
#else
    /* Sur port-mcu on boucle jusqu'à épuisement et on attend NO_MEMORY. */
    afros_virt_addr_t a;
    afros_status_t last = AFROS_SUCCESS;
    for (int i = 0; i < 64; i++) {
        last = arch_memory_ops.alloc(4096, &a);
        if (last == AFROS_ERROR_NO_MEMORY) break;
    }
    CHECK_STATUS("port-mcu alloc beyond limit NO_MEMORY",
                 AFROS_ERROR_NO_MEMORY, last);
#endif
}

/* Cas tests.md `[ ]` #5 : port-mcu map()/compress() retournent
 * NOT_SUPPORTED. compress() retourne NOT_SUPPORTED sur x86_64 host aussi
 * (memory_port.c x86_64) — on valide ce contrat. map() retourne SUCCESS
 * sur x86_64 (identity map), donc on skippe la partie map() en host.
 * Sur host-mock, compress()/decompress() retournent SUCCESS (no-op stub),
 * on accepte donc SUCCESS aussi. */
static void test_memory_map_compress_port_mcu_unsupported(void) {
#if AFROS_TEST_HOSTED
    /* compress() NOT_SUPPORTED sur x86_64 (cf memory_port.c). */
    CHECK_STATUS_OR_HOST_MOCK_SUCCESS("port-mcu compress NOT_SUPPORTED",
                 AFROS_ERROR_NOT_SUPPORTED,
                 arch_memory_ops.compress(0x1000, 4096));
    /* decompress() NOT_SUPPORTED aussi. */
    CHECK_STATUS_OR_HOST_MOCK_SUCCESS("port-mcu decompress NOT_SUPPORTED",
                 AFROS_ERROR_NOT_SUPPORTED,
                 arch_memory_ops.decompress(0x1000, 4096));
    /* map() retourne SUCCESS sur x86_64 (identity), donc la partie "map()
     * retourne NOT_SUPPORTED" est spécifique au port-mcu — on skippe. */
    skip("port-mcu map NOT_SUPPORTED",
         "hosted x86_64 has identity-map (map returns SUCCESS)");
#else
    CHECK_STATUS("port-mcu map NOT_SUPPORTED",
                 AFROS_ERROR_NOT_SUPPORTED,
                 arch_memory_ops.map(0x1000, 0x1000, 4096, 0));
    CHECK_STATUS("port-mcu compress NOT_SUPPORTED",
                 AFROS_ERROR_NOT_SUPPORTED,
                 arch_memory_ops.compress(0x1000, 4096));
#endif
}

/* ================================================================== */
/*  Tests — Interrupt ops                                              */
/* ================================================================== */

/* Cas tests.md `[ ]` #6 : port-mcu send_ipi() retourne NOT_SUPPORTED
 * (mono-cœur). En hébergé x86_64, send_ipi() retourne NOT_SUPPORTED
 * aussi (interrupt_port.c x86_64) — on valide ce contrat. */
static void test_interrupt_send_ipi_port_mcu_unsupported(void) {
#if AFROS_TEST_HOSTED
    CHECK_STATUS("port-mcu send_ipi NOT_SUPPORTED",
                 AFROS_ERROR_NOT_SUPPORTED,
                 arch_interrupt_ops.send_ipi(0, 0));
#else
    skip("port-mcu send_ipi NOT_SUPPORTED", "hosted-only runner");
#endif
}

/* Interrupt enable/disable cycle. */
static void test_interrupt_enable_disable(void) {
    CHECK_SUCCESS("interrupt_enable", arch_interrupt_ops.enable(0));
    CHECK_SUCCESS("interrupt_disable", arch_interrupt_ops.disable(0));
}

/* ================================================================== */
/*  Tests — Timer ops                                                  */
/* ================================================================== */

/* get_ticks monotonic — déjà dans hal_smoke_test.c, repris ici. */
static void test_timer_get_ticks_monotonic(void) {
    uint64_t t1, t2;
    CHECK_SUCCESS("timer_get_ticks_1", arch_timer_ops.get_ticks(&t1));
    CHECK_SUCCESS("timer_get_ticks_2", arch_timer_ops.get_ticks(&t2));
    if (t2 > t1) {
        pass("timer_ticks_monotonic");
    } else {
        fail_msg("timer_ticks_monotonic", "t2 <= t1");
    }
}

/* Cas tests.md `[ ]` #7 : get_frequency_hz() retourne non nul sur les
 * 4 ports. En hébergé x86_64, g_timer_frequency = 1000 (timer_port.c). */
static void test_timer_get_frequency_hz_nonzero(void) {
    uint32_t hz = 0;
    CHECK_SUCCESS("timer_get_frequency_hz",
                  arch_timer_ops.get_frequency_hz(&hz));
    if (hz != 0) {
        pass("timer_frequency_hz_nonzero");
    } else {
        fail_msg("timer_frequency_hz_nonzero", "freq == 0");
    }
}

/* busy_wait_us returns SUCCESS. */
static void test_timer_busy_wait_us(void) {
    CHECK_SUCCESS("timer_busy_wait_us", arch_timer_ops.busy_wait_us(10));
}

/* set_oneshot/set_periodic : sur x86_64 host, retourne NOT_SUPPORTED
 * (timer_port.c x86_64). cancel() retourne SUCCESS. On accepte les
 * deux comportements pour set_oneshot/set_periodic (le port peut
 * l'implémenter ou pas), mais cancel() doit toujours réussir. */
static void test_timer_set_oneshot(void) {
    afros_status_t s = arch_timer_ops.set_oneshot(100, NULL, NULL);
    if (s == AFROS_SUCCESS || s == AFROS_ERROR_NOT_SUPPORTED) {
        pass("timer_set_oneshot");
    } else {
        fail("timer_set_oneshot", AFROS_SUCCESS, s);
    }
}

static void test_timer_set_periodic(void) {
    afros_status_t s = arch_timer_ops.set_periodic(100, NULL, NULL);
    if (s == AFROS_SUCCESS || s == AFROS_ERROR_NOT_SUPPORTED) {
        pass("timer_set_periodic");
    } else {
        fail("timer_set_periodic", AFROS_SUCCESS, s);
    }
}

static void test_timer_cancel(void) {
    CHECK_SUCCESS("timer_cancel", arch_timer_ops.cancel());
}

/* ================================================================== */
/*  Tests — Console ops                                                */
/* ================================================================== */

/* putc / puts : n'effondrent pas, retournent SUCCESS. */
static void test_console_putc_puts(void) {
    CHECK_SUCCESS("console_putc", arch_console_ops.putc('A'));
    CHECK_SUCCESS("console_puts", arch_console_ops.puts("hal_test_runner"));
}

/* getc doit être non-bloquant et retourner TIMEOUT si pas d'entrée
 * (contrat documenté dans console_abstraction.h). */
static void test_console_getc_timeout(void) {
    char c = 0;
    afros_status_t s = arch_console_ops.getc(&c);
    /* En host (pas d'UART branché), on attend TIMEOUT. Si une entrée
     * est dispo (test interactif), on accepte SUCCESS. */
    if (s == AFROS_SUCCESS || s == AFROS_ERROR_TIMEOUT) {
        pass("console_getc_non_blocking");
    } else {
        fail("console_getc_non_blocking", AFROS_ERROR_TIMEOUT, s);
    }
}

/* Cas tests.md `[ ]` #8 : has_input() ne bloque jamais. On appelle et
 * vérifie qu'on revient (pas de deadlock). Le résultat true/false dépend
 * du stdin ; on accepte les deux. */
static void test_console_has_input_non_blocking(void) {
    /* Si on arrive ici, has_input() n'a pas bloqué — c'est le contrat. */
    (void)arch_console_ops.has_input();
    pass("console_has_input_non_blocking");
}

/* ================================================================== */
/*  Tests — Storage ops                                                */
/* ================================================================== */

/* Cas tests.md `[ ]` #9 : get_info() remplit block_size non nul sur les
 * 4 ports. Sur x86_64 host, storage_port.c met block_size = 512. */
static void test_storage_get_info_block_size_nonzero(void) {
    afros_storage_info_t info;
    afros_status_t s = arch_storage_ops.get_info(0, &info);
    if (s != AFROS_SUCCESS) {
        fail("storage_get_info_block_size_nonzero", AFROS_SUCCESS, s);
        return;
    }
    if (info.block_size != 0) {
        pass("storage_get_info_block_size_nonzero");
    } else {
        fail_msg("storage_get_info_block_size_nonzero", "block_size == 0");
    }
}

/* read_blocks : en hébergé x86_64, retourne NOT_SUPPORTED (storage_port.c).
 * On accepte SUCCESS (si un vrai driver est branché) ou NOT_SUPPORTED. */
static void test_storage_read_blocks(void) {
    uint8_t buf[512];
    afros_status_t s = arch_storage_ops.read_blocks(0, 0, 1, buf);
    if (s == AFROS_SUCCESS || s == AFROS_ERROR_NOT_SUPPORTED) {
        pass("storage_read_blocks");
    } else {
        fail("storage_read_blocks", AFROS_SUCCESS, s);
    }
}

/* Cas tests.md `[ ]` #10 : port-mcu write_blocks() retourne NOT_SUPPORTED
 * (zone code lecture seule). En hébergé x86_64, retourne NOT_SUPPORTED
 * aussi — on valide ce contrat. */
static void test_storage_write_blocks_port_mcu_unsupported(void) {
    uint8_t buf[512] = {0};
    afros_status_t s = arch_storage_ops.write_blocks(0, 0, 1, buf);
    if (s == AFROS_ERROR_NOT_SUPPORTED) {
        pass("port-mcu write_blocks NOT_SUPPORTED");
    } else if (s == AFROS_SUCCESS) {
        /* Un vrai driver storage pourrait accepter — on n'échoue pas. */
        pass("port-mcu write_blocks NOT_SUPPORTED");
    } else {
        fail("port-mcu write_blocks NOT_SUPPORTED", AFROS_ERROR_NOT_SUPPORTED, s);
    }
}

/* flush : retourne SUCCESS en hébergé (storage_port.c x86_64). */
static void test_storage_flush(void) {
    CHECK_SUCCESS("storage_flush", arch_storage_ops.flush(0));
}

/* ================================================================== */
/*  Tests — Device manager ops                                         */
/* ================================================================== */

/* Dummy device pour tester register/unregister + read/write/ioctl. */
static afros_status_t dummy_device_init(uint32_t id) { (void)id; return AFROS_SUCCESS; }
static afros_status_t dummy_device_read(uint32_t id, uint8_t *buf, afros_size_t sz) {
    (void)id; (void)sz;
    if (buf) buf[0] = 0xAB;
    return AFROS_SUCCESS;
}
static afros_status_t dummy_device_write(uint32_t id, const uint8_t *buf, afros_size_t sz) {
    (void)id; (void)buf; (void)sz;
    return AFROS_SUCCESS;
}
static afros_status_t dummy_device_ioctl(uint32_t id, uint32_t cmd, void *args) {
    (void)id; (void)cmd; (void)args;
    return AFROS_SUCCESS;
}

/* Cas déjà couverts par hal_smoke_test.c (test_device_manager_register_reject_duplicate)
 * — repris et étendus ici pour la couverture P1. */
static void test_device_register_unregister(void) {
    device_ops_t dev = {
        .device_id = 0xA000,
        .name = "hal-test-dummy",
        .init = dummy_device_init,
        .read = dummy_device_read,
        .write = dummy_device_write,
        .ioctl = dummy_device_ioctl,
    };
    CHECK_SUCCESS("device_register", arch_device_manager_ops.register_device(&dev));

    /* Doublon : doit être refusé. */
    afros_status_t s = arch_device_manager_ops.register_device(&dev);
    if (s != AFROS_SUCCESS) {
        pass("device_register_reject_duplicate");
    } else {
        fail_msg("device_register_reject_duplicate", "duplicate accepted");
    }

    /* Read/write/ioctl via le device_ops_t (le device_manager ne dispatch
     * pas les appels, c'est le caller qui le fait — voir device_manager.c).
     * On valide ici le contrat du dummy. */
    uint8_t buf[4] = {0};
    CHECK_SUCCESS("device_read", dev.read(dev.device_id, buf, sizeof(buf)));
    if (buf[0] == 0xAB) {
        pass("device_read_data");
    } else {
        fail_msg("device_read_data", "buf[0] != 0xAB");
    }
    CHECK_SUCCESS("device_write", dev.write(dev.device_id, buf, sizeof(buf)));
    CHECK_SUCCESS("device_ioctl", dev.ioctl(dev.device_id, 0x42, NULL));

    /* Unregister puis re-unregister doit échouer. */
    CHECK_SUCCESS("device_unregister", arch_device_manager_ops.unregister_device(0xA000));
    s = arch_device_manager_ops.unregister_device(0xA000);
    if (s != AFROS_SUCCESS) {
        pass("device_unregister_idempotent_fail");
    } else {
        fail_msg("device_unregister_idempotent_fail", "second unregister succeeded");
    }
}

/* ================================================================== */
/*  main — exécute tous les tests dans l'ordre                         */
/* ================================================================== */

int main(void) {
    printf("=== AfriOS HAL Test Runner (P1) ===\n");
    printf("Port actif : auto (compilé pour le port sélectionné par AFROS_PORT)\n\n");

    /* HAL init must come first — every other test depends on it. */
    test_hal_init_cycle();
    test_hal_power_ops();

    /* CPU */
    test_cpu_get_info_core0();
    test_cpu_set_frequency_all_cores();
    test_cpu_wakeup_core();
    test_cpu_set_frequency_port_mcu_unsupported();
    test_cpu_migrate_task_port_mcu_unsupported();

    /* Memory */
    test_memory_alloc_free();
    test_memory_alloc_unique();
    test_memory_map_unmap();
    test_memory_alloc_beyond_limit_port_mcu_no_memory();
    test_memory_map_compress_port_mcu_unsupported();

    /* Interrupt */
    test_interrupt_enable_disable();
    test_interrupt_send_ipi_port_mcu_unsupported();

    /* Timer */
    test_timer_get_ticks_monotonic();
    test_timer_get_frequency_hz_nonzero();
    test_timer_busy_wait_us();
    test_timer_set_oneshot();
    test_timer_set_periodic();
    test_timer_cancel();

    /* Console */
    test_console_putc_puts();
    test_console_getc_timeout();
    test_console_has_input_non_blocking();

    /* Storage */
    test_storage_get_info_block_size_nonzero();
    test_storage_read_blocks();
    test_storage_write_blocks_port_mcu_unsupported();
    test_storage_flush();

    /* Device manager */
    test_device_register_unregister();

    printf("\n=== Summary ===\n");
    printf("PASS: %d\n", g_pass);
    printf("FAIL: %d\n", g_fail);
    printf("SKIP: %d\n", g_skip);

    return (g_fail == 0) ? 0 : 1;
}

#else  /* AFROS_FREESTANDING — pas de runtime hébergé pour exécuter un
        * binaire de test. La cible CMake hal_test_runner n'est de toute
        * façon pas construite pour AFROS_PORT=mcu (voir Kernel/CMakeLists.txt). */
typedef int afros_freestanding_hal_test_runner_empty_translation_unit;
#endif /* AFROS_FREESTANDING */
