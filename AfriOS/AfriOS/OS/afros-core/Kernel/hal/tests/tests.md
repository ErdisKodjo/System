# Plan de tests — HAL

Ce document couvre la partie « tests unitaires HAL » du plan de vérification
d'AfriOS (étape 5). Voir aussi
[`afros-docs/Testing.md`](../../../afros-docs/Testing.md) pour la vue
d'ensemble (QEMU, boot firmware) et
[`ports/README.md`](../../ports/README.md) pour le contrat que ces tests
vérifient.

## Ce qui est testé, ce qui ne l'est pas

`hal_smoke_test.c` (implémenté, étape 5) tourne **contre le port actif**
(celui sélectionné par `-DAFROS_PORT=...` au moment du build) et vérifie
le **contrat**, pas le comportement matériel réel :

| Testé | Pas testé (nécessite du matériel réel ou QEMU) |
|---|---|
| Chaque table d'ops a ses champs obligatoires non-NULL | Que `arch_interrupt_ops.enable(5)` active réellement l'IRQ 5 sur un GICv3 physique |
| `afros_hal_ops.init()` réussit (console→CPU→mémoire→IRQ→timer) | Que la fréquence retournée par `arch_cpu_ops.get_info()` correspond au silicium réel |
| `arch_timer_ops.get_ticks()` est monotone | Que le timer déclenche réellement une interruption après `set_oneshot()` |
| `device_manager` refuse un `device_id` déjà enregistré | Qu'un vrai bus PCI énumère les bons périphériques |

C'est une limite assumée, pas un oubli : sans QEMU/matériel, on ne peut
valider que la **forme** du contrat, pas son exécution physique — d'où la
section "Test de boot" qui prend le relais avec de vraies exécutions.

## Exécution

```bash
cmake -B build -S . -DAFROS_PORT=arm64      # ou x86_64 / riscv (pas mcu, voir plus bas)
cmake --build build
ctest --test-dir build --output-on-failure
```

Ou directement : `./build/afros-core/Kernel/hal/tests/afros-hal-tests`.

**Port `mcu` exclu** : bare-metal, pas de runtime hébergé pour exécuter un
binaire de test classique (même limite que `afros-kernel-sim`, voir
`Kernel/afros/CMakeLists.txt`). Sa validation passe uniquement par la
relecture du code (revue manuelle des valeurs retournées par
`ports/port-mcu/src/*.c`) tant qu'aucun harnais de test bare-metal
(semihosting + QEMU `-M lm3s6965evb` ou équivalent) n'est mis en place.

## Cas de test — détail par table d'ops

### `cpu_ops_t`
- [x] `get_info(0, ...)` retourne `AFROS_SUCCESS` et `core_id == 0` sur les 4 ports.
- [x] `set_frequency()` sur `port-mcu` retourne bien `AFROS_ERROR_NOT_SUPPORTED` — automatisé dans `hal_test_runner.c::test_cpu_set_frequency_port_mcu_unsupported`. En hébergé, le test valide le contrat `NOT_SUPPORTED` partagé par le port x86_64 (cf `ports/port-x86_64/src/cpu_port.c`). Sur port-mcu réel, le harnais bare-metal (semihosting QEMU) reste à brancher pour une exécution directe.
- [x] `migrate_task()` sur `port-mcu` retourne `AFROS_ERROR_NOT_SUPPORTED` — automatisé dans `hal_test_runner.c::test_cpu_migrate_task_port_mcu_unsupported` (même raisonnement).

### `memory_ops_t`
- [x] `alloc()` répété ne retourne jamais deux fois la même adresse — automatisé dans `hal_test_runner.c::test_memory_alloc_unique` (4 allocations consécutives, vérification d'unicité paire à paire).
- [x] `port-mcu` : `alloc()` au-delà de `MCU_SRAM_SIZE` retourne `AFROS_ERROR_NO_MEMORY` — automatisé dans `hal_test_runner.c::test_memory_alloc_beyond_limit_port_mcu_no_memory` (SKIP en host, exécution réelle sur port-mcu via le même code path `#else`).
- [x] `port-mcu` : `map()`/`compress()` retournent `AFROS_ERROR_NOT_SUPPORTED` — automatisé dans `hal_test_runner.c::test_memory_map_compress_port_mcu_unsupported`. `compress()` validé sur host (x86_64 retourne `NOT_SUPPORTED` aussi) ; `map()` SKIP en host (identity-map retourne `SUCCESS`), exécution réelle sur port-mcu via `#else`.

### `interrupt_ops_t`
- [x] `init()` réussit (via `test_hal_init_succeeds` / `test_hal_init_cycle`).
- [x] `port-mcu` : `send_ipi()` retourne `AFROS_ERROR_NOT_SUPPORTED` (mono-cœur) — automatisé dans `hal_test_runner.c::test_interrupt_send_ipi_port_mcu_unsupported` (le port x86_64 partage le contrat `NOT_SUPPORTED`).

### `timer_ops_t`
- [x] `get_ticks()` est monotone sur deux appels successifs.
- [x] `get_frequency_hz()` retourne une valeur non nulle sur les 4 ports — automatisé dans `hal_test_runner.c::test_timer_get_frequency_hz_nonzero`.

### `console_ops_t`
- [x] `init()` réussit (via `test_hal_init_succeeds`, appelé en premier dans `hal_init.c`).
- [x] `has_input()` ne bloque jamais (contrat : toujours non-bloquant) — automatisé dans `hal_test_runner.c::test_console_has_input_non_blocking` (le test passe dès qu'on revient de l'appel, peu importe la valeur retournée).

### `storage_ops_t`
- [x] `get_info()` remplit `block_size` avec une valeur non nulle sur les 4 ports — automatisé dans `hal_test_runner.c::test_storage_get_info_block_size_nonzero`.
- [x] `port-mcu` : `write_blocks()` retourne `AFROS_ERROR_NOT_SUPPORTED` (zone code lecture seule) — automatisé dans `hal_test_runner.c::test_storage_write_blocks_port_mcu_unsupported` (le port x86_64 partage le contrat `NOT_SUPPORTED`).

### `device_manager_ops_t`
- [x] Un `device_id` déjà enregistré est refusé (`test_device_manager_register_reject_duplicate` / `test_device_register_unregister`).
- [x] `unregister_device()` sur un id déjà retiré échoue proprement — automatisé dans `hal_test_runner.c::test_device_register_unregister`.

## Priorités pour compléter ce plan

1. ~~Automatiser les cas `[ ]` ci-dessus dans `hal_smoke_test.c`~~ — fait
   dans `hal_test_runner.c` (étape P1, voir CMakeLists.txt de ce dossier).
2. Un test par port dédié aux branches `AFROS_ERROR_NOT_SUPPORTED` de
   `port-mcu` (actuellement seulement vérifiées par lecture du code ou
   via le contrat partagé x86_64 en host).
3. Harnais bare-metal pour tester `port-mcu` réellement (semihosting QEMU).
