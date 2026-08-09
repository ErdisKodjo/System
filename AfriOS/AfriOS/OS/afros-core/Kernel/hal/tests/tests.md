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
- [ ] `set_frequency()` sur `port-mcu` retourne bien `AFROS_ERROR_NOT_SUPPORTED` (revue manuelle faite ; pas encore automatisé — nécessite le harnais bare-metal ci-dessus).
- [ ] `migrate_task()` sur `port-mcu` retourne `AFROS_ERROR_NOT_SUPPORTED` (idem).

### `memory_ops_t`
- [ ] `alloc()` répété ne retourne jamais deux fois la même adresse (à automatiser : actuellement seul le smoke test générique tourne).
- [ ] `port-mcu` : `alloc()` au-delà de `MCU_SRAM_SIZE` retourne `AFROS_ERROR_NO_MEMORY` (revue manuelle du code, cas limite non testé automatiquement).
- [ ] `port-mcu` : `map()`/`compress()` retournent `AFROS_ERROR_NOT_SUPPORTED`.

### `interrupt_ops_t`
- [x] `init()` réussit (via `test_hal_init_succeeds`).
- [ ] `port-mcu` : `send_ipi()` retourne `AFROS_ERROR_NOT_SUPPORTED` (mono-cœur).

### `timer_ops_t`
- [x] `get_ticks()` est monotone sur deux appels successifs.
- [ ] `get_frequency_hz()` retourne une valeur non nulle sur les 4 ports.

### `console_ops_t`
- [x] `init()` réussit (via `test_hal_init_succeeds`, appelé en premier dans `hal_init.c`).
- [ ] `has_input()` ne bloque jamais (contrat : toujours non-bloquant).

### `storage_ops_t`
- [ ] `get_info()` remplit `block_size` avec une valeur non nulle sur les 4 ports.
- [x] `port-mcu` : `write_blocks()` retourne `AFROS_ERROR_NOT_SUPPORTED` (zone code lecture seule) — couvert par revue de code, à automatiser.

### `device_manager_ops_t`
- [x] Un `device_id` déjà enregistré est refusé (`test_device_manager_register_reject_duplicate`).
- [x] `unregister_device()` sur un id déjà retiré échoue proprement.

## Priorités pour compléter ce plan

1. Automatiser les cas `[ ]` ci-dessus dans `hal_smoke_test.c` (tous
   réalisables dès aujourd'hui, en hébergé, sans QEMU).
2. Un test par port dédié aux branches `AFROS_ERROR_NOT_SUPPORTED` de
   `port-mcu` (actuellement seulement vérifiées par lecture du code).
3. Harnais bare-metal pour tester `port-mcu` réellement (semihosting QEMU).
