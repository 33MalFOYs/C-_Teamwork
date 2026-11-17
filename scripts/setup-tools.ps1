# Optional project-local kit. No system PATH, registry, services, or other
# compiler installations are modified. Qt archive terms/licenses are retained.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = Split-Path -Parent $PSScriptRoot
$toolsDirectory = Join-Path $projectRoot '.tools'
$qtDirectory = Join-Path $toolsDirectory 'Qt'
$repository = 'https://download.qt.io/online/qtsdkrepository/windows_x86/desktop'
$packages = @(
    @{
        Name = 'qtbase'
        Url = "$repository/qt6_6103/qt6_6103/qt.qt6.6103.win64_mingw/6.10.3-0-202603310407qtbase-Windows-Windows_11_24H2-Mingw-Windows-Windows_11_24H2-X86_64.7z"
        Hash = '5C551318297ED374AC2C40A3E9A8061FFB0414F01346F15304E0CA5C0B8DE08C'
        Destination = Join-Path $qtDirectory '6.10.3/mingw_64'
        Marker = Join-Path $qtDirectory '6.10.3/mingw_64/lib/cmake/Qt6/Qt6Config.cmake'
    },
    @{
        Name = 'mingw'
        Url = "$repository/tools_mingw1310/qt.tools.win64_mingw1310/13.1.0-202407240918mingw1310.7z"
        Hash = 'A7B502294A903B64FCCD4A41BA39F48D3F6C9A5ECE167F0CFAEAC920D15B344F'
        Destination = $qtDirectory
        Marker = Join-Path $qtDirectory 'Tools/mingw1310_64/bin/g++.exe'
    },
    @{
        Name = 'cmake'
        Url = "$repository/tools_cmake/qt.tools.cmake/3.30.5-202502251255windows64_x64_cmake_tools.7z"
        Hash = '257FF96C0E9FA6765C8C247132CF3A2A02B2A5BA360D32097B3605674196CABF'
        Destination = Join-Path $qtDirectory 'Tools/CMake'
        Marker = Join-Path $qtDirectory 'Tools/CMake/bin/cmake.exe'
    }
)
New-Item -ItemType Directory -Force -Path $toolsDirectory | Out-Null
foreach ($package in $packages) {
    $archive = Join-Path $toolsDirectory ($package.Name + '.7z')
    if (-not (Test-Path -LiteralPath $archive)) {
        Write-Output "Downloading $($package.Name)..."
        & curl.exe --fail --location --silent --show-error --retry 2 --output $archive $package.Url
        if ($LASTEXITCODE -ne 0) { throw "Download failed: $($package.Name)" }
    }
    if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $package.Hash) {
        throw "Checksum mismatch: $archive. Remove that archive and retry."
    }
    if (-not (Test-Path -LiteralPath $package.Marker)) {
        Write-Output "Extracting $($package.Name)..."
        New-Item -ItemType Directory -Force -Path $package.Destination | Out-Null
        & tar.exe -xf $archive -C $package.Destination
        if ($LASTEXITCODE -ne 0) { throw "Extraction failed: $($package.Name)" }
    }
}
Write-Output 'Local Qt 6.10.3 / MinGW 13.1 / CMake 3.30.5 kit ready. Run scripts/build.ps1.'
