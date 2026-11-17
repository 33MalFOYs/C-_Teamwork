param(
    [Parameter(Mandatory = $true)][string]$Config,
    [string]$CliBinary
)

# Exercise the production CLI across independent processes. Only fictional
# accounts in an explicitly named isolated test schema are permitted.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = Split-Path -Parent $PSScriptRoot
if (-not $CliBinary) { $CliBinary = Join-Path $projectRoot 'build/app/bin/expense_tracker_cli.exe' }
$cliPath = (Resolve-Path -LiteralPath $CliBinary).Path
$configPath = (Resolve-Path -LiteralPath $Config).Path
$databaseName = ''
foreach ($line in [IO.File]::ReadAllLines($configPath)) {
    if ($line -match '^\s*EXPENSE_DB_NAME\s*=(.*)$') { $databaseName = $Matches[1].Trim() }
}
$environmentDatabase = [Environment]::GetEnvironmentVariable('EXPENSE_DB_NAME')
if ($null -ne $environmentDatabase) { $databaseName = $environmentDatabase }
if (-not $databaseName.StartsWith('expense_tracker_test_', [StringComparison]::Ordinal)) {
    throw 'Refusing process-test writes: database must start with expense_tracker_test_.'
}

$script:checks = 0
$script:processes = 0
$account = 'phase3_process_' + [Guid]::NewGuid().ToString('N')
$password = 'Fictional process passphrase 123!'
$wrongPassword = 'Wrong fictional process password!'
$login = "1`n$account`n$password`n"

function Assert-Condition([bool]$condition, [string]$message) {
    $script:checks++
    if (-not $condition) { throw $message }
}
function Assert-Contains([string]$text, [string]$expected, [string]$message) {
    Assert-Condition $text.Contains($expected) $message
}
function Invoke-Cli([string]$inputText) {
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = $cliPath
    $start.Arguments = '--config "' + $configPath + '"'
    $start.WorkingDirectory = $projectRoot
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardInput = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $process = New-Object Diagnostics.Process
    $process.StartInfo = $start
    try {
        Assert-Condition $process.Start() 'Could not start the CLI test process.'
        $script:processes++
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        $process.StandardInput.Write($inputText)
        $process.StandardInput.Close()
        if (-not $process.WaitForExit(30000)) {
            $process.Kill()
            $process.WaitForExit()
            throw 'CLI process exceeded its 30-second test timeout.'
        }
        $output = $stdout.Result
        $errors = $stderr.Result
        Assert-Condition ($process.ExitCode -eq 0) 'CLI process returned a nonzero exit code.'
        Assert-Condition ([string]::IsNullOrWhiteSpace($errors)) 'CLI process wrote an unexpected diagnostic.'
        Assert-Condition (-not $output.Contains($password) -and -not $output.Contains($wrongPassword)) 'CLI printed a password.'
        Assert-Condition (-not $output.Contains('$argon2')) 'CLI printed an encoded password hash.'
        return $output
    } finally {
        $process.Dispose()
    }
}

Assert-Contains (Invoke-Cli "0`n") 'Goodbye.' 'Auth-menu exit failed.'
Assert-Contains (Invoke-Cli '') 'End of input. Goodbye.' 'Auth-menu EOF failed.'
$initial = Invoke-Cli (
    "2`n$account`n$password`n$password`n1`n$account`n$wrongPassword`n" + $login +
    "1`n20.00`nFood`n2026-12-28`nFictional process lunch`n" +
    "1`n7.50`nTravel`n2026-12-29`nFictional process ride`n" +
    "2`n14`n2026`n12`n200.00`n17`n0`n")
Assert-Contains $initial 'Account created. Sign in with your password.' 'Account registration failed.'
Assert-Contains $initial 'Invalid username or password.' 'Wrong password was accepted or exposed account details.'
Assert-Contains $initial "Signed in as $account." 'Correct login failed.'
Assert-Contains $initial 'Count: 2 | Total: USD 27.50' 'Saved rows were not displayed.'
Assert-Contains $initial 'Remaining: USD 172.50' 'Budget save/remaining failed.'
Assert-Contains $initial 'Logged out. All account data cleared from this session.' 'Logout failed.'
$ids = [regex]::Matches($initial, 'Saved expense ID ([0-9]+) to MySQL\.')
Assert-Condition ($ids.Count -eq 2) 'CLI did not return two database-generated IDs.'
$retainedId = $ids[0].Groups[1].Value
$removedId = $ids[1].Groups[1].Value
Assert-Condition ([int]$removedId -gt [int]$retainedId -and [int]$retainedId -gt 0) 'Database-generated IDs were not increasing.'

$edited = Invoke-Cli ($login +
    "2`n5`n$retainedId`n30.00`nFood`n2026-12-28`nFictional edited process lunch`n" +
    "6`n$removedId`nno`n2`n6`n2147483647`n6`n$removedId`nyes`n" +
    "15`n2026`n12`n17`n" + $login + "2`n0`n")
Assert-Contains $edited 'Count: 2 | Total: USD 27.50' 'A new process did not load saved rows.'
Assert-Contains $edited "Updated saved expense $retainedId." 'Saved update failed.'
Assert-Contains $edited 'Deletion canceled.' 'Delete cancellation failed.'
Assert-Contains $edited 'Count: 2 | Total: USD 37.50' 'Canceled delete changed saved rows.'
Assert-Contains $edited 'No expense with ID 2147483647.' 'Missing-ID handling failed.'
Assert-Contains $edited "Deleted saved expense $removedId." 'Confirmed delete failed.'
Assert-Contains $edited 'Remaining: USD 170.00' 'Saved budget did not survive process restart.'
Assert-Contains $edited 'Count: 1 | Total: USD 30.00' 'Logout/login did not reload final state.'

$reopened = Invoke-Cli ($login + "2`n3`n$removedId`n15`n2026`n12`n")
Assert-Contains $reopened 'Fictional edited process lunch' 'Edited description did not survive process restart.'
Assert-Contains $reopened 'Count: 1 | Total: USD 30.00' 'Saved edit/deletion did not survive process restart.'
Assert-Contains $reopened "No expense with ID $removedId." 'Deleted ID reappeared after process restart.'
Assert-Contains $reopened 'Budget: USD 200.00 | Spent: USD 30.00 | Remaining: USD 170.00' 'Budget data did not survive another process restart.'
Assert-Contains $reopened 'End of input. Goodbye. Saved changes remain in MySQL.' 'Authenticated EOF failed.'
Write-Output "Persistent CLI process checks passed: $script:checks checks across $script:processes processes."
$csvPath = Join-Path $projectRoot ("build/phase4-cli-" + [Guid]::NewGuid().ToString('N') + '.csv')
$weeklyPath = $csvPath + '.weekly.csv'
$csvRun = Invoke-Cli ($login + "18`n$csvPath`n20`n$weeklyPath`n2026-12-28`n19`n$csvPath`nyes`n2`n0`n")
Assert-Contains $csvRun 'Count: 2 | Total: USD 60.00' 'CLI export/import did not round trip.'
Assert-Condition (Test-Path -LiteralPath $weeklyPath) 'CLI weekly export missing.'
Assert-Contains ([IO.File]::ReadAllText($weeklyPath)) 'summary,total_usd,30.00' 'CLI weekly export incorrect.'
Write-Output "Phase 4 CLI process checks passed: $script:checks checks across $script:processes processes."
