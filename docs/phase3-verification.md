# Phase 3 verification

Historical path note: application sources now live under `main/`, artwork under
`all asset/`, and new builds under `build/app/` (`build/cli/` for CLI-only).
Paths and generated outputs below describe the original verification runs.
See the [current README](../README.md) for current run commands and locations.

Verified on 2026-11-16 in the shared Windows workspace. Phase 3 implements real
MySQL persistence and account authentication through both the CLI and Qt GUI.
The user selected an isolated MySQL instance for this phase instead of supplying
credentials for the existing MAMP service.

## Environment

| Component | Verified configuration |
| --- | --- |
| Language / compiler | C++17, Qt-kit MinGW 13.1.0 x64 |
| GUI / build | Qt Widgets 6.10.3, CMake 3.30.5 |
| Connector | Oracle Connector/C++ 8.1.0 classic JDBC API, source-built with the matching MinGW kit |
| C client | Installed MySQL 8.0.46 C API headers/import library/runtime, linked through its C ABI |
| Password library | libsodium 1.0.22 MinGW x64; Argon2id interactive policy |
| Test server | MAMP's installed MySQL 5.7.24 binary, separate project-local data directory and generated accounts |
| Connection | Loopback port 59735 for this instance; recorded in ignored local configuration |
| Data separation | Application schema `expense_tracker`; generated `expense_tracker_test_*` schema for fictional integration records |

The existing MAMP service on port 8889 and the unrelated MySQL server on port
3306 were not used or changed. Workbench authentication to that existing MAMP
service remains untested. MinGW connector support is a locally verified build,
not an Oracle-supported Windows compiler configuration.

## Results

The final full build succeeded. **All 12 CTest suites passed, with no skips or
failures**, in 8.61 seconds:

| Suite | Result |
| --- | --- |
| `core_smoke` | Production account-menu entry, exit, EOF, and stream failures |
| `domain_tests` | 131 domain checks, zero failures; Expense/SLL/validation regression |
| `services_tests` | In-memory filtering, reports, workspace/session boundaries |
| `cli_phase2_tests` | Explicit temporary CLI CRUD/filter/report regression |
| `auth_policy_tests` | Username normalization, UTF-8/password limits and confirmation policy |
| `config_tests` | BOM/key parsing, exact password bytes, environment overrides, secret-safe errors |
| `persistence_tests` | 102 checks against real MySQL, including both optional failure fixtures below |
| `cli_persistence_tests` | 65 account/CRUD/report/budget checks through the real CLI menu coordinator |
| `gui_smoke` | Login controls, masking, Enter, explicit rejection of 4097-byte passwords |
| `gui_phase2_tests` | Temporary forms, filters/reports, confirmation, resize and close regression |
| `gui_phase3_tests` | Asynchronous configuration failure, clear feedback, blocked financial access |
| `gui_persistence_tests` | Real registration/login, CRUD, budgets, account isolation, logout/stale callbacks, restart, responsive event loop |

Additional verification:

- `scripts/test-cli-process.ps1`: **48 checks passed across 5 independent CLI
  processes**. Saved edits, confirmed/canceled deletion, generated IDs, budgets,
  logout/relogin, restart, and EOF behavior passed. Captured output contained no
  test password or encoded hash.
- Wrong database credentials and a closed loopback port were rejected. The
  production CLI returned exit code 1 on connection failure and did not enter
  an unsaved workspace or expose its configured password.
- Database-generated IDs remained distinct after deletion/insertion. Quotes,
  commas, SQL-looking punctuation, and Khmer UTF-8 text round-tripped intact.
- Two accounts could not read, update, delete, report on, or obtain the other
  account's expense collection, including direct use of a known foreign ID.
  Budgets and transactional imports were scoped to the current account.
- Correct/wrong passwords, duplicate normalized usernames, password whitespace,
  Unicode code-point length, retry delay, logout, and restart were checked.
  Aggregate-only database inspection confirmed Argon2id prefixes and distinct
  salted stored hashes. Password and hash values were not printed.
- A deliberate trigger rejected the second row of an import transaction; the
  first row rolled back. A denied SQL UPDATE invalidated the readable cache.
  An INSERT-only connection committed an expense but failed its subsequent
  SELECT: the message correctly said the change was saved, blocked stale reads,
  and reload recovered exactly one inserted row.
- Schema setup was repeated successfully on both application and test schemas.
  Separate partial/legacy and incompatible-unique-index fixture schemas were
  rejected by setup and verification. Their table definitions and sentinel
  records remained unchanged. No existing schema was dropped or reset.
- Repeated reconnects, overlapping same-thread connections, concurrent worker
  connections, and shutdown passed. This covers the repaired MinGW cleanup
  crash: connection-owned thread leases now release the connector before its
  process-static driver is destroyed.

Qt integration uses the real widgets and event loop with the offscreen platform.
A timer verified that the event loop continued processing during authentication
and database work. Screenshots of login, dashboard, and expense screens were
rendered and visually inspected; text and controls were readable without
clipping. The dashboard correctly showed USD 17.66 remaining from a USD 30.00
budget and USD 12.34 spending. These are programmatic GUI checks and image
reviews, not a new manual desktop-interaction session.

Artifacts remain in the ignored build directory:

- `build/Testing/Temporary/LastTest.log`
- `build/gui-phase3-login.png`
- `build/gui-phase3-dashboard.png`
- `build/gui-phase3-expenses.png`

## Repeat the checks

From PowerShell in the prepared project workspace:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/local-mysql.ps1 -Action Start
.\build\bin\expense_tracker_cli.exe --config .tools/mysql/test.config --init-db
$testConfig = (Resolve-Path -LiteralPath .tools/mysql/test.config).Path
$env:EXPENSE_TEST_WRITE_ONLY_CONFIG = (Resolve-Path -LiteralPath .tools/mysql/writer.config).Path
$env:EXPENSE_TEST_REJECT_CATEGORY = '__reject_test__'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -IntegrationConfig $testConfig
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-cli-process.ps1 -Config $testConfig
```

The prepared test schema already contains the rejection trigger. For a fresh
instance, install `tests/fixtures/phase3_reject_insert.sql` in the selected
`expense_tracker_test_*` schema before setting `EXPENSE_TEST_REJECT_CATEGORY`.
Do not install this deliberately rejecting trigger in the application schema.
The local-server script generates the INSERT-only test account/config used by
`EXPENSE_TEST_WRITE_ONLY_CONFIG`. Without these optional fixtures, the backend
suite explicitly reports their checks as skipped.

Every integration executable refuses to write unless the selected database name
starts with `expense_tracker_test_`. Each run creates unique fictional accounts;
test records are retained instead of deleting or resetting data. Configuration
environment overrides must point at the same intended isolated server. Test
configurations and local credentials are ignored by Git.

A normal build without `-IntegrationConfig` still runs offline tests, but its
three database integration suites return CTest's skip code 77. That result must
not be described as full database verification.

## Remaining scope

Phase 4 still owns CSV file parsing/export and weekly-summary CSV, remaining
GUI search/polish, and delivery packaging/documentation. The transaction service
is ready for CSV integration, but no CSV import/export command is claimed here.
Existing MAMP-service connection testing can follow once its verified local
configuration is supplied. The isolated server is left running for the user;
`scripts/local-mysql.ps1 -Action Stop` stops it while retaining its data.
