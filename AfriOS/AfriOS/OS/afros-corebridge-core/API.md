# AfriOS CoreBridge — Spécification de l'API Publique v1.0.0

**Module :** `afros-corebridge-core`
**Version de l'API :** `1.0.0` (gelée le 2026-08-11)
**Header umbrella :** [`include/afros_corebridge.h`](include/afros_corebridge.h)
**Statut :** Stable (Tier 1) pour les sections 5, 6, 7, 10 — Beta (Tier 2) pour les sections 8 et 9.

---

## 1. Introduction

### 1.1 Rôle de CoreBridge

`afros-corebridge-core` est le point d'entrée unique d'AfriOS pour lancer
des applications multi-runtime. Étant donné un binaire
(PE / ELF / Mach-O / DEX / HAP), CoreBridge :

1. **détecte** son format (`loader/app_detector.c`),
2. **analyse** ses métadonnées (arch, subsystem, interpréteur, etc.)
   (`loader/format_analyzer.c`),
3. **résout** ses dépendances partagées (`loader/dependency_resolver.c`),
4. **choisit** le meilleur runtime enregistré
   (`src/selection_engine.c` + `loader/intelligent_loader.c`),
5. **lance** le runtime correspondant
   (`runtime_managers/{linux,win,android,ios,harmony}_runtime_manager.*`),
6. **expose** une surface d'exécution unifiée (VFS, address space,
   network, resource manager) via `unified_execution/`,
7. **surveille** le runtime lancé (watchdog + quotas CPU/mémoire/IO).

### 1.2 Principes de design

| Principe          | Description |
|-------------------|-------------|
| **Stabilité**     | Toute fonction documentée ici comme Tier 1 est gelée : un breaking change nécessite un bump majeur (`2.0.0`) et une RFC acceptée. |
| **Simplicité**    | Trois fonctions suffisent pour lancer une application : `orchestrator_init`, `orchestrator_run_app`, `orchestrator_shutdown`. Tout le reste est optionnel. |
| **Extensibilité** | Les runtimes tiers s'enregistrent via `runtime_register_manager()` ; les nouveaux formats binaires via la table `loader_ops_t`. |
| **Cohérence**     | Toutes les fonctions retournent `0` (`AFROS_CB_SUCCESS`) en cas de succès ou un entier négatif (`AFROS_CB_ERR_*`) en cas d'échec. |
| **Opaque par défaut** | Les handles (`runtime_handle_t`, `vfs_view_t *`, `as_region_t *`) sont opaques ; leur structure interne n'est pas garantie stable. |

---

## 2. Versioning

L'API suit **Semantic Versioning 2.0.0** (https://semver.org).

```
MAJOR.MINOR.PATCH
   1   . 0  . 0
```

| Bump     | Quand                                                                                              |
|----------|----------------------------------------------------------------------------------------------------|
| `MAJOR`  | Breaking change sur une API Tier 1 (signature, sémantique, valeurs d'enum).                        |
| `MINOR`  | Ajout backward-compatible (nouvelle fonction Tier 1/2/3, nouveau champ en fin de struct Tier 2/3). |
| `PATCH`  | Bug fix, doc, refactor interne sans changement de signature.                                       |

La version courante est obtenue via :

```c
const char *afros_corebridge_api_version(void);  /* "1.0.0" */
```

ou via les macros `AFROS_COREBRIDGE_API_VERSION_MAJOR/MINOR/PATCH`.

### 2.1 Breaking change

Est considéré comme **breaking** :

- renommage ou retrait d'une fonction publique Tier 1,
- changement de signature (ajout/retrait de paramètre, changement de type),
- changement de la sémantique d'un paramètre ou d'une valeur de retour,
- réordonnancement ou retrait d'un enum Tier 1,
- retrait d'un champ d'une struct Tier 1.

### 2.2 Politique de dépréciation

Une fonction Tier 1 peut être marquée **deprecated** mais n'est **jamais
retirée dans la même version majeure**. Le cycle de dépréciation est de
**3 versions mineures** :

1. `v1.X.0` — la fonction est marquée `__attribute__((deprecated))` et la
   doc pointe vers son remplaçant.
2. `v1.X+1.0` — un warning runtime est émis à chaque appel.
3. `v1.X+2.0` — la fonction devient un stub qui retourne
   `AFROS_CB_ERR_DEPRECATED`.
4. `v2.0.0` — la fonction est retirée.

---

## 3. Stability Tiers

| Tier       | Couleur      | Contrat                                                                                       |
|------------|--------------|-----------------------------------------------------------------------------------------------|
| **Tier 1** | Stable       | Gelé. Breaking change ⇒ `MAJOR` bump + RFC.                                                   |
| **Tier 2** | Beta         | Stable à l'intérieur d'une `MINOR` mais peut évoluer entre deux `MINOR` (champ ajouté en fin de struct, signature étendue avec valeur par défaut). Un warning `Beta` est émis à l'utilisation. |
| **Tier 3** | Experimental | Instable. Peut casser à tout moment, même en `PATCH`. À n'utiliser qu'en développement.       |

Aucune API Tier 3 n'est exposée dans la release 1.0.0.

---

## 4. Core Types

Tous les types ci-dessous sont définis dans les headers publics
(`include/*.h`) et sont **Tier 1 Stable** sauf mention contraire.

### 4.1 `app_type_t` — Tier 1

```c
typedef enum {
    APP_TYPE_UNKNOWN = 0,   /* Could not classify                       */
    APP_TYPE_NATIVE,        /* AfriOS native ELF or script              */
    APP_TYPE_LINUX,         /* ELF (\x7FELF)                            */
    APP_TYPE_WINDOWS,       /* PE (MZ)                                  */
    APP_TYPE_MACOS,         /* Mach-O (0xFEEDFACE / 0xFEEDFACF / ...)   */
    APP_TYPE_ANDROID,       /* DEX (dex\n035)                            */
    APP_TYPE_HARMONY        /* HarmonyOS .hap / .hsp (ZIP + module.json)*/
} app_type_t;
```

**Source :** [`include/loader.h`](include/loader.h) (lignes 33-41).
L'ordre des énumérateurs est gelé ; les valeurs numériques font partie
du contrat API.

### 4.2 `format_info_t` — Tier 1

```c
#define MAX_INTERP_LEN     256
#define MAX_DEP_NAME_LEN   256

typedef struct {
    app_type_t  type;                 /* Result of AppDetect             */
    uint32_t    arch;                 /* APP_ARCH_* (see loader.h)       */
    uint32_t    subsystem;            /* PE subsystem (0 for non-PE)     */
    uint32_t    bits;                 /* 32 or 64                        */
    uint32_t    format_version;       /* DEX version, PE machine ver, ...*/
    char        interpreter[MAX_INTERP_LEN];     /* ELF PT_INTERP        */
    char        bundle_id[MAX_DEP_NAME_LEN];     /* macOS / HarmonyOS    */
    char        entry_name[MAX_DEP_NAME_LEN];    /* Main class / entry   */
} format_info_t;
```

**Source :** [`include/loader.h`](include/loader.h) (lignes 65-74).
L'ordre et le type des champs sont gelés. De nouveaux champs ne peuvent
être ajoutés qu'en fin de struct **et** seulement via un bump `MAJOR`.

### 4.3 `runtime_handle_t` — Tier 1

```c
typedef uint32_t runtime_handle_t;
#define INVALID_RUNTIME_HANDLE ((runtime_handle_t)0)
```

Handle opaque retourné par `IntelligentLoad` / `SelectRuntime`.
`0` signifie "handle invalide". La valeur numérique n'a aucun sens
pour l'appelant et peut être réutilisée après `orchestrator_shutdown`.

### 4.4 `runtime_type_t` — Tier 1

```c
typedef enum {
    RUNTIME_TYPE_NATIVE,
    RUNTIME_TYPE_ANDROID,
    RUNTIME_TYPE_LINUX,
    RUNTIME_TYPE_WINBRIDGE,
    RUNTIME_TYPE_IOS,
    RUNTIME_TYPE_HARMONY
} afros_runtime_type_t;
typedef afros_runtime_type_t runtime_type_t;  /* alias */
```

**Source :** [`include/runtime_manager.h`](include/runtime_manager.h) et
[`include/version_mgmt.h`](include/version_mgmt.h) (alias).

### 4.5 `version_t`, `quota_t`, `usage_t` — Tier 1 (version_t), Tier 2 (quota_t, usage_t)

```c
#define MAX_VERSION_STR   64
#define MAX_INSTALL_PATH  512

typedef struct {
    runtime_type_t type;
    char           version[MAX_VERSION_STR];        /* "9.0"             */
    char           install_path[MAX_INSTALL_PATH];  /* /opt/afros/wine/9.0 */
    int            is_default;                      /* 1 if default for type */
} version_t;

typedef struct {
    uint32_t cpu_weight;        /* 1..1000 (cgroup-style shares)         */
    uint64_t mem_limit_bytes;   /* Max RSS in bytes (0 = unlimited)      */
    uint64_t io_quota_kbps;     /* Max read+write KB/s                   */
    uint32_t fd_limit;          /* Max open file descriptors             */
    uint32_t port_limit;        /* Max bound TCP/UDP ports               */
} quota_t;

typedef struct {
    uint32_t cpu_percent;
    uint64_t mem_used_bytes;
    uint64_t io_read_kb;
    uint64_t io_write_kb;
    uint32_t fd_count;
    uint32_t port_count;
    uint64_t faults;
} usage_t;
```

`quota_t` et `usage_t` sont **Tier 2** : des champs pourront être ajoutés
en fin de struct dans une prochaine `MINOR`.

### 4.6 `loader_ops_t`, `version_mgmt_ops_t` — Tier 1 / Tier 2

```c
typedef struct {
    app_type_t        (*detect)(const char *path);
    format_info_t     (*analyze)(const char *path, app_type_t type);
    dep_list_t       *(*resolve_deps)(const char *path, app_type_t type);
    runtime_handle_t  (*load)(const char *path, const char *args);
} loader_ops_t;

typedef struct {
    afros_status_t (*register_ver)(const version_t *);
    afros_status_t (*unregister)(runtime_type_t, const char *);
    uint32_t       (*list)(version_t *, uint32_t);
    afros_status_t (*get_default)(runtime_type_t, version_t *);
    afros_status_t (*set_default)(runtime_type_t, const char *);
    afros_status_t (*check)(runtime_type_t, version_t *);
    afros_status_t (*fetch)(const char *, const char *, const char *);
    afros_status_t (*install)(const char *);
    afros_status_t (*rollback)(runtime_type_t);
} version_mgmt_ops_t;
```

`loader_ops_t` est Tier 1. `version_mgmt_ops_t` est Tier 2.

---

## 5. Loader API — Tier 1 Stable

**Header :** [`include/loader.h`](include/loader.h)
**Sources :** `loader/app_detector.c`, `loader/format_analyzer.c`,
`loader/dependency_resolver.c`, `loader/intelligent_loader.c`.

```c
app_type_t      AppDetect(const char *path);
app_type_t      AppDetectBuffer(const void *buf, size_t len);
format_info_t   FormatAnalyze(const char *path, app_type_t type);
dep_list_t     *ResolveDeps(const char *path, app_type_t type);
void            DepListFree(dep_list_t *list);
runtime_handle_t IntelligentLoad(const char *path, const char *args);
const loader_ops_t *LoaderGetOps(void);
```

| Fonction            | Sémantique                                                                                 |
|---------------------|--------------------------------------------------------------------------------------------|
| `AppDetect`         | Lit les magic bytes du fichier `path`, retourne le type détecté.                           |
| `AppDetectBuffer`   | Idem mais depuis un buffer mémoire (utile pour les streams réseau).                        |
| `FormatAnalyze`     | Deep-inspect : arch, bits, subsystem PE, interpréteur ELF, bundle_id, entry_name.          |
| `ResolveDeps`       | Retourne une `dep_list_t` heap-allocée (max 64 entrées). **Caller frees via `DepListFree`.** |
| `IntelligentLoad`   | Decision engine : detect + analyze + budget + score, retourne un `runtime_handle_t`. Cache la décision pour les appels suivants. |
| `LoaderGetOps`      | Retourne la table `loader_ops_t` statique (utile pour les plugins).                        |

**Exemple :**
```c
app_type_t t = AppDetect("/usr/bin/notepad.exe");
if (t == APP_TYPE_WINDOWS) {
    format_info_t fi = FormatAnalyze("/usr/bin/notepad.exe", t);
    printf("arch=%u subsystem=%u\n", fi.arch, fi.subsystem);
}
```

---

## 6. Runtime Manager API — Tier 1 Stable

**Header :** [`include/runtime_manager.h`](include/runtime_manager.h)
**Sources :** `src/runtime_registry.c`, `runtime_managers/*`.

### 6.1 Vtable

```c
typedef struct {
    afros_status_t (*initialize)(void);
    afros_status_t (*load_app)(const char *path);
    afros_status_t (*start_app)(const char *name);
    afros_status_t (*stop_app)(const char *name);
    afros_status_t (*get_status)(const char *name, uint32_t *status);
} runtime_ops_t;
```

Chaque runtime manager expose :

```c
const runtime_ops_t *LinuxRuntimeOps(void);
const runtime_ops_t *WinRuntimeOps(void);
const runtime_ops_t *AndroidRuntimeOps(void);
const runtime_ops_t *IosRuntimeOps(void);
const runtime_ops_t *HarmonyRuntimeOps(void);
```

### 6.2 Per-runtime lifecycle (Tier 1)

Au-delà de la vtable générique, chaque runtime expose des fonctions
spécialisées Tier 1 :

| Runtime  | Init                      | Spawn                                     | Signal                    | Wait                     | Shutdown                    |
|----------|---------------------------|-------------------------------------------|---------------------------|--------------------------|-----------------------------|
| Linux    | `LinuxRuntimeInit`        | `LinuxRuntimeSpawn(path,args,&pid)`       | `LinuxRuntimeSignal(pid,s)` | `LinuxRuntimeWait(pid,&st)` | `LinuxRuntimeShutdown`      |
| Windows  | `WinRuntimeInit`          | `WinRuntimeSpawn(path,args,&pid)`         | `WinRuntimeSignal`        | `WinRuntimeWait`         | `WinRuntimeShutdown`        |
| Android  | `AndroidRuntimeInit`      | `AndroidRuntimeSpawnApk(apk,entry,&pid)`  | `AndroidRuntimeSignal`    | `AndroidRuntimeWait`     | `AndroidRuntimeShutdown`    |
| iOS      | `IosRuntimeInit`          | `IosRuntimeSpawnApp(bundle,entry,&pid)`   | `IosRuntimeSignal`        | `IosRuntimeWait`         | `IosRuntimeShutdown`        |
| Harmony  | `HarmonyRuntimeInit`      | `HarmonyRuntimeSpawnHap(hap,entry,&pid)`  | `HarmonyRuntimeSignal`    | `HarmonyRuntimeWait`     | `HarmonyRuntimeShutdown`    |

**Sources :** `runtime_managers/linux_runtime_manager.c`,
`runtime_managers/win_runtime_manager.c`,
`runtime_managers/android_runtime_manager.cpp`,
`runtime_managers/ios_runtime_manager.cpp`,
`runtime_managers/harmony_runtime_manager.c`.

### 6.3 Registration

```c
afros_status_t runtime_register_manager(afros_runtime_type_t type,
                                        runtime_ops_t *ops);
afros_status_t runtime_init(void);
```

`runtime_init()` enregistre les 5 runtime managers ci-dessus. À appeler
une seule fois (idempotent).

---

## 7. Unified Execution API — Tier 1 Stable

**Sources :** `unified_execution/filesystem_view.c`,
`unified_execution/address_space.c`, `unified_execution/network_stack.c`,
`unified_execution/resource_manager.c`.

### 7.1 VFS

```c
typedef uint32_t runtime_mask_t;
#define RMASK_NATIVE   (1u << RUNTIME_TYPE_NATIVE)
#define RMASK_LINUX    (1u << RUNTIME_TYPE_LINUX)
#define RMASK_WIN      (1u << RUNTIME_TYPE_WINBRIDGE)
#define RMASK_ANDROID  (1u << RUNTIME_TYPE_ANDROID)
#define RMASK_IOS      (1u << RUNTIME_TYPE_IOS)
#define RMASK_HARMONY  (1u << RUNTIME_TYPE_HARMONY)
#define RMASK_ALL      0xFFFFFFFFu

typedef struct { /* opaque */ } vfs_view_t;

vfs_view_t *VfsCreateView(runtime_mask_t mask);
int         VfsOpen(vfs_view_t *v, const char *path, int flags, int mode);
ssize_t     VfsRead (vfs_view_t *v, int handle, void *buf, size_t len);
ssize_t     VfsWrite(vfs_view_t *v, int handle, const void *buf, size_t len);
int         VfsClose(vfs_view_t *v, int handle);
void        VfsDestroyView(vfs_view_t *v);

const char *WinPathToUnix(const char *win);
const char *UnixPathToWin(const char *unix_path);
const char *IOSPathToUnix(const char *ios);
```

`VfsOpen` retourne un handle opaque `> 0` (slot+1) ou `-1` sur erreur
(errno positionné). Le paramètre `mode` n'est utilisé que si `O_CREAT`
est dans `flags`.

### 7.2 Address Space

```c
typedef struct { /* opaque */ } as_region_t;

as_region_t  *AsReserve(runtime_handle_t rt, void *hint, size_t len, int prot);
as_region_t  *AsMap   (runtime_handle_t rt, void *addr, size_t len,
                       int prot, int flags, int fd, off_t off);
afros_status_t AsUnmap(runtime_handle_t rt, void *addr, size_t len);
as_region_t  *AsShare (runtime_handle_t src, runtime_handle_t dst,
                       void *addr, size_t len);
uint32_t      AsRegionCount     (runtime_handle_t rt);
uint64_t      AsRegionTotalBytes(runtime_handle_t rt);
```

`AsReserve` réserve une région anonyme. `AsShare` partage une région
entre deux runtimes (MAP_SHARED → même mapping ; MAP_PRIVATE → copy).

### 7.3 Network

```c
int           NetCreateNamespace(runtime_handle_t rt, const char *name);
afros_status_t NetAttach(runtime_handle_t rt, int ns_fd);
afros_status_t NetForwardPort(int host_port, runtime_handle_t rt,
                              int rt_port, int proto);
afros_status_t NetCancelForward(int host_port);
afros_status_t NetGetStats(runtime_handle_t rt, void *out); /* net_stats_t */
afros_status_t NetStackInit(void);
afros_status_t NetStackShutdown(void);
```

`NetCreateNamespace` retourne un slot `>= 0` (le "ns_fd") ou `-1`.
`proto` est `IPPROTO_TCP` ou `IPPROTO_UDP`.

### 7.4 Resource Manager

```c
afros_status_t ResSetQuota (runtime_handle_t rt, const quota_t *q);
afros_status_t ResGetUsage (runtime_handle_t rt, usage_t *out);
afros_status_t ResThrottle (runtime_handle_t rt);
afros_status_t ResRelease  (runtime_handle_t rt);
int            ResIsThrottled(runtime_handle_t rt);
afros_status_t ResStartMonitor(void);
afros_status_t ResStopMonitor (void);
```

---

## 8. Version Management API — Tier 2 Beta

**Header :** [`include/version_mgmt.h`](include/version_mgmt.h)
**Sources :** `version_management/*`.

```c
afros_status_t VersionRegister(const version_t *v);
afros_status_t VersionUnregister(runtime_type_t type, const char *version);
uint32_t       VersionList(version_t *out, uint32_t max);
afros_status_t VersionGetDefault(runtime_type_t type, version_t *out);
afros_status_t VersionSetDefault(runtime_type_t type, const char *version);

afros_status_t UpdateCheck   (runtime_type_t rt, version_t *latest);
uint32_t       UpdateCheckAll(version_t *updates, uint32_t max);

afros_status_t DownloaderFetch(const char *url, const char *dest_path,
                               const char *expected_sha256);

afros_status_t InstallerInstall (const char *archive_path);
afros_status_t InstallerRollback(runtime_type_t rt);

const version_mgmt_ops_t *VersionMgmtGetOps(void);
```

**Tier 2** : ces signatures sont stables à l'intérieur de la `MINOR`
1.0.x mais pourront être étendues en 1.1.0 (par exemple ajout d'un
paramètre `flags` à `InstallerInstall` avec une valeur par défaut de `0`
pour préserver la compat binaire).

`UpdateCheck` et `DownloaderFetch` nécessitent un accès réseau
(`AFROS_CB_ERR_TIMEOUT` si le serveur ne répond pas dans les 30 s).

---

## 9. Selection & Monitoring API — Tier 2 Beta

**Sources :** `src/selection_engine.c`, `src/monitoring.c`.

```c
runtime_handle_t SelectRuntime(const char *path, const char *args);

afros_status_t MonitorStart(void);
afros_status_t MonitorStop (void);
afros_status_t MonitorRegister  (runtime_handle_t rt, pid_t pid);
afros_status_t MonitorUnregister(runtime_handle_t rt);
afros_status_t MonitorHeartbeat (runtime_handle_t rt);
afros_status_t MonitorGetStats  (runtime_handle_t rt, uint64_t *cpu_ms,
                                 uint64_t *mem_bytes, uint32_t *faults,
                                 int *alive);
afros_status_t MonitorWatchdog(void);
```

`SelectRuntime` est appelé par `orchestrator_run_app` ; il est exposé
Tier 2 pour les outils avancés (runtime-selector CLI) qui veulent
inspecter le choix sans réellement lancer.

`MonitorGetStats` retourne l'instantané CPU/mémoire/faults d'un runtime.
Le contrat est gelé pour ces 4 paramètres de sortie ; un cinquième
champ `io_bytes` pourrait être ajouté en 1.1.0 sans casser la compat.

---

## 10. High-level orchestrator entry point — Tier 1 Stable

**Header :** [`include/orchestrator.h`](include/orchestrator.h)
**Source :** [`src/api_version.c`](src/api_version.c).

```c
int orchestrator_init(void);
int orchestrator_run_app(const char *path, const char *args);
int orchestrator_shutdown(void);
const char *afros_corebridge_api_version(void);
```

| Fonction                     | Effet                                                                              |
|------------------------------|------------------------------------------------------------------------------------|
| `orchestrator_init`          | Idempotent. Initialise tous les runtime managers, le réseau, le sampler, le watchdog. |
| `orchestrator_run_app`       | Lance une application : `SelectRuntime` → spawn → `MonitorRegister`.               |
| `orchestrator_shutdown`      | Arrête threads, libère ressources, remet la librairie à zéro.                      |
| `afros_corebridge_api_version` | Retourne `"1.0.0"`.                                                              |

**Contrat** : un programme utilisateur n'a besoin que de ces 4 fonctions.
Tout le reste (Loader, Runtime Manager, VFS, etc.) est exposé pour les
cas d'usage avancés.

**Wiring interne** (corrige le P2 du rapport d'analyse) :

```
orchestrator_init()
  ├─ runtime_init()
  ├─ {Linux,Win,Android,Ios,Harmony}RuntimeInit()
  ├─ NetStackInit()
  ├─ ResStartMonitor()
  └─ MonitorStart()

orchestrator_run_app(path, args)
  ├─ SelectRuntime(path, args)
  │    └─ IntelligentLoad(path, args)
  │         ├─ AppDetect(path)
  │         ├─ FormatAnalyze(path, type)
  │         └─ cache_insert(path, handle, type)
  ├─ LoaderCachedType(path) -> type
  ├─ <type>RuntimeSpawn(path, args, &pid)
  └─ MonitorRegister(handle, pid)

orchestrator_shutdown()
  ├─ MonitorStop()
  ├─ ResStopMonitor()
  ├─ {Linux,Win,Android,Ios,Harmony}RuntimeShutdown()
  └─ NetStackShutdown()
```

---

## 11. Error codes

Définis dans [`include/afros_corebridge.h`](include/afros_corebridge.h) :

| Constante                       | Valeur | Signification                                            |
|---------------------------------|--------|----------------------------------------------------------|
| `AFROS_CB_SUCCESS`              | `0`    | OK.                                                      |
| `AFROS_CB_ERR_INVALID_ARG`      | `-1`   | Argument NULL ou invalide.                               |
| `AFROS_CB_ERR_NOT_FOUND`        | `-2`   | Fichier / runtime / version introuvable.                 |
| `AFROS_CB_ERR_NO_RUNTIME`       | `-3`   | Aucun runtime compatible enregistré.                     |
| `AFROS_CB_ERR_OUT_OF_MEMORY`    | `-4`   | Allocation échouée ou quota de slots atteint.            |
| `AFROS_CB_ERR_RUNTIME_CRASHED`  | `-5`   | Le runtime s'est terminé anormalement.                   |
| `AFROS_CB_ERR_TIMEOUT`          | `-6`   | Délai dépassé (update check, watchdog, etc.).            |
| `AFROS_CB_ERR_PERMISSION_DENIED`| `-7`   | Permission refusée (sandbox, namespace, file mode).      |

Les fonctions Tier 1 retournent un `int` (ou `afros_status_t` qui est
`uint32_t` pour la compat avec le noyau). Un retour `0` est toujours un
succès ; tout retour négatif est une erreur appartenant à la liste
ci-dessus. Les codes d'erreur font partie du contrat API et ne peuvent
être renumérotés sans bump `MAJOR`.

---

## 12. Thread safety

| Fonction / groupe                | Réentrance                                                                 |
|----------------------------------|----------------------------------------------------------------------------|
| `orchestrator_init`              | Thread-safe (idempotent, mais non atomique — ne pas appeler depuis 2 threads en parallèle). |
| `orchestrator_run_app`           | **Non** thread-safe par défaut. Si vous lancez plusieurs apps en parallèle, sérialisez avec un mutex externe. |
| `orchestrator_shutdown`          | Thread-safe.                                                               |
| `AppDetect`, `AppDetectBuffer`   | Réentrante (lecture seule).                                                |
| `FormatAnalyze`                  | Réentrante.                                                                |
| `ResolveDeps`                    | Réentrante ; chaque appel retourne une `dep_list_t` distincte.             |
| `IntelligentLoad`, `SelectRuntime` | Verrou interne (cache). Peut être appelée depuis plusieurs threads.     |
| `VfsCreateView` / `VfsOpen` etc. | **Non** thread-safe au niveau d'une même `vfs_view_t *`. Une vue par thread, ou mutex externe. |
| `AsReserve` / `AsMap` / `AsShare` / `AsUnmap` | Thread-safe (mutex interne).                                  |
| `NetCreateNamespace` / `NetForwardPort` | Thread-safe (mutex interne).                                       |
| `ResSetQuota` / `ResGetUsage`    | Thread-safe (mutex interne). Le sampler tourne dans un thread dédié.       |
| `MonitorRegister` / `MonitorHeartbeat` / `MonitorGetStats` | Thread-safe (mutex interne).                          |
| `VersionRegister` / `VersionList` / `VersionSetDefault` | Thread-safe (verrou interne du registry).              |
| `UpdateCheck` / `DownloaderFetch` | Réentrantes mais **lentes** (réseau). Ne pas appeler depuis le thread UI. |

En résumé : tout ce qui prend un handle en écriture (`vfs_view_t *`,
`runtime_handle_t` pour spawn/signal/wait) nécessite un lock externe si
partagé entre threads.

---

## 13. Memory ownership

| Ressource                  | Allouée par       | Libérée par                              |
|----------------------------|-------------------|------------------------------------------|
| `dep_list_t *`             | `ResolveDeps`     | `DepListFree(list)`                      |
| `vfs_view_t *`             | `VfsCreateView`   | `VfsDestroyView(v)`                      |
| `as_region_t *`            | `AsReserve`/`AsMap`/`AsShare` | `AsUnmap` (le pointeur redevient invalide) |
| `version_t *out`           | Caller            | Caller (stack ou heap)                   |
| `usage_t *out`             | Caller            | Caller                                   |
| `quota_t *q`               | Caller            | Caller (la fonction copie)               |
| Buffer passé à `VfsRead`/`VfsWrite` | Caller    | Caller                                   |
| Chaîne retournée par `afros_corebridge_api_version` | Library (const) | Jamais (pointeur statique)      |
| Chaînes retournées par `WinPathToUnix` etc. | Library (static buffer) | Jamais — buffer écrasé au prochain appel. Ne pas stocker. |

**Règle générale** : si la fonction retourne un pointeur et que la
signature ne dit pas `const`, l'appelant doit le libérer. Les buffers
`const char *` retournés par les helpers de path appartiennent à la
library et sont éphémères.

---

## 14. Exemples de code

### 14.1 Lancer `notepad.exe`

```c
/* notepad.c — compile: gcc notepad.c -lafros-corebridge */
#include "afros_corebridge.h"
#include <stdio.h>

int main(void)
{
    int rc;

    rc = orchestrator_init();
    if (rc != AFROS_CB_SUCCESS) {
        fprintf(stderr, "init failed: %d\n", rc);
        return 1;
    }

    rc = orchestrator_run_app("/opt/afros/winapps/notepad.exe",
                              /*args=*/ NULL);
    if (rc != AFROS_CB_SUCCESS) {
        fprintf(stderr, "run_app failed: %d\n", rc);
        orchestrator_shutdown();
        return 1;
    }

    /* L'app tourne. À la fin : */
    orchestrator_shutdown();
    return 0;
}
```

### 14.2 Partager un fichier entre Wine et Android

```c
/* share_file.c — Wine écrit /Z:/shared/data.txt, Android lit /sdcard/shared/data.txt */
#include "afros_corebridge.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    runtime_mask_t mask = RMASK_WIN | RMASK_ANDROID;
    vfs_view_t *v;
    int fd_win, fd_andr;
    const char *data = "Hello from Wine!\n";
    char buf[64];
    ssize_t n;

    orchestrator_init();

    /* 1. Créer une vue VFS qui voit Wine + Android. */
    v = VfsCreateView(mask);
    if (!v) { fprintf(stderr, "VfsCreateView failed\n"); return 1; }

    /* 2. Wine écrit dans /wine/c/shared/data.txt
     *    (vu côté Wine comme C:\shared\data.txt). */
    fd_win = VfsOpen(v, "/wine/c/shared/data.txt",
                     O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd_win < 0) { perror("open win"); return 1; }
    VfsWrite(v, fd_win, data, strlen(data));
    VfsClose(v, fd_win);

    /* 3. Android lit depuis /android/sdcard/shared/data.txt. */
    fd_andr = VfsOpen(v, "/android/sdcard/shared/data.txt", O_RDONLY, 0);
    if (fd_andr < 0) { perror("open android"); return 1; }
    n = VfsRead(v, fd_andr, buf, sizeof(buf) - 1);
    if (n > 0) { buf[n] = '\0'; printf("Android received: %s", buf); }
    VfsClose(v, fd_andr);

    VfsDestroyView(v);
    orchestrator_shutdown();
    return 0;
}
```

### 14.3 Surveiller un runtime

```c
/* monitor.c — lance un .hap HarmonyOS et surveille sa conso CPU/mémoire. */
#include "afros_corebridge.h"
#include <stdio.h>
#include <unistd.h>

int main(void)
{
    runtime_handle_t h;
    uint64_t cpu_ms, mem_bytes;
    uint32_t faults;
    int alive;

    orchestrator_init();

    /* Lance via le high-level entry point : le handle est alloué
     * par IntelligentLoad et le pid est enregistré auprès du monitor. */
    if (orchestrator_run_app("/opt/afros/hap/hello.hap", NULL) != 0) {
        fprintf(stderr, "run_app failed\n");
        return 1;
    }

    /* Récupère le handle (le dernier lancé). En production on stockerait
     * le handle retourné par IntelligentLoad directement. */
    h = IntelligentLoad("/opt/afros/hap/hello.hap", NULL);

    for (int i = 0; i < 10; i++) {
        sleep(1);
        if (MonitorGetStats(h, &cpu_ms, &mem_bytes, &faults, &alive) == 0) {
            printf("cpu=%lums mem=%luB faults=%u alive=%d\n",
                   (unsigned long)cpu_ms, (unsigned long)mem_bytes,
                   faults, alive);
        }
        if (!alive) { printf("runtime exited\n"); break; }
    }

    orchestrator_shutdown();
    return 0;
}
```

---

## 15. Vérification

La commande suivante doit compiler sans erreur ni warning :

```sh
gcc -fsyntax-only -I include -I ../afros-core/Kernel/hal/include src/api_version.c
```

Elle valide que :

- l'umbrella header `afros_corebridge.h` inclut correctement tous les
  sous-headers,
- les forward declarations des sous-systèmes (SelectRuntime,
  MonitorStart, NetStackInit, ResStartMonitor, etc.) sont cohérentes
  avec les implémentations,
- les signatures Tier 1 de `orchestrator_init` / `run_app` / `shutdown`
  correspondent entre le header `orchestrator.h` et l'umbrella.

---

## 16. Références

- Headers : [`include/`](include/)
- Sources entry points : [`src/api_version.c`](src/api_version.c),
  [`src/central_manager.c`](src/central_manager.c)
- Loader : [`loader/`](loader/)
- Runtime managers : [`runtime_managers/`](runtime_managers/)
- Unified execution : [`unified_execution/`](unified_execution/)
- Version management : [`version_management/`](version_management/)
- Changelog : [`CHANGELOG.md`](CHANGELOG.md)
- Processus RFC : [`RFC-PROCESS.md`](RFC-PROCESS.md)
- RFC template : [`rfcs/0000-template.md`](rfcs/0000-template.md)
- RFC exemple : [`rfcs/0001-stabilize-loader-api.md`](rfcs/0001-stabilize-loader-api.md)

*Document généré pour la release 1.0.0 du 2026-08-11. Toute modification
doit suivre le processus RFC décrit dans `RFC-PROCESS.md`.*
