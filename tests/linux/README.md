# Tests de compatibilité Linux (baseline)

Valident le runtime Linux natif d'AfriOS — sans couche de compatibilité.
Sert de **baseline** : si ces tests échouent, le problème est dans le
runtime lui-même, pas dans une couche d'émulation.

## Liste des tests

| Test        | Ce que ça valide                                  |
|-------------|---------------------------------------------------|
| `hello-elf` | ELF natif + printf (canari de la suite)           |
| `fork-exec` | fork + execve(/bin/echo) + waitpid                |

## Build

```bash
make -C hello-elf
make -C fork-exec
```

## Exécution

```bash
python3 ../compat-test-harness.py --platform linux
```

Le harness invoque `afros-launch --runtime=linux <binary>`. Sans
`afros-launch`, vous pouvez aussi exécuter directement :

```bash
./hello-elf/hello-elf
./fork-exec/fork-exec
```
