# build.ps1
# Build script for phu-splitter JUCE plugin
# Usage: .\scripts\build.ps1 [-Config Release|Debug] [-Clean]

param(
    [ValidateSet("Release", "Debug")]
    [string]$Config = "Release",
    
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

# Locate cmake using the find-cmake script
$cmake = & "$PSScriptRoot\find-cmake.ps1"
if ($LASTEXITCODE -ne 0) {
    Write-Error "Failed to locate cmake"
    exit 1
}

Write-Host "`n=== Building phu-splitter ($Config) ===`n" -ForegroundColor Cyan

$buildDir = "build\vs2026-x64"

# Clean if requested
if ($Clean) {
    Write-Host "Cleaning build directory..." -ForegroundColor Yellow
    Remove-Item -Recurse -Force $buildDir -ErrorAction SilentlyContinue
}

# Configure if needed
if (-not (Test-Path "$buildDir\CMakeCache.txt")) {
    Write-Host "Configuring CMake project..." -ForegroundColor Cyan
    & $cmake -B $buildDir -G "Visual Studio 18 2026" -A x64
    if ($LASTEXITCODE -ne 0) {
        Write-Error "CMake configuration failed"
        exit 1
    }
}

# Build
Write-Host "Building..." -ForegroundColor Cyan
& $cmake --build $buildDir --config $Config
if ($LASTEXITCODE -ne 0) {
    Write-Error "Build failed"
    exit 1
}

Write-Host "`n=== BUILD SUCCESSFUL ===`n" -ForegroundColor Green
Write-Host "VST3 plugin:" -ForegroundColor Cyan
$vst3Path = "$buildDir\src\phu-splitter_artefacts\$Config\VST3\PHU SPLITTER.vst3\Contents\x86_64-win\PHU SPLITTER.vst3"
if (Test-Path $vst3Path) {
    Get-Item $vst3Path | Select-Object Name, @{N='Size (MB)';E={[math]::Round($_.Length/1MB, 2)}}, LastWriteTime | Format-Table -AutoSize
} else {
    Write-Warning "VST3 plugin not found at expected location: $vst3Path"
}
