# Phase 4 verification

Historical path note: application sources now live under `main/`, artwork under
`all asset/`, and new builds under `build/app/` (`build/cli/` for CLI-only).
Paths and generated outputs below describe the original verification runs.
See the [current README](../README.md) for current run commands and locations.

Verified 2026-11-16 on Windows x64 using the isolated MySQL instance selected by
the user. Full CMake/MinGW/Qt build succeeded; all 13 CTest suites passed with
zero skips/failures in 11.04 seconds. Both optional backend failure fixtures
were enabled (SQL rejection trigger and INSERT-only account).

New CSV integration verifies escaped commas/quotes, round-trip field values,
new generated IDs, complete-file validation, row/quote/header errors, missing
and unwritable paths, overwrite refusal without changes, transaction rollback,
signed-out rejection, overspending, and independently expected weekly totals
35.00 / 3 records / 5.00 daily average. Existing domain, account isolation,
password/config, persistence and GUI regression suites pass.

The production CLI script passed 56 checks across six separate processes,
including restart persistence, saved edit/delete, budgets and expense/weekly CSV
export and reimport. GUI integration exercised CSV operations through the real
worker and verified refreshed records. Screenshots at 850x720 were inspected:
CSV and Expenses screens have readable controls without clipping. The tests
use Qt's offscreen platform; native OS file-picker dialogs and a different target
computer have not been manually tested.

Reproduce:

```powershell
$env:EXPENSE_TEST_WRITE_ONLY_CONFIG = (Resolve-Path .tools/mysql/writer.config).Path
$env:EXPENSE_TEST_REJECT_CATEGORY = '__reject_test__'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -IntegrationConfig (Resolve-Path .tools/mysql/test.config).Path
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-cli-process.ps1 -Config .tools/mysql/test.config
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/package.ps1
```

The isolated schema already has the rejection fixture installed, as recorded
in phase3-verification.md. Test accounts/files are fictional and retained under
the guarded test schema and ignored build directory. No application accounts
or credentials are included in the delivery ZIP. See phase4-guide.md for usage,
assignment mapping, package requirements and remaining operational limitations.
