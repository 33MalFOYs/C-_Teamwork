# Final verification audit - 2026-11-16

Historical path note: application sources now live under `main/`, artwork under
`all asset/`, and new builds under `build/app/` (`build/cli/` for CLI-only).
Paths and generated outputs below describe the original verification runs.
See the [current README](../README.md) for current run commands and locations.

The application was audited against the roadmap using the user-approved isolated
MySQL instance. The final audit added GUI filtered CSV export (using the table's
exact predicate), GUI filesystem error handling, and explicit tests of the Find
ID dialog and Qt CSV save dialog. Stale design/service documentation was corrected.
The delivery now includes source code and the installed MinGW runtime notices.

Verification covers domain/SLL ownership and cleanup, input validation, calendar
boundaries, exact cents, scoped account authentication, prepared CRUD, budgets,
restart persistence, SQL failures, cache invalidation, rollback, CSV validation,
new imported IDs, quotes/commas, BOM/CRLF, Unicode filenames, invalid UTF-8,
confirmed/rejected overwrites, file errors, weekly exports, GUI responsiveness,
logout/stale callbacks, resized layouts, and filtered export predicates.

Run scripts/build.ps1 with the isolated integration config and both failure
fixtures as documented in phase4-verification.md. A build without that config
skips the four database integration suites and is not full verification.

The CLI process demonstration passed 56 checks across six independent processes.
The backend suite includes 102 checks and the CLI coordinator suite 65 checks.
All integration records stay in the generated expense_tracker_test_* schema;
the application schema remains empty, ready for the user's first account.
Local credentials are ignored and excluded from the runtime/source ZIP.

Operational boundaries remain explicit: native Windows file-picker integration
(the tests exercise Qt's non-native dialogs), another target PC, and the user's
existing MAMP service on port 8889 have not been verified. The GUI budget editor
currently edits the current month; the CLI supports arbitrary month/year budgets.
Confirmed export overwrites are not atomic across a power loss. This is a local
classroom/demo package, not a signed installer or public-distribution license audit.

See the latest build/Testing/Temporary/LastTest.log and build/delivery package.

Final full-build result: 13/13 suites passed, zero failures/skips, 11.73 seconds.
The updated CSV screen was rendered at 850x720 and reviewed without clipping.

## EX GUI redesign verification

The GUI now uses an embedded EX icon, monochrome branding, readable summary
cards, a monthly-limit progress indicator, latest entries and direct dashboard
shortcuts. Expense forms distinguish new entries from edits, disable row actions
until selection, and keep all controls accessible through scrolling. CSV actions
have short explanations. No database schema or authentication rules changed.

All 13 test suites passed in build-ui (9.40 seconds, zero skips/failures), including
real MySQL integration. Added checks cover dashboard shortcut navigation/focus,
immediate clearing of dashboard values at logout, account switching, and
nonoverlapping expense/report widgets. Normal and compact (850x720) screenshots,
including top and bottom scroll positions, were visually inspected. Screenshots
use Qt's offscreen renderer, not a manual native-desktop session.

The updated application is in build-ui/bin. The older running build/bin window
was left intact to preserve any unfinished input. The desktop launcher opens the
newest build/bin or build-ui/bin GUI. Both builds use the existing ignored
configuration, so saved accounts and records remain. Theme resources are embedded
in the executable and included in packaged source. Package this verified build
with scripts/package.ps1 -BuildDirectory build-ui.
# Folder organization verification

The current application layout groups code in `main` and artwork in `all asset`.
`main/main.cpp` starts the GUI, `main/gui.cpp` implements its main window, and
`main/sql.cpp` implements database access. Shared modules, headers, and the schema
remain under `main`; planning documents are in `docs/planning`.

- All 55 relocated application/asset files were accounted for against a pre-move
  SHA-256 manifest. Only resource paths, the schema-path message, and documentation
  needed content changes; no application behavior was changed.
- A fresh default build in `build/app` passed all 13 CTest suites with zero skips
  and zero failures (13.65 seconds), using the isolated test database and the
  optional write-only/failure fixtures.
- The production CLI passed 56 checks across six separate processes using the
  isolated test database.
- All eight PowerShell scripts parsed successfully; README local links resolved.
- The existing desktop shortcut was updated to `all asset/ex-icon.ico`. Launching
  it opened the responsive `build/app/bin/expense_tracker_gui.exe` window titled
  `EX - Personal Expense Tracker`; only this newly created test window was closed.
- Removed the three verified empty root placeholders (`algorithm.cpp`, `gui.cpp`,
  `sqlite.cpp`) and obsolete generated `build-cli` / `build-phase2-cli` folders
  (545 generated files, approximately 20 MiB). Both folders were verified to be
  inside this workspace, without reparse points, private data, or running binaries.
- Older `build/bin` and `build-ui/bin` runtime files remain because existing user
  windows still use them. The launcher now prefers `build/app/bin`.
- Existing private configuration, saved MySQL data, toolchains, and test fixtures
  were retained. Earlier phase results are historical evidence.

