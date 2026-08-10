## @file
# Hybrid Firmware Platform Description
#
# This file describes the build configuration for the Hybrid Firmware.
#
# Copyright (c) 2026, AfriOS. All rights reserved.
##

[Defines]
  PLATFORM_NAME                  = HybridFirmwarePlatform
  PLATFORM_GUID                  = 7F1E2C0D-5A4B-3C2D-1E0F-9A8B7C6D5E4F
  PLATFORM_VERSION               = 0.1
  DSC_SPECIFICATION              = 0x00010005
  OUTPUT_DIRECTORY               = Build/HybridFirmwarePlatform
  SUPPORTED_ARCHITECTURES        = IA32|X64|AARCH64|RISCV64
  BUILD_TARGETS                  = DEBUG|RELEASE|NOOPT
  SKUID_IDENTIFIER               = DEFAULT

[LibraryClasses]
  # Standard EDK2 Library Classes (Pointers to other packages)
  BaseLib|MdePkg/Library/BaseLib/BaseLib.inf
  BaseMemoryLib|MdePkg/Library/BaseMemoryLib/BaseMemoryLib.inf
  PrintLib|MdePkg/Library/BasePrintLib/BasePrintLib.inf
  TimerLib|HybridFirmwarePlatformPkg/Libraries/TimerLib/TimerLib.inf
  PcdLib|MdePkg/Library/BasePcdLibNull/BasePcdLibNull.inf
  DebugLib|MdePkg/Library/BaseDebugLibNull/BaseDebugLibNull.inf
  UefiLib|MdeModulePkg/Library/UefiLib/UefiLib.inf
  UefiDriverEntryPoint|MdePkg/Library/UefiDriverEntryPoint/UefiDriverEntryPoint.inf
  UefiBootServicesTableLib|MdePkg/Library/UefiBootServicesTableLib/UefiBootServicesTableLib.inf
  UefiRuntimeServicesTableLib|MdePkg/Library/UefiRuntimeServicesTableLib/UefiRuntimeServicesTableLib.inf
  DevicePathLib|MdePkg/Library/UefiDevicePathLib/UefiDevicePathLib.inf
  MemoryAllocationLib|MdePkg/Library/UefiMemoryAllocationLib/UefiMemoryAllocationLib.inf

  # Custom Library Classes
  CpuHalLib|HybridFirmwarePlatformPkg/HardwareAbstractionLayer/CpuHal/CpuHalLib.inf
  PlatformDetectLib|HybridFirmwarePlatformPkg/HardwareAbstractionLayer/PlatformDetect/PlatformDetectLib.inf
  MeasuredBootLib|HybridFirmwarePlatformPkg/Security/MeasuredBoot/MeasuredBootLib.inf

  # Library classes introduced by Agent FW (étape firmware-completion) :
  # un parser plist minimaliste pour AppleBoot, un gestionnaire A/B slot, et
  # un module de passthrough EPT/Stage-2 pour le minimal hypervisor.
  ConfigPlistParserLib|HybridFirmwarePlatformPkg/BootManager/AppleBoot/ConfigPlistParser.inf
  AbSlotManagerLib|HybridFirmwarePlatformPkg/OtaUpdate/ABSlotManager.inf
  PassthroughLib|HybridFirmwarePlatformPkg/ShimLayer/MinimalHypervisor/Passthrough.inf

[LibraryClasses.common.SEC]
  ExtractGuidedSectionLib|MdePkg/Library/BaseExtractGuidedSectionLib/BaseExtractGuidedSectionLib.inf

[LibraryClasses.common.PEIM]
  PeimEntryPoint|MdePkg/Library/PeimEntryPoint/PeimEntryPoint.inf
  PeiServicesLib|MdePkg/Library/PeiServicesLib/PeiServicesLib.inf
  PeiServicesTablePointerLib|MdePkg/Library/PeiServicesTablePointerLib/PeiServicesTablePointerLib.inf
  HobLib|MdePkg/Library/PeiHobLib/PeiHobLib.inf

[LibraryClasses.common.DXE_DRIVER]
  DxeServicesLib|MdePkg/Library/DxeServicesLib/DxeServicesLib.inf
  DxeServicesTableLib|MdePkg/Library/DxeServicesTableLib/DxeServicesTableLib.inf
  # Étape 4 : nécessaire pour que FdtPlatformDxe relise le HOB publié par
  # PlatformInfoPei (GetFirstGuidHob).
  HobLib|MdePkg/Library/DxeHobLib/DxeHobLib.inf

[Components]
  # Platform Initialization
  HybridFirmwarePlatformPkg/PlatformInit/Pei/PlatformInfoPei.inf
  HybridFirmwarePlatformPkg/PlatformInit/Pei/MemoryInit.inf
  HybridFirmwarePlatformPkg/PlatformInit/Dxe/PlatformInitDxe.inf
  HybridFirmwarePlatformPkg/PlatformInit/Dxe/AcpiPlatformDxe.inf

  # Hardware Abstraction Layer
  HybridFirmwarePlatformPkg/HardwareAbstractionLayer/Acpi/AcpiTableGenerator.inf
  HybridFirmwarePlatformPkg/HardwareAbstractionLayer/DeviceTree/FdtPlatformDxe.inf
  HybridFirmwarePlatformPkg/HardwareAbstractionLayer/Smbios/SmbiosGenerator.inf
  HybridFirmwarePlatformPkg/HardwareAbstractionLayer/CpuHal/CpuHalLib.inf

  # Boot Manager
  HybridFirmwarePlatformPkg/BootManager/BootPolicyEngine.inf
  HybridFirmwarePlatformPkg/BootManager/UefiBootManager/GenericBootManager.inf
  HybridFirmwarePlatformPkg/BootManager/UefiBootManager/LinuxBootManager.inf
  HybridFirmwarePlatformPkg/BootManager/WindowsBoot/WinBootMgr.inf
  HybridFirmwarePlatformPkg/BootManager/AppleBoot/AppleBootHelper.inf
  HybridFirmwarePlatformPkg/BootManager/PxeBoot/IpxeDriver.inf
  HybridFirmwarePlatformPkg/BootManager/PxeBoot/HttpBootClient.inf
  HybridFirmwarePlatformPkg/BootManager/BootMenu/BootMenuUi.inf
  HybridFirmwarePlatformPkg/BootManager/BootMenu/BootScripts.inf

  # OTA Update
  HybridFirmwarePlatformPkg/OtaUpdate/FwUpdateAgent.inf

  # Diagnostics
  HybridFirmwarePlatformPkg/Diagnostics/PowerOnSelfTest/MemoryTest.inf
  HybridFirmwarePlatformPkg/Diagnostics/PowerOnSelfTest/PciEnumTest.inf
  HybridFirmwarePlatformPkg/Diagnostics/UefiShell/ShellExtensions.inf

  # Security
  HybridFirmwarePlatformPkg/Security/SecureBoot/SecureBootPolicy.inf

[PcdsFixedAtBuild]
  gHybridFirmwarePlatformPkgTokenSpaceGuid.PcdPowerSourceType|2 # Default to Solar for Hybrid Firmware

  # Détection de plateforme (étape 3) : par défaut, DeviceTree sur
  # AARCH64/RISCV64 et Acpi sur X64/IA32 (voir PlatformDetectLib). Ces deux
  # PCD permettent de forcer un comportement différent par plateforme réelle.
  gHybridFirmwarePlatformPkgTokenSpaceGuid.PcdPreferDeviceTree|FALSE
  gHybridFirmwarePlatformPkgTokenSpaceGuid.PcdPlatformHasNoFirmwareTables|FALSE

  # Étape 4 : à surcharger par le .dsc d'une carte réelle utilisant le
  # backend DeviceTree (voir PlatformInfoPei.c / FdtPlatformDxe.c).
  gHybridFirmwarePlatformPkgTokenSpaceGuid.PcdFdtBaseAddress|0x0
