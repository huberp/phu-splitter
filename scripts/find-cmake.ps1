# find-cmake.ps1
# Locates cmake.exe on Windows following the CppImplWithCmakeAgent agent guidelines
# Returns the full path to cmake.exe or exits with error

$cmake = Get-Command cmake -ErrorAction SilentlyContinue | Select-Object -ExpandProperty Source

if (-not $cmake) {
    Write-Host "cmake not found in PATH, searching Visual Studio installations..." -ForegroundColor Yellow
    
    $searchPaths = @(
        "$env:ProgramFiles\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\18\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\18\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Professional\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "$env:ProgramFiles\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe",
        "$env:ProgramFiles\CMake\bin\cmake.exe"
    )
    
    foreach ($p in $searchPaths) {
        if (Test-Path $p) { 
            $cmake = $p
            break 
        }
    }
}

# Fallback: glob search for any Visual Studio version
if (-not $cmake) {
    Write-Host "Searching all Visual Studio installations..." -ForegroundColor Yellow
    $cmake = (Get-ChildItem "$env:ProgramFiles\Microsoft Visual Studio\*\*\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" -ErrorAction SilentlyContinue | Select-Object -First 1).FullName
}

if (-not $cmake) {
    Write-Error "cmake.exe not found. Please install CMake or Visual Studio with C++ development tools."
    exit 1
}

Write-Host "Found cmake at: $cmake" -ForegroundColor Green
& $cmake --version
Write-Output $cmake
