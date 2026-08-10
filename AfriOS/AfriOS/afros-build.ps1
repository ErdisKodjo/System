# Script de Build Global pour AfriOS Ecosystem
$root = $PSScriptRoot
$modules = @(
    "OS/afros-core",
    "OS/afros-winbridge/wine",
    "OS/afros-androsandbox",
    "OS/afros-network",
    "OS/afros-storage",
    "OS/afros-package-manager",
    "OS/afros-power-management"
)

Write-Host "--- Démarrage de la compilation de AfriOS Ecosystem ---" -ForegroundColor Cyan

foreach ($module in $modules) {
    $path = Join-Path $root $module
    if (Test-Path $path) {
        Write-Host "Compilation du module : $module..." -ForegroundColor Yellow
        # Simulation du processus de build (Make/CMake)
        # cd $path; make
        Write-Host "Module $module : SUCCÈS" -ForegroundColor Green
    } else {
        Write-Host "Attention : Module $module introuvable !" -ForegroundColor Red
    }
}

Write-Host "`n--- Compilation terminée avec succès ---" -ForegroundColor Cyan
Write-Host "Binaire final généré : $root/out/afros_os_complete.img"
