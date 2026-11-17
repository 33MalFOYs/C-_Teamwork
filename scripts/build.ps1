param(
    [string]$QtRoot,
    [string]$CompilerBin,
    [string]$CMake,
    [string]$BuildDirectory,
    [string]$IntegrationConfig,
    [string]$ConnectorRoot,
    [string]$SodiumRoot,
    [switch]$CliOnly
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = Split-Path -Parent $PSScriptRoot
$portableRoot = Join-Path $projectRoot '.tools/Qt'
if (-not $ConnectorRoot) { $ConnectorRoot = Join-Path $projectRoot '.tools/connector-cpp' }
if (-not $SodiumRoot) { $SodiumRoot = Join-Path $projectRoot '.tools/libsodium-win64' }
if (-not $QtRoot) { $QtRoot = Join-Path $portableRoot '6.10.3/mingw_64' }
if (-not $CompilerBin) {
    $CompilerBin = Join-Path $portableRoot 'Tools/mingw1310_64/bin'
    if (-not (Test-Path -LiteralPath $CompilerBin)) {
        $CompilerBin = Split-Path -Parent (Get-Command g++.exe -ErrorAction Stop).Source
    }
}
if (-not $CMake) {
    $CMake = Join-Path $portableRoot 'Tools/CMake/bin/cmake.exe'
    if (-not (Test-Path -LiteralPath $CMake)) {
        $CMake = (Get-Command cmake.exe -ErrorAction Stop).Source
    }
}
if (-not $BuildDirectory) {
    $folder = if ($CliOnly) { 'build/cli' } else { 'build/app' }
    $BuildDirectory = Join-Path $projectRoot $folder
}
$compiler = Join-Path $CompilerBin 'g++.exe'
$make = Join-Path $CompilerBin 'mingw32-make.exe'
foreach ($path in @($CMake, $compiler, $make)) {
    if (-not (Test-Path -LiteralPath $path)) { throw "Required tool not found: $path" }
}
if (-not $CliOnly -and -not (Test-Path -LiteralPath (Join-Path $QtRoot 'lib/cmake/Qt6/Qt6Config.cmake'))) {
    throw 'Qt Widgets development kit missing. Run scripts/setup-tools.ps1 or pass -QtRoot and its matching -CompilerBin.'
}

$originalPath = $env:PATH
try {
    $env:PATH = "$CompilerBin;$(Join-Path $QtRoot 'bin');$originalPath"
    $gui = if ($CliOnly) { 'OFF' } else { 'ON' }
    & $CMake -S $projectRoot -B $BuildDirectory -G 'MinGW Makefiles' "-DCMAKE_CXX_COMPILER=$compiler" "-DCMAKE_MAKE_PROGRAM=$make" "-DCMAKE_PREFIX_PATH=$QtRoot" '-DCMAKE_BUILD_TYPE=Debug' "-DEXPENSE_TRACKER_BUILD_GUI=$gui" '-DBUILD_TESTING=ON' "-DEXPENSE_CONNECTOR_ROOT=$ConnectorRoot" "-DEXPENSE_SODIUM_ROOT=$SodiumRoot" "-DEXPENSE_TRACKER_INTEGRATION_CONFIG=$IntegrationConfig"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configuration failed.' }
    & $CMake --build $BuildDirectory --parallel 4
    if ($LASTEXITCODE -ne 0) { throw 'Build failed.' }

    # Keep executables runnable outside this temporary PATH. Only the Qt modules
    # used by the login and expense forms are staged; final deployment is Phase 4.
    $output = Join-Path $BuildDirectory 'bin'
    foreach ($name in @('libgcc_s_seh-1.dll', 'libstdc++-6.dll', 'libwinpthread-1.dll')) {
        Copy-Item -LiteralPath (Join-Path $CompilerBin $name) -Destination $output -Force
    }
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $ConnectorRoot 'bin') -Filter '*.dll') {
        Copy-Item -LiteralPath $file.FullName -Destination $output -Force
    }
    foreach ($file in Get-ChildItem -LiteralPath (Join-Path $SodiumRoot 'bin') -Filter '*.dll') {
        Copy-Item -LiteralPath $file.FullName -Destination $output -Force
    }
    if (-not $CliOnly) {
        foreach ($name in @('Qt6Core.dll', 'Qt6Gui.dll', 'Qt6Widgets.dll', 'Qt6Test.dll')) {
            Copy-Item -LiteralPath (Join-Path $QtRoot "bin/$name") -Destination $output -Force
        }
        $platforms = Join-Path $output 'platforms'
        New-Item -ItemType Directory -Force -Path $platforms | Out-Null
        foreach ($name in @('qwindows.dll', 'qoffscreen.dll')) {
            Copy-Item -LiteralPath (Join-Path $QtRoot "plugins/platforms/$name") -Destination $platforms -Force
        }
    }
    $ctest = Join-Path (Split-Path -Parent $CMake) 'ctest.exe'
    & $ctest --test-dir $BuildDirectory --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Verification checks failed.' }
    Write-Output "Build and tests passed. Executables: $output"
} finally {
    $env:PATH = $originalPath
}

