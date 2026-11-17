# Phase 1 verification record

Historical path note: application sources now live under `main/`, artwork under
`all asset/`, and new builds under `build/app/` (`build/cli/` for CLI-only).
Paths and generated outputs below describe the original verification runs.
See the [current README](../README.md) for current run commands and locations.

Recorded for 2026-11-16. Scope: requirements, interfaces, project skeleton,
database design, local environment inspection, and fictional test data. This is
not a claim that later-phase features are implemented.

## Environment evidence

| Inspection | Observed result |
| --- | --- |
| Existing source | `algorithm.cpp`, `sqlite.cpp`, and `gui.cpp` were empty placeholders; retained. |
| Existing compiler | `C:\ProgramData\mingw64\mingw64\bin\g++.exe`, GCC 15.2.0, x86_64 POSIX/SEH; `mingw32-make` also present. |
| Selected GUI compiler | Project-local `.tools/Qt/Tools/mingw1310_64/bin/g++.exe` reported MinGW 13.1.0, 64-bit; matching-kit full build passed. |
| Qt/CMake | Initially absent. Project-local `qmake -query QT_VERSION` confirmed Qt 6.10.3 at `.tools/Qt/6.10.3/mingw_64`; `.tools/Qt/Tools/CMake/bin/cmake.exe --version` confirmed CMake 3.30.5. |
| MAMP | MAMP 5.0.6 installed under `C:\MAMP`. |
| MAMP MySQL version | `C:\MAMP\bin\mysql\bin\mysqld.exe --version` reported MySQL 5.7.24 for Win64 x86_64. |
| MAMP port | `C:\MAMP\conf\mysql\my.ini` has port 8889 in both `[client]` and `[mysqld]`. No listener observed on 8889. |
| Other MySQL server | Separate MySQL 8.0.46 installation/process listens on 3306 and 33060. It was not substituted for MAMP. |
| Workbench | 8.0.47 installed at `C:\Program Files\MySQL\MySQL Workbench 8.0 CE\MySQLWorkbench.exe`. |
| Credentials | MAMP username/password unverified; none guessed or written to source. |

## Executed checks

| Check | Result |
| --- | --- |
| Roadmap/source inspection | AI roadmap baseline selected; differing SQLite/teamwork scope documented. |
| Schema review | Required tables, ownership FKs, per-user budget uniqueness, and expense user/date index present. No reset/data reassignment; not executed against a server. |
| Fixture parsing | PowerShell `Import-Csv` parsed four records and preserved embedded quotes. |
| Fixture dates and totals | `DateTime.ParseExact` confirmed Monday 2026-12-28 through Sunday 2027-01-03; three matching rows sum to 35.00 using `Decimal`; next Monday excluded. |
| CMake configure/build | PASS with matching Qt 6.10.3 / MinGW 13.1.0 / CMake 3.30.5. Shared core, CLI, GUI, and smoke targets built; CTest passed 2/2 tests. |
| CLI smoke checks | PASS (`core_smoke`): explicit exit, EOF, invalid choices, stream failure, unavailable auth, and signed-out initial session. |
| CLI-only compiler check | PASS: separate GCC 15.2.0 configure/build in `build-cli`; CTest passed 1/1 core test. |
| Qt GUI build/open | PASS: GUI compiled and the native window `Personal Expense Tracker - Sign in` launched successfully. |
| GUI interaction smoke | PASS (`gui_smoke`, offscreen): visible controls, password mask/show/hide, fail-closed Sign in/Create account, Enter submission, password clearing, no financial window, and close. |
| Manual GUI visual/resize review | NOT CHECKED: computer interaction was stopped by the user's Escape key before screenshot/layout verification. No further desktop interaction was performed. |
| libsodium dependency check | PASS: official libsodium 1.0.22 MinGW archive signature and trusted comment verified using Minisign 0.12. DLL architecture is PE x86-64. Compile/link/runtime smoke checks passed with both GCC 15.2.0 and the selected MinGW 13.1.0 kit: explicit Argon2id hashing, correct-password acceptance, wrong-password rejection, independently generated salts, and encoded hash length within VARCHAR(255). Not integrated into application auth. |

## Repeatable verification

From the project directory in PowerShell:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1
.\build\bin\expense_tracker_cli.exe
.\build\bin\expense_tracker_gui.exe
```

The build script runs CTest automatically, stages the required runtime files,
and restores its temporary `PATH`. The command's execution-policy setting is
limited to that PowerShell process. For a fresh checkout, first run the optional
`powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/setup-tools.ps1`.
It downloads pinned official Qt/MinGW/CMake archives (about 160 MiB), verifies
their SHA-256 checksums, and extracts them under `.tools/Qt`.

To rerun just the configured tests with the local CMake installation:

```powershell
& '.tools\Qt\Tools\CMake\bin\ctest.exe' --test-dir build --output-on-failure
```

A successful full check includes the GUI target. `-CliOnly` builds in
`build-cli/bin` and runs the core test without verifying GUI requirements.

Manual CLI check: enter invalid text, choose Sign in and Create account, view
project information, then enter `0`. The menu must remain usable and must never
claim authentication. Also run with end-of-input to confirm clean termination.

Manual GUI check: verify visible username/password labels, masked password and
show/hide control, Enter submission, Create account navigation/status, keyboard
tab order, inline feedback, resizing, and close. Neither a submitted form nor
test data may open authenticated financial screens during Phase 1.

The dependency smoke source and tools are local, ignored artifacts under `.tools`.
The exact matching-kit check executed was:

```powershell
& '.tools\minisign-win64\x86_64\minisign.exe' -Vm .tools/libsodium-1.0.22-mingw.tar.gz -P RWQf6LRCGA9i53mlYecO4IzT51TGPpvWucNSCh1CBM0QTaLn73Y7GFO3
$env:PATH = (Resolve-Path -LiteralPath '.tools\libsodium-win64\bin').Path + ';' + (Resolve-Path -LiteralPath '.tools\Qt\Tools\mingw1310_64\bin').Path + ';' + $env:PATH
& '.tools\Qt\Tools\mingw1310_64\bin\g++.exe' -std=c++17 -Wall -Wextra -Wpedantic -I.tools/libsodium-win64/include .tools/libsodium_smoke.cpp -L.tools/libsodium-win64/lib -lsodium -o .tools/libsodium_smoke_mingw13.exe
& '.tools\libsodium_smoke_mingw13.exe'
```

All commands returned exit code 0. The test prints only the library version and
check summary, never password or hash values. The archive's recorded SHA-256 is
`1D99E0AFAF27BCE664249232E9DC628AE6BB7B49F0AB53AD6DB372B028A35D9D`.
The archive is from the [official release directory](https://download.libsodium.org/libsodium/releases/libsodium-1.0.22-mingw.tar.gz);
the verification key is published in the [installation documentation](https://doc.libsodium.org/installation).

## Pending external integration

- Start the intended MAMP MySQL service and verify an authenticated Workbench
  connection to `127.0.0.1:8889` using the user's verified credentials.
- Inspect/back up existing data and agree on ownership before any migration;
  `sql/schema.sql` has not been applied.
- Source-build the selected Connector/C++ 8.1.0 classic JDBC API with the matching
  MinGW kit, link it, and test the real MySQL 5.7.24 connection. MinGW support is
  unofficial; ABI/linking compatibility is not claimed before that check.
- Integrate libsodium Argon2id and execute registration/login/account-isolation
  checks in Phase 3. No plaintext account passwords or demo bypasses are used.

Phase 2 remains responsible for complete Expense/SLL behavior and validation;
Phase 3 for MySQL, authentication, and persistent CRUD; Phase 4 for completed
reports, budgets, CSV, GUI flows, packaging, and end-to-end demonstration.
