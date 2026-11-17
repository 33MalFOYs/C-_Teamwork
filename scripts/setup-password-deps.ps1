param()

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$dependencyRoot = Join-Path $projectRoot '.tools'
$archive = Join-Path $dependencyRoot 'libsodium-1.0.22-mingw.tar.gz'
$expected = '1D99E0AFAF27BCE664249232E9DC628AE6BB7B49F0AB53AD6DB372B028A35D9D'
New-Item -ItemType Directory -Force -Path $dependencyRoot | Out-Null
if (-not (Test-Path -LiteralPath $archive)) {
    Invoke-WebRequest -UseBasicParsing -Uri 'https://download.libsodium.org/libsodium/releases/libsodium-1.0.22-mingw.tar.gz' -OutFile $archive
}
if ((Get-FileHash -LiteralPath $archive -Algorithm SHA256).Hash -ne $expected) {
    throw 'Password library archive checksum mismatch. The archive was not used.'
}
# This pinned digest belongs to the official archive whose Minisign signature
# was independently verified during Phase 1 (docs/phase1-verification.md).
& tar.exe -xzf $archive -C $dependencyRoot libsodium-win64
if ($LASTEXITCODE -ne 0) { throw 'Password library extraction failed.' }
Write-Output 'libsodium 1.0.22 MinGW x64 is ready under .tools/libsodium-win64.'
