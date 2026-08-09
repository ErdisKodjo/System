#!/bin/bash
# Build script for Hybrid Firmware Platform
# Copyright (c) 2026, AfriOS. All rights reserved.
#
# Étape 4 : ce script invoquait auparavant `echo "Done building $ARCH
# (Simulated)"` sans jamais appeler `build` (ligne commentée) — il rapportait
# donc toujours un succès, y compris quand rien n'était réellement compilé.
# Il appelle maintenant réellement `build`, avec :
#   - un seul ARCH par invocation (usage: ./build.sh <X64|AARCH64|RISCV64>,
#     défaut X64) plutôt qu'une boucle sur les trois qui masquait les échecs
#     individuels ;
#   - une vérification explicite du cœur EDK2 (MdePkg/BaseTools) avant de
#     tenter quoi que ce soit, avec un message actionnable plutôt qu'un échec
#     cryptique de `build` — voir docs/architecture_overview.md.

set -e

ARCH="${1:-X64}"
case "$ARCH" in
  X64|AARCH64|RISCV64) ;;
  *)
    echo "Architecture inconnue : $ARCH (attendu : X64, AARCH64 ou RISCV64)" >&2
    exit 1
    ;;
esac

TARGET="DEBUG"
PKG="HybridFirmwarePlatformPkg/HybridFirmwarePlatformPkg.dsc"

echo "--- Building Hybrid Firmware Platform ($ARCH) ---"

if [ ! -f "MdePkg/MdePkg.dec" ] || [ ! -d "BaseTools" ]; then
  echo "ERREUR : cœur EDK2 non vendorisé dans ce dépôt (MdePkg/, BaseTools/ vides)." >&2
  echo "Voir docs/architecture_overview.md, section 'Prérequis manquant' pour" >&2
  echo "la commande de vendoring avant de relancer ce script." >&2
  exit 1
fi

if [ -f edksetup.sh ]; then
  source edksetup.sh
else
  echo "ERREUR : edksetup.sh introuvable — environnement EDK2 non initialisé." >&2
  exit 1
fi

build -a "$ARCH" -t GCC5 -p "$PKG" -b "$TARGET"

echo "Build complete ($ARCH)."
