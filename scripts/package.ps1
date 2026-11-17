param([string]$BuildDirectory = 'build/app')
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$source = Join-Path (Join-Path $root $BuildDirectory) 'bin'
$destination = Join-Path $root ('build/delivery/ExpenseTracker-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))
New-Item -ItemType Directory -Force -Path $destination | Out-Null
foreach ($name in @('expense_tracker_cli.exe','expense_tracker_gui.exe')) {
    Copy-Item -LiteralPath (Join-Path $source $name) -Destination $destination
}
Get-ChildItem -LiteralPath $source -Filter '*.dll' | Copy-Item -Destination $destination
Copy-Item -LiteralPath (Join-Path $source 'platforms') -Destination $destination -Recurse
Copy-Item -LiteralPath (Join-Path $root 'config.example') -Destination $destination
Copy-Item -LiteralPath (Join-Path $root 'docs'),(Join-Path $root 'tests/fixtures') -Destination $destination -Recurse
$runtimeMain = Join-Path $destination 'main'
New-Item -ItemType Directory -Path $runtimeMain | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'main/sql') -Destination $runtimeMain -Recurse
@'
# EX Personal Expense Tracker

Run expense_tracker_gui.exe from this directory after configuring MySQL.
Install/start a compatible MySQL server separately. Copy config.example to
config.local and fill it with your verified database settings privately.
Initialize a new tracker schema with expense_tracker_cli.exe --init-db using
an authorized setup account, then use a limited application account for normal use.

This package contains no database credentials or saved accounts/expenses.
The main/sql folder contains the schema; fixtures contains fictional sample expenses.
The source folder contains the complete project, its build scripts and README.
Rebuild from that source folder; do not compile a single .cpp file on its own.
'@ | Set-Content -LiteralPath (Join-Path $destination 'README.md') -Encoding UTF8
# Include build sources for the classroom delivery, with an explicit allowlist.
$sourceDelivery = Join-Path $destination 'source'
New-Item -ItemType Directory -Path $sourceDelivery | Out-Null
foreach ($folder in @('main','all asset','cmake','scripts','tests','docs')) {
    Copy-Item -LiteralPath (Join-Path $root $folder) -Destination $sourceDelivery -Recurse
}
foreach ($file in @('CMakeLists.txt','README.md','config.example','.gitignore')) {
    Copy-Item -LiteralPath (Join-Path $root $file) -Destination $sourceDelivery
}
$licenses = Join-Path $destination 'licenses'
New-Item -ItemType Directory -Path $licenses | Out-Null
Copy-Item -LiteralPath (Join-Path $root '.tools/connector-cpp/share/doc') -Destination $licenses -Recurse
foreach ($component in @('gcc','winpthreads','mingw-w64')) {
    Copy-Item -LiteralPath (Join-Path $root ('.tools/Qt/Tools/mingw1310_64/licenses/' + $component)) -Destination $licenses -Recurse
}
$qtLicenses = Join-Path $root '.tools/Qt/6.10.3/mingw_64/LICENSES'
if (Test-Path -LiteralPath $qtLicenses) { Copy-Item -LiteralPath $qtLicenses -Destination $licenses -Recurse }
$sodiumLicense = Join-Path $root '.tools/libsodium-win64/LICENSE'
if (Test-Path -LiteralPath $sodiumLicense) { Copy-Item -LiteralPath $sodiumLicense -Destination $licenses }
if (Test-Path -LiteralPath (Join-Path $destination 'config.local')) { throw 'Unexpected credentials in delivery.' }
Compress-Archive -LiteralPath $destination -DestinationPath ($destination + '.zip')
Write-Output "Delivery created: $destination.zip"
Write-Output 'Provide a separately installed MySQL server and private config.local on the target computer.'
