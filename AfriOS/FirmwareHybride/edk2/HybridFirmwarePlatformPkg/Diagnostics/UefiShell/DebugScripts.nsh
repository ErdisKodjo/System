## @file
# AfriOS UEFI Shell Debug Script.
#
# Runs the `afri_*` shell commands and prints a debug summary that can be
# captured from the UEFI shell for offline diagnosis. Intended to be invoked
# from the UEFI shell prompt as:
#
#   fs0:\> DebugScripts.nsh
#
# Copyright (c) 2026, AfriOS. All rights reserved.
##

@echo -off
echo "==============================================================="
echo " AfriOS Hybrid Firmware — Debug Summary"
echo "==============================================================="
echo ""

echo "[1/5] Firmware version"
afri ver
echo ""

echo "[2/5] A/B slot state"
afri slot
echo ""

echo "[3/5] Firmware update capsule"
afri capsule
echo ""

echo "[4/5] PCI enumeration"
afri pci
echo ""

echo "[5/5] Memory test (quick scan)"
afri memtest
echo ""

echo "=============================================================="
echo " Debug summary complete."
echo "=============================================================="
