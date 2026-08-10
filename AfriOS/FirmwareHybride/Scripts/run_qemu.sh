#!/bin/bash
# Run script for Hybrid Firmware Platform in QEMU
# Copyright (c) 2026, AfriOS. All rights reserved.

ARCH=${1:-"X64"}
FIRMWARE="Build/HybridFirmwarePlatform/DEBUG_GCC5/FV/HYBRID_FIRMWARE.fd"

echo "--- Launching Hybrid Firmware in QEMU ($ARCH) ---"

case $ARCH in
  X64)
    qemu-system-x86_64 -bios $FIRMWARE -m 512M -serial stdio
    ;;
  AARCH64)
    qemu-system-aarch64 -M virt -cpu cortex-a57 -bios $FIRMWARE -m 512M -serial stdio
    ;;
  RISCV64)
    qemu-system-riscv64 -M virt -bios $FIRMWARE -m 512M -serial stdio
    ;;
  *)
    echo "Unsupported architecture: $ARCH"
    exit 1
    ;;
esac
