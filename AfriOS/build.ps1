# Orchestrateur de build unifie pour l'ecosysteme AfriOS.
#
# Contrairement a afros-build.ps1 (AfriOS-dev_4/, qui ne fait qu'afficher des
# messages "SUCCES" sans jamais invoquer make/cmake), ce script appelle
# reellement les deux systemes de build independants du depot :
#   - le noyau AfriOS (CMake, AFROS_PORT)      -> AfriOS-dev_4/.../OS
#   - le firmware universel (EDK2, -a <ARCH>)   -> FirmwareHybride
#
# Usage :
#   ./build.ps1 -Target arm64      # noyau (port arm64) + firmware AARCH64
#   ./build.ps1 -Target x86_64     # noyau (port x86_64) + firmware X64
#   ./build.ps1 -Target riscv      # noyau (port riscv)  + firmware RISCV64
#   ./build.ps1 -Target mcu        # noyau (port mcu) uniquement - pas d'UEFI sur MCU
#   ./build.ps1 -Target arm64 -KernelOnly
#   ./build.ps1 -Target arm64 -FirmwareOnly

param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("arm64", "x86_64", "riscv", "mcu")]
    [string]$Target,

    [switch]$KernelOnly,
    [switch]$FirmwareOnly,

    # Utilise le toolchain croisé cmake/toolchains/toolchain-<target>.cmake
    # (voir OS/cmake/toolchains/) au lieu du compilateur natif de l'hôte.
    # Nécessaire pour arm64/x86_64/riscv/mcu si l'hôte n'est pas déjà de
    # cette architecture ; toujours nécessaire pour mcu (bare-metal).
    [switch]$Cross
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

$portMap = @{
    "arm64"  = "arm64"
    "x86_64" = "x86_64"
    "riscv"  = "riscv"
    "mcu"    = "mcu"
}

$edk2ArchMap = @{
    "arm64"  = "AARCH64"
    "x86_64" = "X64"
    "riscv"  = "RISCV64"
    "mcu"    = $null   # Pas de firmware UEFI sur ce port : EDK2 ne cible pas les MCU bare-metal.
}

$kernelPort = $portMap[$Target]
$edk2Arch   = $edk2ArchMap[$Target]

Write-Host "=== AfriOS build --target=$Target ===" -ForegroundColor Cyan
Write-Host "  Port noyau (AFROS_PORT)  : $kernelPort"
if ($edk2Arch) {
    Write-Host "  Architecture firmware (EDK2 -a) : $edk2Arch"
} else {
    Write-Host "  Firmware : aucun (port MCU, pas de cible EDK2 - voir docs/porting_guide.md)"
}

function Build-Kernel {
    $osDir = Join-Path $root "AfriOS-dev_4/AfriOS-dev_4/OS"
    if (-not (Test-Path $osDir)) {
        throw "Introuvable : $osDir"
    }
    Write-Host "`n--- Noyau AfriOS (CMake, AFROS_PORT=$kernelPort) ---" -ForegroundColor Yellow

    $buildDir = Join-Path $osDir "build-$kernelPort"
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        Write-Host "cmake introuvable dans ce PATH - build ignore (voir prerequis dans README)." -ForegroundColor Red
        return
    }

    $cmakeArgs = @("-B", $buildDir, "-S", $osDir, "-DAFROS_PORT=$kernelPort")

    if ($Cross -or $kernelPort -eq "mcu") {
        $toolchain = Join-Path $osDir "cmake/toolchains/toolchain-$kernelPort.cmake"
        if (-not (Test-Path $toolchain)) {
            throw "Toolchain introuvable : $toolchain"
        }
        Write-Host "  Cross-compilation via $toolchain" -ForegroundColor DarkYellow
        $cmakeArgs += "-DCMAKE_TOOLCHAIN_FILE=$toolchain"
    }

    cmake @cmakeArgs
    cmake --build $buildDir
}

function Build-Firmware {
    if (-not $edk2Arch) {
        Write-Host "`n--- Firmware : ignore (port $Target sans cible EDK2) ---" -ForegroundColor Yellow
        return
    }

    $fwDir = Join-Path $root "FirmwareHybride"
    if (-not (Test-Path $fwDir)) {
        throw "Introuvable : $fwDir"
    }
    Write-Host "`n--- Firmware Hybride (EDK2, -a $edk2Arch) ---" -ForegroundColor Yellow
    Write-Host "ATTENTION : le coeur EDK2 (MdePkg/BaseTools/...) n'est pas vendorise" -ForegroundColor Red
    Write-Host "dans ce depot (voir docs/architecture_overview.md). Ce build echouera" -ForegroundColor Red
    Write-Host "tant qu'un checkout EDK2 upstream n'est pas fusionne dans edk2/." -ForegroundColor Red

    if (-not (Get-Command bash -ErrorAction SilentlyContinue)) {
        Write-Host "bash introuvable dans ce PATH - Scripts/build.sh ne peut pas etre invoque." -ForegroundColor Red
        return
    }

    Push-Location $fwDir
    try {
        bash "Scripts/build.sh" $edk2Arch
    } finally {
        Pop-Location
    }
}

if (-not $FirmwareOnly) { Build-Kernel }
if (-not $KernelOnly)   { Build-Firmware }

Write-Host "`n=== Termine pour --target=$Target ===" -ForegroundColor Cyan
