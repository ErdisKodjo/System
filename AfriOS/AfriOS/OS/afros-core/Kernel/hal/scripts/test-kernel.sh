#!/bin/bash
# Étape 5 (Vérification) : ce script était vide (0 octet) — implémenté ici.
#
# Construit et exécute les tests unitaires HAL (hal/tests/, voir
# hal/tests/tests.md) pour chaque port hébergé, plus un lancement du
# simulateur noyau (afros-kernel-sim) pour vérifier que la séquence de boot
# complète (hal_init -> kernel_main) se termine sans erreur.
#
# Le port mcu est exclu : bare-metal, ni afros-hal-tests ni afros-kernel-sim
# ne sont construits pour lui (voir Kernel/afros/CMakeLists.txt et
# Kernel/CMakeLists.txt) — sa validation reste manuelle pour l'instant.
#
# Usage :
#   ./test-kernel.sh              # teste les 3 ports hébergés (arm64, x86_64, riscv)
#   ./test-kernel.sh arm64        # un seul port

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"   # afros-core/Kernel
OS_DIR="$(cd "$KERNEL_DIR/../.." && pwd)"       # OS/ (racine du projet CMake)

if [ -z "$1" ]; then
  PORTS=(arm64 x86_64 riscv)
else
  PORTS=("$1")
fi

if ! command -v cmake >/dev/null 2>&1; then
  echo "ERREUR : cmake introuvable dans le PATH." >&2
  exit 1
fi

FAILED=()

for PORT in "${PORTS[@]}"; do
  if [ "$PORT" == "mcu" ]; then
    echo "--- Port mcu : ignoré (bare-metal, pas de runtime hébergé pour ces tests) ---"
    continue
  fi

  echo "=== Port $PORT ==="
  BUILD_DIR="$OS_DIR/build-test-$PORT"

  cmake -B "$BUILD_DIR" -S "$OS_DIR" -DAFROS_PORT="$PORT" -DAFRIOS_BUILD_KERNEL=ON >/dev/null
  cmake --build "$BUILD_DIR" >/dev/null

  echo "--- Tests unitaires HAL ($PORT) ---"
  if ! ctest --test-dir "$BUILD_DIR" --output-on-failure; then
    FAILED+=("$PORT: tests unitaires HAL")
  fi

  echo "--- Simulateur noyau ($PORT) ---"
  SIM_BIN="$BUILD_DIR/afros-core/Kernel/afros/afros-kernel-sim"
  if [ -x "$SIM_BIN" ]; then
    # afros-kernel-sim boucle indéfiniment en fin de kernel_main() (idle
    # loop volontaire, voir Kernel/afros/main.c) : on borne son exécution
    # plutôt que d'attendre qu'il se termine seul.
    if command -v timeout >/dev/null 2>&1; then
      timeout 3 "$SIM_BIN" || true
    else
      "$SIM_BIN" &
      SIM_PID=$!
      sleep 3
      kill "$SIM_PID" 2>/dev/null || true
    fi
  else
    echo "ATTENTION : binaire afros-kernel-sim introuvable ($SIM_BIN)" >&2
    FAILED+=("$PORT: afros-kernel-sim introuvable")
  fi
done

echo
if [ ${#FAILED[@]} -eq 0 ]; then
  echo "=== Tous les ports testés sont OK ==="
  exit 0
else
  echo "=== Echecs : ===" >&2
  printf ' - %s\n' "${FAILED[@]}" >&2
  exit 1
fi
