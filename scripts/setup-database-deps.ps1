[CmdletBinding()]
param(
    [string]$QtRoot = '',
    [string]$MySQLClientRoot = 'C:\Program Files\MySQL\MySQL Server 8.0',
    [ValidateRange(1, 16)][int]$Jobs = 4
)

$ErrorActionPreference = 'Stop'
$repository = Split-Path -Parent $PSScriptRoot
$dependencyRoot = Join-Path $repository '.tools'
if (-not $QtRoot) { $QtRoot = Join-Path $dependencyRoot 'Qt' }
$compilerBin = Join-Path $QtRoot 'Tools\mingw1310_64\bin'
$cmake = Join-Path $QtRoot 'Tools\CMake\bin\cmake.exe'
$clientLibrary = Join-Path $MySQLClientRoot 'lib\libmysql.lib'
$runtimeFiles = @(
    (Join-Path $MySQLClientRoot 'lib\libmysql.dll'),
    (Join-Path $MySQLClientRoot 'bin\libssl-3-x64.dll'),
    (Join-Path $MySQLClientRoot 'bin\libcrypto-3-x64.dll')
)
foreach ($required in @($cmake, (Join-Path $compilerBin 'g++.exe'),
    (Join-Path $MySQLClientRoot 'include\mysql.h'), $clientLibrary) + $runtimeFiles) {
    if (-not (Test-Path -LiteralPath $required -PathType Leaf)) {
        throw "Missing build dependency: $required"
    }
}

$version = '8.1.0'
$archive = Join-Path $dependencyRoot "mysql-connector-cpp-$version-src.tar.gz"
$source = Join-Path $dependencyRoot "mysql-connector-c++-$version-src"
$build = Join-Path $dependencyRoot 'connector-cpp-build'
$prefix = Join-Path $dependencyRoot 'connector-cpp'
$archiveHash = '2EE3C7D0D031CE581DEEED747D9561D140172373592BED5D0630A790E6053DC1'
New-Item -ItemType Directory -Path $dependencyRoot -Force | Out-Null
if (-not (Test-Path -LiteralPath $archive)) {
    $uri = "https://cdn.mysql.com/archives/mysql-connector-c++/mysql-connector-c++-$version-src.tar.gz"
    Write-Host "Downloading Oracle Connector/C++ $version source..."
    Invoke-WebRequest -Uri $uri -OutFile $archive -UseBasicParsing
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $archiveHash) {
    throw 'Connector source archive checksum mismatch. The downloaded archive was not used.'
}
if (-not (Test-Path -LiteralPath (Join-Path $source 'version.cmake'))) {
    & tar.exe -xzf $archive -C $dependencyRoot
    if ($LASTEXITCODE -ne 0) { throw 'Connector source extraction failed.' }
}

$previousPath = $env:PATH
try {
    $env:PATH = $compilerBin + ';' + $previousPath
    & $cmake -S (Join-Path $repository 'cmake\connector-jdbc') -B $build `
        -G 'MinGW Makefiles' -DCMAKE_BUILD_TYPE=Release `
        "-DCMAKE_C_COMPILER=$(Join-Path $compilerBin 'gcc.exe')" `
        "-DCMAKE_CXX_COMPILER=$(Join-Path $compilerBin 'g++.exe')" `
        "-DCONNECTOR_SOURCE_ROOT=$source" "-DWITH_MYSQL=$MySQLClientRoot" `
        "-DCMAKE_INSTALL_PREFIX=$prefix"
    if ($LASTEXITCODE -ne 0) { throw 'Connector configuration failed.' }
    & $cmake --build $build --target jdbc --parallel $Jobs
    if ($LASTEXITCODE -ne 0) { throw 'Connector source build failed.' }

    # jdbc is the full classic C++ implementation. Its C API dependency stays
    # explicit, so no MSVC C++ binary or upstream archive merge is involved.
    New-Item -ItemType Directory -Force -Path (Join-Path $prefix 'lib'),
        (Join-Path $prefix 'include'), (Join-Path $prefix 'bin'),
        (Join-Path $prefix 'share\doc') | Out-Null
    Copy-Item -LiteralPath (Join-Path $build 'jdbc\driver\libjdbc.a') `
        -Destination (Join-Path $prefix 'lib\libmysqlcppconn-static.a') -Force
    Copy-Item -LiteralPath (Join-Path $build 'include\jdbc') `
        -Destination (Join-Path $prefix 'include') -Recurse -Force
    Copy-Item -LiteralPath $clientLibrary -Destination (Join-Path $prefix 'lib') -Force
    Copy-Item -LiteralPath $runtimeFiles -Destination (Join-Path $prefix 'bin') -Force
    Copy-Item -LiteralPath (Join-Path $source 'LICENSE.txt') `
        -Destination (Join-Path $prefix 'share\doc\Connector-Cpp-LICENSE.txt') -Force
    if (Test-Path -LiteralPath (Join-Path $MySQLClientRoot 'LICENSE')) {
        Copy-Item -LiteralPath (Join-Path $MySQLClientRoot 'LICENSE') `
            -Destination (Join-Path $prefix 'share\doc\MySQL-Client-LICENSE.txt') -Force
    }
    Write-Host "Connector/C++ $version classic JDBC is ready at $prefix"
    Write-Host 'Link libmysqlcppconn-static.a + libmysql.lib + dnsapi; define STATIC_CONCPP.'
    Write-Host 'MinGW is a locally verified configuration, not an Oracle-supported Windows compiler.'
} finally {
    $env:PATH = $previousPath
}
