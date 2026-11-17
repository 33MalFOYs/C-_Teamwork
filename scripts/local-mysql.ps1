param(
    [ValidateSet('Start','Stop','Status')][string]$Action = 'Start',
    [string]$Server = 'C:\MAMP\bin\mysql\bin\mysqld.exe'
)

# An isolated development instance, using the installed MySQL binary. It has its
# own data directory, random loopback port, accounts, and app/test databases.
# Never starts, stops, resets, or migrates an existing MAMP/Windows service.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = Split-Path -Parent $PSScriptRoot
$instanceRoot = Join-Path $projectRoot '.tools/mysql'
$stateFile = Join-Path $instanceRoot 'instance.json'
$serverConfig = Join-Path $instanceRoot 'server.ini'
$adminClient = Join-Path $instanceRoot 'admin-client.cnf'
$mysqlAdmin = Join-Path (Split-Path -Parent $Server) 'mysqladmin.exe'
$utf8 = New-Object System.Text.UTF8Encoding($false)
function Write-LocalText([string]$path, [string]$value) {
    [IO.File]::WriteAllText($path, $value, $utf8)
}
function New-LocalSecret {
    $bytes = New-Object byte[] 24
    $random = [Security.Cryptography.RandomNumberGenerator]::Create()
    try { $random.GetBytes($bytes) } finally { $random.Dispose() }
    return ([BitConverter]::ToString($bytes)).Replace('-', '').ToLowerInvariant()
}
function Write-AppConfig([string]$name, [string]$database, [string]$user, [string]$secret, [int]$port) {
    Write-LocalText (Join-Path $instanceRoot $name) "EXPENSE_DB_HOST=127.0.0.1`nEXPENSE_DB_PORT=$port`nEXPENSE_DB_NAME=$database`nEXPENSE_DB_USER=$user`nEXPENSE_DB_PASSWORD=$secret`nEXPENSE_CURRENCY=USD`n"
}

if (-not (Test-Path -LiteralPath $Server)) { throw 'MySQL server binary not found. Pass -Server with the installed mysqld.exe path.' }
if ($Action -ne 'Start' -and -not (Test-Path -LiteralPath $stateFile)) {
    Write-Output 'No project-local MySQL instance has been initialized.'
    exit 0
}
if (-not (Test-Path -LiteralPath $stateFile)) {
    $dataDirectory = Join-Path $instanceRoot 'data'
    if (Test-Path -LiteralPath $dataDirectory) {
        throw 'An existing local data directory lacks instance metadata. Inspect it before continuing; nothing was reset.'
    }
    New-Item -ItemType Directory -Force -Path $instanceRoot | Out-Null
    $listener = New-Object Net.Sockets.TcpListener([Net.IPAddress]::Loopback, 0)
    $listener.Start()
    $port = $listener.LocalEndpoint.Port
    $listener.Stop()
    $suffix = [Guid]::NewGuid().ToString('N').Substring(0, 12)
    $testDatabase = "expense_tracker_test_$suffix"
    $adminPassword = New-LocalSecret
    $appPassword = New-LocalSecret
    $testPassword = New-LocalSecret
    $writerPassword = New-LocalSecret
    $readerPassword = New-LocalSecret
    $baseDirectory = (Split-Path -Parent (Split-Path -Parent $Server)).Replace('\', '/')
    $dataForward = $dataDirectory.Replace('\', '/')
    $logForward = (Join-Path $instanceRoot 'server.log').Replace('\', '/')
    $pidForward = (Join-Path $instanceRoot 'server.pid').Replace('\', '/')
    $settings = "[mysqld]`nbasedir=`"$baseDirectory`"`ndatadir=`"$dataForward`"`nport=$port`nbind-address=127.0.0.1`nskip-name-resolve`nlog-error=`"$logForward`"`npid-file=`"$pidForward`"`nmax-connections=30`ninnodb-buffer-pool-size=32M`ncharacter-set-server=utf8mb4`ncollation-server=utf8mb4_unicode_ci`nsql-mode=STRICT_TRANS_TABLES,NO_ZERO_IN_DATE,NO_ZERO_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION`n"
    Write-LocalText $serverConfig $settings
    $initialization = Start-Process -FilePath $Server -ArgumentList "--defaults-file=`"$serverConfig`" --initialize-insecure --console" -WindowStyle Hidden -PassThru -Wait -RedirectStandardOutput (Join-Path $instanceRoot 'initialize.out.log') -RedirectStandardError (Join-Path $instanceRoot 'initialize.log')
    if ($initialization.ExitCode -ne 0) { throw 'Isolated MySQL initialization failed; inspect .tools/mysql/initialize.log. No existing server data was modified.' }

    # init-file runs before accepting clients. Secrets are generated here, never
    # guessed from MAMP and never passed as command-line arguments or printed.
    $bootstrap = @(
        "ALTER USER 'root'@'localhost' IDENTIFIED BY '$adminPassword';",
        "CREATE USER 'expense_owner'@'127.0.0.1' IDENTIFIED BY '$adminPassword';",
        "GRANT ALL PRIVILEGES ON *.* TO 'expense_owner'@'127.0.0.1' WITH GRANT OPTION;",
        'CREATE DATABASE expense_tracker CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;',
        "CREATE DATABASE $testDatabase CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;",
        "CREATE USER 'expense_app'@'127.0.0.1' IDENTIFIED BY '$appPassword';",
        "GRANT SELECT, INSERT, UPDATE, DELETE ON expense_tracker.* TO 'expense_app'@'127.0.0.1';",
        "CREATE USER 'expense_test'@'127.0.0.1' IDENTIFIED BY '$testPassword';",
        "GRANT ALL PRIVILEGES ON $testDatabase.* TO 'expense_test'@'127.0.0.1';",
        "CREATE USER 'expense_writer'@'127.0.0.1' IDENTIFIED BY '$writerPassword';",
        "GRANT INSERT ON $testDatabase.* TO 'expense_writer'@'127.0.0.1';",
        "CREATE USER 'expense_reader'@'127.0.0.1' IDENTIFIED BY '$readerPassword';",
        "GRANT SELECT ON $testDatabase.* TO 'expense_reader'@'127.0.0.1';"
    )
    $bootstrapFile = Join-Path $instanceRoot 'bootstrap.sql'
    Write-LocalText $bootstrapFile ($bootstrap -join "`n")
    $bootstrapForward = $bootstrapFile.Replace('\', '/')
    Write-LocalText $serverConfig ($settings + "init-file=`"$bootstrapForward`"`n")
    Write-LocalText $adminClient "[client]`nprotocol=TCP`nhost=127.0.0.1`nport=$port`nuser=expense_owner`npassword=$adminPassword`n"
    Write-AppConfig 'admin.config' 'expense_tracker' 'expense_owner' $adminPassword $port
    Write-AppConfig 'app.config' 'expense_tracker' 'expense_app' $appPassword $port
    Write-AppConfig 'test.config' $testDatabase 'expense_test' $testPassword $port
    Write-AppConfig 'writer.config' $testDatabase 'expense_writer' $writerPassword $port
    Write-AppConfig 'reader.config' $testDatabase 'expense_reader' $readerPassword $port
    $state = @{ port = $port; testDatabase = $testDatabase; server = $Server; firstStart = $true }
    Write-LocalText $stateFile ($state | ConvertTo-Json)
}
$state = Get-Content -LiteralPath $stateFile -Raw | ConvertFrom-Json
if ($state.server -ne $Server) { throw 'This instance belongs to a different server binary. Pass its recorded -Server path.' }

# ping alone may return success for access-denied; query version through the
# private client config instead so readiness also proves authentication.
$mysql = Join-Path (Split-Path -Parent $Server) 'mysql.exe'
function Test-LocalServer {
    $oldPreference = $ErrorActionPreference
    try {
        $ErrorActionPreference = 'Continue'
        & $mysql "--defaults-extra-file=$adminClient" --connect-timeout=1 --batch --skip-column-names --execute='SELECT 1' 2>$null | Out-Null
        return $LASTEXITCODE -eq 0
    } finally { $ErrorActionPreference = $oldPreference }
}
if ($Action -eq 'Status') {
    if (Test-LocalServer) { Write-Output "Project-local MySQL is running on 127.0.0.1:$($state.port)." }
    else { Write-Output 'Project-local MySQL is stopped.' }
    exit 0
}
if ($Action -eq 'Stop') {
    if (Test-LocalServer) {
        & $mysqlAdmin "--defaults-extra-file=$adminClient" --connect-timeout=3 shutdown
        if ($LASTEXITCODE -ne 0) { throw 'Could not stop the project-local server.' }
    }
    Write-Output 'Project-local MySQL stopped. Its data is retained.'
    exit 0
}
if (-not (Test-LocalServer)) {
    $process = Start-Process -FilePath $Server -ArgumentList "--defaults-file=`"$serverConfig`" --console" -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $instanceRoot 'console.out.log') -RedirectStandardError (Join-Path $instanceRoot 'console.err.log')
    $ready = $false
    for ($attempt = 0; $attempt -lt 100; ++$attempt) {
        if ($process.HasExited) { throw 'Project-local MySQL exited; inspect .tools/mysql/server.log.' }
        if (Test-LocalServer) { $ready = $true; break }
        Start-Sleep -Milliseconds 200
    }
    if (-not $ready) { throw 'Project-local MySQL did not become ready; inspect its local logs.' }
}
if ($state.firstStart) {
    $lines = Get-Content -LiteralPath $serverConfig | Where-Object { $_ -notmatch '^init-file=' }
    Write-LocalText $serverConfig ($lines -join "`n")
    $state.firstStart = $false
    Write-LocalText $stateFile ($state | ConvertTo-Json)
}
$localConfig = Join-Path $projectRoot 'config.local'
if (-not (Test-Path -LiteralPath $localConfig)) {
    Copy-Item -LiteralPath (Join-Path $instanceRoot 'app.config') -Destination $localConfig
}
Write-Output "Project-local MySQL is ready on 127.0.0.1:$($state.port)."
Write-Output 'Application config: config.local (created only if absent). Credentials stay in ignored local files.'
Write-Output 'Initialize app tables with expense_tracker_cli --config .tools/mysql/admin.config --init-db after building.'
