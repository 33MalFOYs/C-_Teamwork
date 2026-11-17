# EX Personal Expense Tracker - Phase 4

C++17 expense tracker with a Qt Widgets GUI, CLI, MySQL persistence, and a
manually owned singly linked list. Phase 3 adds real registration/sign-in,
account-scoped CRUD, database-generated IDs, saved monthly budgets, and logout.
Both interfaces use the same services and SLL-backed filters and reports.

The implementation follows [Expense_Tracker_AI_Roadmap.txt](docs/planning/Expense_Tracker_AI_Roadmap.txt).
Phase 4 adds CSV import/export, weekly summary CSV, GUI ID search and a runtime package. See [Phase 4 verification](docs/phase4-verification.md) and [Phase 4 guide](docs/phase4-guide.md) for the CSV contract, demonstration and requirement mapping.

## Updated GUI

The EX interface follows the supplied light finance-dashboard reference: white
rounded cards, cyan charts, blue actions, and consistent outline icons. Overview
has a monthly-plan card and live spending charts; Expenses pairs a transaction
ledger with a receipt-style editor; Reports adds weekly and monthly visual
breakdowns; Files presents separate import/export cards. Sign-in and registration
use their own welcome panel. Icon actions have tooltips and accessible names;
essential amount/date labels remain visible. Compact layouts scroll without
overlapping controls, and the expense editor stacks below the ledger when needed.

Charts use the same saved records and exact USD totals as the reports. Empty
periods remain empty, and signing out clears charts and account data. The desktop
shortcut uses scripts/launch-gui.ps1, which opens the application in build/app/bin.
Until that build exists, the launcher can use an older standard or UI build.
See [GUI design and verification](docs/gui-reference-redesign.md) for the page
layouts and visual regression coverage.

## Project folders

All application code is grouped in `main`; icons and their source artwork are in
`all asset`. The three principal files are:

| File | Role |
| --- | --- |
| [main/main.cpp](main/main.cpp) | Starts the Qt application and opens the main window |
| [main/gui.cpp](main/gui.cpp) | Main window, dashboard, and navigation |
| [main/sql.cpp](main/sql.cpp) | MySQL connections, prepared queries, and transactions |

```text
main/
  main.cpp             GUI entry point
  gui.cpp              Main window
  sql.cpp              Database implementation
  cli.cpp              Command-line entry point
  core/                Expenses, linked list, validation, CLI menus
  gui/                 Forms, controller, theme, and embedded resource list
  services/            Authentication, expenses, reports, and CSV
  include/             Shared C++ headers
  sql/schema.sql       Database schema
all asset/             EX icons and artwork prompt
scripts/               Launch, build, setup, and packaging commands
tests/                 Automated checks and fictional fixtures
docs/                  Design, verification, and planning records
cmake/                 Dependency build configuration
build/app/bin/         Compiled application and runtime libraries
.tools/                Local compiler, dependencies, and saved MySQL database
```

Build the complete project with `scripts/build.ps1`; the `.cpp` files depend on
each other and are not standalone programs. The original empty root-level
`algorithm.cpp`, `gui.cpp`, and `sqlite.cpp` placeholders have been removed.
Keep `config.local` and `.tools/mysql` in place to retain the existing connection
and saved data. Older verification documents retain their original path names.

## Run the application

From PowerShell in this project directory:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/launch-gui.ps1
# Or start the database and use the CLI:
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/local-mysql.ps1 -Action Start
.\build\app\bin\expense_tracker_cli.exe
```

The existing desktop shortcut runs the same launcher, which starts the isolated
database before opening the GUI.

Create an application account, then sign in. Usernames contain 3-64 ASCII
letters, digits, underscores, dots, or hyphens; surrounding whitespace is
removed and letters are lowercased. Passwords require at least 12 Unicode
characters, with an explicit 4096-byte UTF-8 limit. They are not trimmed or
truncated. Password confirmation must match exactly. Only salted Argon2id
hashes are stored. Account passwords are separate from MySQL connection
credentials.

The user selected an **isolated local MySQL instance** for Phase 3. It uses the
installed MAMP MySQL 5.7.24 executable, its own data under `.tools/mysql/data`,
a generated loopback port, and generated database credentials. The script
does not manage the existing MAMP service or the separate server on port 3306.
`config.local` points the application at this instance. The application
database starts without seed accounts; integration tests use a separate
`expense_tracker_test_*` database.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/local-mysql.ps1 -Action Status
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/local-mysql.ps1 -Action Stop
```

Stopping retains all data. Starting again reuses the same data and port.
Keep `.tools/mysql` and `config.local` private; they contain local credentials.

## Build and initial setup

The verified Windows kit is Qt 6.10.3, matching MinGW 13.1.0, and CMake 3.30.5.
Close running tracker windows before rebuilding. For this prepared workspace:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1
```

The script builds all targets into `build/app`, stages runtime DLLs/Qt platform
plugins in `build/app/bin`, and runs CTest. Integration suites are marked skipped
unless an isolated test config is supplied. For full verification of this prepared
workspace, use the existing isolated test database and its failure fixtures:

```powershell
$env:EXPENSE_TEST_WRITE_ONLY_CONFIG = (Resolve-Path .tools/mysql/writer.config).Path
$env:EXPENSE_TEST_REJECT_CATEGORY = '__reject_test__'
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1 -IntegrationConfig (Resolve-Path .tools/mysql/test.config).Path
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/test-cli-process.ps1 -Config .tools/mysql/test.config
```

The test scripts refuse database writes outside a named `expense_tracker_test_*`
schema. See [Phase 3 verification](docs/phase3-verification.md) for fixture setup
and the historical results.

For a fresh checkout, install the project-local dependencies first:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/setup-tools.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/setup-password-deps.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/setup-database-deps.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/local-mysql.ps1 -Action Start
.\build\app\bin\expense_tracker_cli.exe --config .tools/mysql/admin.config --init-db
```

Setup scripts verify pinned archive SHA-256 digests. They install under
`.tools`, restore temporary PATH changes, and do not replace system compilers.
The password archive's Minisign signature was verified in
[Phase 1](docs/phase1-verification.md). The connector script needs an installed
MySQL C client SDK and runtime; its default is
`C:\Program Files\MySQL\MySQL Server 8.0`. Pass `-MySQLClientRoot` if different.
The isolated-server script defaults to `C:\MAMP\bin\mysql\bin\mysqld.exe`;
pass `-Server` to select another installed compatible server binary.

Connector/C++ 8.1.0's classic JDBC API is source-built with the same MinGW kit.
The imported C client library is a C ABI dependency; no MSVC-built C++
connector is linked into the application. The source build uses compatibility
macros for Oracle's Windows integer literals. MinGW is a locally tested
configuration, not an Oracle-supported Windows compiler.

| Dependency | Local location |
| --- | --- |
| Qt 6.10.3 / MinGW 13.1 / CMake 3.30.5 | `.tools/Qt` |
| Connector/C++ 8.1.0 classic API | `.tools/connector-cpp` |
| libsodium 1.0.22 MinGW x64 | `.tools/libsodium-win64` |

The build script also accepts `-QtRoot`, `-CompilerBin`, `-CMake`,
`-ConnectorRoot`, `-SodiumRoot`, and `-BuildDirectory`. A CLI-only build uses
`-CliOnly` and writes to `build/cli/bin`; it still requires the connector and
password library. `-ExecutionPolicy Bypass` applies to the invoked PowerShell
process only.

## Configuration and schema

Copy [config.example](config.example) to ignored `config.local` for another
database. Both applications also accept `--config FILE`. Environment variables
override file values:

- `EXPENSE_DB_HOST`, `EXPENSE_DB_PORT`, `EXPENSE_DB_USER`,
  `EXPENSE_DB_PASSWORD`: explicitly configured; no guessed credentials.
- `EXPENSE_DB_NAME`: defaults to `expense_tracker`.
- `EXPENSE_CURRENCY`: USD only.

The file uses literal `KEY=value` lines. Password bytes after `=` are preserved,
including spaces and additional equals signs; do not add quotes. Unknown or
duplicate keys fail with line-number feedback without exposing values.
A missing password setting is rejected; an explicitly empty setting is accepted.

Normal startup verifies the schema and connects without creating tables.
Explicit `--init-db` creates a new schema using an account with DDL permissions;
it accepts an already compatible schema and refuses incompatible or partial
tracker tables. It never drops, resets, or reassigns records.
[main/sql/schema.sql](main/sql/schema.sql) documents the same users/expenses/budgets schema.
Use a backup and an explicit migration plan for pre-existing incompatible data.

The generated application MySQL account has SELECT/INSERT/UPDATE/DELETE access
only to the application schema. The local owner config is used separately for
schema setup. Workbench can inspect this same isolated instance using the host,
port, username, and password in the local config; the application does not
depend on Workbench, Apache, or PHP.

Connecting the existing MAMP service on its observed port 8889 remains untested:
its credentials were not supplied, and the user selected the isolated instance
instead. No connection to the unrelated server on port 3306 is claimed.

## Persistent workflows

The GUI offers registration/login and Dashboard, Expenses, and Reports tabs.
Dashboard shows the current week/month, saved current-month budget, remaining
amount, and overspending. Expense forms support add/edit, confirmed delete,
category filtering, and inclusive date filters. Reports use the same SLL
calculations as the CLI. Database and password work runs on a worker thread.
Logout clears the account data immediately; callbacks from an earlier session
cannot repopulate it.

The GUI also includes Find ID and a CSV files tab. CLI choices 18, 19 and 20 export filtered expenses, import CSV, and export weekly summaries.

The CLI account menu is 1 sign in, 2 create account, and 0 exit. Password echo is
disabled in an interactive console. Its authenticated menu is:

| Choice | Action |
| --- | --- |
| 1 / 2 | Add / view filtered expenses |
| 3 / 4 | Search by ID / category |
| 5 / 6 | Update / confirmed delete |
| 7 / 8 | Set / clear filters |
| 9 / 10 | Weekly / monthly report |
| 11 / 12 / 13 | Previous / next / current week |
| 14 / 15 | Set monthly budget / view remaining budget |
| 16 / 17 | Reload records / logout |
| 0 | Exit; committed records remain saved |

`/cancel` cancels an input form; EOF exits cleanly. A complete replacement is
required for edits, and an empty description clears the existing description.
Deletion requires confirmation. All SQL reads and mutations use the signed-in
user's ID, including operations supplied another user's known expense ID.

Successful writes reach MySQL before refreshing the SLL. If a write cannot be
confirmed, financial reads and further mutations are blocked until a successful
reload. If the write committed but reload failed, the message explicitly says
the change was saved: verify the refreshed records before retrying.
For a broken connection, restart the CLI, or log out and use the GUI's
configuration/reconnect button. There is no automatic unsaved fallback.

The explicit practice modes remain available:

```powershell
.\build\app\bin\expense_tracker_cli.exe --memory-demo
.\build\app\bin\expense_tracker_memory_gui.exe
```

They have no account or database access, start empty, and discard data on exit.
They are separate from normal persistent startup.

## Validation and design

Amounts use integer USD cents and MySQL `DECIMAL(12,2)`, never floating point.
Expenses accept 0.01 through 9999999999.99; budgets additionally accept zero.
Signs, exponents, commas, and excessive decimal precision are rejected.
Categories are trimmed and limited to 50 Unicode code points; descriptions may
be empty and have a 255-code-point limit. Invalid UTF-8 and embedded controls or
line breaks are rejected. Dates are real Gregorian `YYYY-MM-DD` dates in years
1000-9999.

Filters combine exact category and inclusive date endpoints with AND.
Weeks run Monday-Sunday and can cross months/years. Weekly averages divide by
seven, rounding only for display. A missing budget differs from a zero budget;
negative remaining budget indicates overspending.

See [design.md](docs/design.md) for ownership, threading, schema, and navigation;
[Phase 2 verification](docs/phase2-verification.md) remains a historical record.

| Source | Purpose |
| --- | --- |
| `main/core` | Expense, manually owned SLL, validation, and CLI menus |
| `main/services` and `main/sql.cpp` | Config, scoped prepared SQL, authentication, persistence, reports |
| `main/gui` and `main/gui.cpp` | Login, worker/controller, dashboard, expense and report forms |
| `tests` | Domain, auth/config, CLI, database, and Qt checks |
| `scripts` | Build, dependency setup, isolated MySQL lifecycle |
| `main/include/expense_tracker/file_manager.hpp` | CSV file import/export contract |
