# Kernel/ports/

Chaque `port-<architecture>/` implémente **exactement** les six tables d'ops
déclarées dans `hal/include/*_abstraction.h` :

| Table | Header | Rôle |
|---|---|---|
| `cpu_ops_t arch_cpu_ops` | `cpu_abstraction.h` | Topologie CPU, fréquence, sleep/wake, migration |
| `memory_ops_t arch_memory_ops` | `memory_abstraction.h` | Allocation, MMU (map/unmap), compression |
| `interrupt_ops_t arch_interrupt_ops` | `interrupt_abstraction.h` | Contrôleur d'interruptions, IPI |
| `timer_ops_t arch_timer_ops` | `timer_abstraction.h` | Timer système, ticks, callbacks |
| `console_ops_t arch_console_ops` | `console_abstraction.h` | Console/UART d'amorçage |
| `storage_ops_t arch_storage_ops` | `storage_abstraction.h` | Stockage bloc |

`device_manager_ops_t arch_device_manager_ops` (`device_abstraction.h`) est
l'exception délibérée : c'est un registre générique (`hal/src/device_manager.c`),
pas une table par port — il ne dépend d'aucune architecture, donc ne doit
**pas** être dupliqué dans `ports/`.

## Ports disponibles

| Port | Cible | Contrôleur IRQ | Timer | Console | Stockage |
|---|---|---|---|---|---|
| `port-arm64` | ARMv8/v9, big.LITTLE | GICv3 | ARM Generic Timer | PL011 | eMMC/UFS |
| `port-x86_64` | x86_64, P/E-core | Local APIC + IOAPIC | TSC-deadline | 16550 | NVMe |
| `port-riscv` | RV64GC, harts | PLIC + CLINT | CLINT mtime / SBI | SBI console | virtio-blk |
| `port-mcu` | Cortex-M, mono-cœur | NVIC | SysTick | USART | SPI NOR |

`port-mcu` n'a pas de MMU ni de second cœur : `memory_ops_t.map/unmap`,
`memory_ops_t.compress/decompress`, `cpu_ops_t.migrate_task` et
`interrupt_ops_t.send_ipi` y renvoient `AFROS_ERROR_NOT_SUPPORTED`
délibérément — ce n'est pas un TODO, c'est une contrainte matérielle réelle
qu'un appelant générique doit savoir gérer.

## Sélection du port

```
cmake -B build -DAFROS_PORT=arm64   # arm64 (défaut) | x86_64 | riscv | mcu
cmake --build build
```

`Kernel/CMakeLists.txt` n'ajoute que `ports/port-${AFROS_PORT}/` ; chaque
sous-dossier expose une cible CMake nommée uniformément `afros-port` (jamais
`afros-port-arm64` etc.) pour que `afros/CMakeLists.txt` puisse s'y lier sans
connaître le port actif. C'est ce qui rend l'ajout d'un port transparent
pour le reste du noyau — voir `docs/porting_guide.md` (FirmwareHybride) pour
la procédure complète d'ajout d'une plateforme.

## Règle absolue

Un fichier `Kernel/afros/**` ou `Kernel/drivers/**` qui a besoin de savoir
"quelle architecture ?" a un problème de conception : cette information ne
doit exister que dans `ports/`. Si un besoin nouveau ne rentre dans aucune
des six tables ci-dessus, c'est qu'il manque une abstraction dans
`hal/include/`, pas une exception dans le code générique.
