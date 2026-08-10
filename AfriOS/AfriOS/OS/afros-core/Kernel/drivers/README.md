# Kernel/drivers/

Couche de pilotes matériels d'AfriOS, extraite de `Kernel/afros/` pour former
une couche indépendante au même niveau que `afros/` (noyau générique) et
`hal/` (contrats d'abstraction matérielle).

## Règle de dépendance

```
drivers/  →  hal/include/  (jamais l'inverse, jamais d'accès direct au matériel hors HAL)
afros/    →  hal/include/
```

Un pilote implémente les structures d'opérations définies dans
`hal/include/device_abstraction.h` (`device_ops_t`) et s'enregistre auprès du
gestionnaire de périphériques exposé par la HAL
(`device_manager_ops_t.register_device`). Il ne doit contenir aucune adresse
physique, aucun numéro d'IRQ ni registre codé en dur : ces valeurs viennent du
port matériel actif (device tree / ACPI / table fixe), fourni par `hal/` au
démarrage.

## Contenu actuel

- `pci/afros_pci.c` — énumération de bus PCI (implémentation minimale,
  déplacée depuis `afros/drivers/afros_pci.c`).

## Ajouter un pilote

1. Créer `drivers/<famille>/<nom>.c`.
2. Implémenter `device_ops_t` (init/read/write/ioctl) depuis
   `hal/include/device_abstraction.h`.
3. Ajouter le fichier à `drivers/CMakeLists.txt` et, si besoin d'un build
   direct hors CMake, à `afros-core/Makefile`.
