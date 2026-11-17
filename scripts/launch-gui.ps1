$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
try {
    & (Join-Path $PSScriptRoot 'local-mysql.ps1') -Action Start
    # New builds live together under build/app. Keep a fallback for older builds
    # until the reorganized project has been built on this computer.
    $application = Join-Path $projectRoot 'build/app/bin/expense_tracker_gui.exe'
    if (-not (Test-Path -LiteralPath $application)) {
        $candidates = @('build/bin/expense_tracker_gui.exe', 'build-ui/bin/expense_tracker_gui.exe') |
            ForEach-Object { Join-Path $projectRoot $_ } |
            Where-Object { Test-Path -LiteralPath $_ } |
            ForEach-Object { Get-Item -LiteralPath $_ } |
            Sort-Object LastWriteTimeUtc -Descending
        if (-not $candidates) {
            throw 'Application not built. Run scripts/build.ps1 first.'
        }
        $application = @($candidates)[0].FullName
    }
    Start-Process -FilePath $application -WorkingDirectory $projectRoot
} catch {
    Add-Type -AssemblyName System.Windows.Forms
    [System.Windows.Forms.MessageBox]::Show($_.Exception.Message, 'Expense Tracker startup failed') | Out-Null
    exit 1
}
