# Changelog

All notable changes to afros-corebridge-core are documented here.
Format based on Keep a Changelog (https://keepachangelog.com/en/1.1.0/),
adheres to Semantic Versioning (https://semver.org/spec/v2.0.0.html).

## [Unreleased]

_Pas de changement en attente de release._

---

## [1.0.0] - 2026-08-11

### Added

- Initial public API freeze.
- **Loader API** (Tier 1 Stable) :
  `AppDetect`, `AppDetectBuffer`, `FormatAnalyze`, `IntelligentLoad`,
  `ResolveDeps`, `DepListFree`, `LoaderGetOps`. Headers :
  `include/loader.h`. Sources : `loader/app_detector.c`,
  `loader/format_analyzer.c`, `loader/dependency_resolver.c`,
  `loader/intelligent_loader.c`.
- **Runtime Manager API** (Tier 1 Stable) :
  `runtime_ops_t` vtable (`initialize`, `load_app`, `start_app`,
  `stop_app`, `get_status`) + les 5 per-runtime lifecycle sets
  (`{Linux,Win,Android,Ios,Harmony}Runtime{Init,Spawn,Signal,Wait,Shutdown}`
  + `{Linux,Win,Android,Ios,Harmony}RuntimeOps`). Header :
  `include/runtime_manager.h`.
- **Unified Execution API** (Tier 1 Stable) :
  - VFS : `VfsCreateView`, `VfsOpen`, `VfsRead`, `VfsWrite`,
    `VfsClose`, `VfsDestroyView`, `WinPathToUnix`, `UnixPathToWin`,
    `IOSPathToUnix`. Source : `unified_execution/filesystem_view.c`.
  - Address Space : `AsReserve`, `AsMap`, `AsUnmap`, `AsShare`,
    `AsRegionCount`, `AsRegionTotalBytes`. Source :
    `unified_execution/address_space.c`.
  - Network : `NetCreateNamespace`, `NetAttach`, `NetForwardPort`,
    `NetCancelForward`, `NetGetStats`, `NetStackInit`,
    `NetStackShutdown`. Source : `unified_execution/network_stack.c`.
  - Resource Manager : `ResSetQuota`, `ResGetUsage`, `ResThrottle`,
    `ResRelease`, `ResIsThrottled`, `ResStartMonitor`,
    `ResStopMonitor`. Source : `unified_execution/resource_manager.c`.
- **Version Management API** (Tier 2 Beta) :
  `VersionRegister`, `VersionUnregister`, `VersionList`,
  `VersionGetDefault`, `VersionSetDefault`, `UpdateCheck`,
  `UpdateCheckAll`, `DownloaderFetch`, `InstallerInstall`,
  `InstallerRollback`, `VersionMgmtGetOps`. Header :
  `include/version_mgmt.h`.
- **Selection & Monitoring API** (Tier 2 Beta) :
  `SelectRuntime`, `MonitorStart`, `MonitorStop`, `MonitorRegister`,
  `MonitorUnregister`, `MonitorHeartbeat`, `MonitorGetStats`,
  `MonitorWatchdog`. Sources : `src/selection_engine.c`,
  `src/monitoring.c`.
- **High-level entry points** (Tier 1 Stable) :
  `orchestrator_init`, `orchestrator_run_app(path, args)`,
  `orchestrator_shutdown`, `afros_corebridge_api_version`. Header :
  `include/orchestrator.h` + umbrella `include/afros_corebridge.h`.
  Source : `src/api_version.c`.
- **Umbrella header** `include/afros_corebridge.h` : inclut tous les
  headers publics et définit `AFROS_COREBRIDGE_API_VERSION` +
  macros `MAJOR/MINOR/PATCH`.
- **Error codes** : `AFROS_CB_SUCCESS` et 7 codes négatifs
  (`AFROS_CB_ERR_INVALID_ARG`, `_NOT_FOUND`, `_NO_RUNTIME`,
  `_OUT_OF_MEMORY`, `_RUNTIME_CRASHED`, `_TIMEOUT`,
  `_PERMISSION_DENIED`).
- **Documentation API** : `API.md` (specification complète, 14
  sections + exemples).
- **Processus RFC** : `RFC-PROCESS.md` + `rfcs/0000-template.md` +
  `rfcs/0001-stabilize-loader-api.md` (RFC d'exemple).

### Changed

- `orchestrator_run_app` prend désormais un second paramètre `args`
  (chaîne d'arguments séparés par des espaces, ou NULL). La signature
  précédente `orchestrator_run_app(const char *path)` est retirée
  (breaking, justifié par la freeze v1.0.0 — aucun consommateur tiers
  n'existait avant).
- Les implémentations stubs de `orchestrator_init` /
  `orchestrator_run_app` / `orchestrator_monitor_system` qui
  vivaient dans `src/central_manager.c` ont été déplacées vers
  `src/api_version.c`. La nouvelle implémentation wire vraiment sur
  `SelectRuntime` → `IntelligentLoad` → `MonitorStart` (corrige le P2
  flaggé dans le rapport d'analyse : les stubs imprimaient juste un
  message sans lancer le runtime).
- `orchestrator.h` : le retour des 3 entry points passe de
  `afros_status_t` à `int` pour cohérence avec le contrat
  `AFROS_CB_ERR_*` négatif. Les valeurs de retour restent
  compatibles au niveau binaire (`AFROS_SUCCESS == 0`).
- `central_manager.c` ne contient plus que `orchestrator_monitor_system`
  (Tier 2 Beta, non exposé dans l'umbrella).

### Stability Tiers

- **Tier 1 (Stable)** : Loader API, Runtime Manager API, Unified
  Execution API (VFS / Address Space / Network / Resource Manager),
  High-level orchestrator entry points, error codes, core types
  (`app_type_t`, `format_info_t`, `runtime_handle_t`,
  `runtime_type_t`, `version_t`, `loader_ops_t`).
- **Tier 2 (Beta)** : Version Management API, Selection &
  Monitoring API, `quota_t` / `usage_t` structs,
  `version_mgmt_ops_t`, `orchestrator_monitor_system`,
  `Babel Bridge API` (`babel_init`, `babel_translate_api`).
- **Tier 3 (Experimental)** : Pas d'API publique dans cette release.

### Known Limitations

- Pas de support multi-utilisateurs (single-user seulement).
- Pas de sandboxing SELinux (sandboxing via namespaces Linux
  uniquement : `CLONE_NEWNS` + `CLONE_NEWPID` + `CLONE_NEWUSER`).
- Pas de persistance des quotas entre redémarrages (les `ResSetQuota`
  sont lost au `orchestrator_shutdown`).
- Version Management API requiert un accès réseau pour `UpdateCheck`
  et `DownloaderFetch` (timeout 30 s par défaut, non configurable
  en 1.0.0).
- `VfsRead` / `VfsWrite` retournent `ssize_t` (et non `int` comme
  indiqué dans certaines sections de l'API spec) : c'est volontaire
  pour autoriser des lectures > 2 GiB sur 64-bit. Les wrappers
  utilisateur qui castent en `int` peuvent casser sur de gros IO.
- `WinPathToUnix` / `UnixPathToWin` / `IOSPathToUnix` utilisent un
  buffer statique : ne pas appeler depuis plusieurs threads
  simultanément, ne pas stocker le pointeur retourné.
- `AndroidRuntimeSpawnApk` prend `entry` en second argument (classe
  Java principale) — en 1.0.0 ce champ est optionnel (NULL = lire le
  manifest) mais ce comportement pourrait devenir obligatoire en 1.1.
- Le watchdog (`MonitorWatchdog`) tue les runtimes qui ne heartbeat
  pas pendant 5 s (`WATCHDOG_TIMEOUT_MS`). Non configurable en 1.0.0.

### Verification

La commande ci-dessous compile sans erreur ni warning :

```sh
gcc -fsyntax-only -I include -I ../afros-core/Kernel/hal/include src/api_version.c
```

---

## Conventions pour les releases futures

- Toute nouvelle fonction publique doit être documentée dans
  `API.md` **et** ajoutée à `CHANGELOG.md` sous `[Unreleased]`.
- Tout changement de signature d'une fonction Tier 1 déclenche un
  bump `MAJOR` et nécessite une RFC acceptée
  (voir `RFC-PROCESS.md`).
- Tout retrait d'API suit la politique de dépréciation 3-minor
  décrite dans `API.md` §2.2.
- La version courante de l'API est queryable runtime via
  `afros_corebridge_api_version()` et compile-time via
  `AFROS_COREBRIDGE_API_VERSION_MAJOR/MINOR/PATCH`.

---

[Unreleased]: https://github.com/ErdisKodjo/System/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/ErdisKodjo/System/releases/tag/v1.0.0
