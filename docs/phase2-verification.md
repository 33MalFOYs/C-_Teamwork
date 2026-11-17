# Phase 2 verification record

Historical path note: application sources now live under `main/`, artwork under
`all asset/`, and new builds under `build/app/` (`build/cli/` for CLI-only).
Paths and generated outputs below describe the original verification runs.
See the [current README](../README.md) for current run commands and locations.

Recorded for 2026-11-16. Scope: in-memory Expense/SLL operations, validation,
date/category filtering, weekly/monthly calculations, CLI behavior, and Qt forms.
Phase 1 setup evidence remains in `phase1-verification.md`.

## Implementation boundary

- The CLI scratch workflow requires `--memory-demo`; the temporary GUI is the
  separate `expense_tracker_memory_gui` executable.
- Each starts with a separate empty SLL and loses its records on closing.
  Temporary positive IDs increase on successful insertions and are not reused.
- Both presentations call `ExpenseWorkspace`, implemented by
  `MemoryExpenseService`; validation and calculations are shared C++ code.
- Normal CLI startup and `expense_tracker_gui` keep unavailable authentication.
  No production session becomes signed in; no MySQL or CSV writes occur.

## Check evidence

| Check | Result |
| --- | --- |
| Service test executable | PASS in direct MinGW 13.1 C++17 build with `-Wall -Wextra -Wpedantic`; filter/report/workspace tests completed successfully. |
| Service scenarios | Cross-year Monday–Sunday fixture gives USD 35.00, count 3, daily average USD 5.00; next Monday excluded. Seven daily rows/category sums, empty reports, leap day, inclusive endpoints, combined filters, monthly year separation, mutations, temporary IDs, and signed-out production report guard covered. |
| SLL static diagnostic | MinGW 13.1 `-fanalyzer` check of `src/core/expense_list.cpp` returned exit code 0 with no diagnostics. This is not a whole-program memory sanitizer. |
| Domain test suite | PASS: 131 checks covering default/parameterized constructors, money/date/text validation, empty/single/multiple nodes, head/middle/tail deletion, duplicate/missing IDs, atomic failed updates/insertion, clear/destructor reuse, and test-local allocation accounting. Cleanup runs 30 cycles of 100 nodes, alternating explicit clear and destructor cleanup. |
| CLI test suite | PASS: scripted tests exercise the temporary workflow, validation, cancellation, deletion confirmation, missing IDs, filtering/reports, and input termination. |
| CLI-only integration | PASS: CMake build in `build-phase2-cli` with MinGW 13.1; all four core/domain/services/CLI suites passed. |
| Actual CLI process demo | PASS: `tests/fixtures/phase2_cli_demo.txt` was piped into the executable; output is in ignored `build-phase2-cli/demo-output.txt`. It produced week total 35.00/count 3/average 5.00, Food filter 15.00, edited Food 17.00, canceled then confirmed deletion, and final week total 17.00/average 2.43. A second process started empty. |
| GUI test suite | PASS: `gui_smoke` and `gui_phase2_tests` passed using Qt's offscreen platform. Form interactions and the unchanged login boundary are checked separately. |
| Full CMake build and CTest | PASS: canonical `scripts/build.ps1` built all targets and passed all six suites: `core_smoke`, `domain_tests`, `services_tests`, `cli_phase2_tests`, `gui_smoke`, and `gui_phase2_tests`. |
| GUI visual review | PASS: all four freshly rendered expense/report images were inspected at 1060×820 and 850×720. Labels, tables, forms, temporary-data banner, and all seven daily rows were readable without clipping. Initial offscreen font discovery was corrected in the test with Windows Segoe UI. |
| Final GUI refinement check | PASS: `gui_phase2_tests` was rebuilt and rerun after the test-font correction and keyboard Tab check; the targeted test passed 1/1. The earlier full suite passed 6/6 before these test-only refinements. |

Allocation accounting in `domain_tests.cpp` observes ordinary allocations made
by the test/program, including SLL nodes. A shared standard library may allocate
internally, so it is not a claim of complete process-wide leak coverage.

GUI evidence consists of offscreen-rendered images and simulated Qt interactions.
No native manual desktop interaction or native interactive GUI launch was
performed during Phase 2; these checks do not claim one.

## Repeatable build and runs

From this project directory in PowerShell, close running tracker windows before
rebuilding:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/build.ps1
.\build\bin\expense_tracker_cli.exe --memory-demo
.\build\bin\expense_tracker_memory_gui.exe
```

The build script runs CTest automatically. To rerun the configured tests:

```powershell
& '.tools\Qt\Tools\CMake\bin\ctest.exe' --test-dir build --output-on-failure
```

To replay the fictional CLI demonstration in a new temporary process:

```powershell
Get-Content tests/fixtures/phase2_cli_demo.txt | .\build\bin\expense_tracker_cli.exe --memory-demo
```

The normal entry points remain available for checking the login boundary:

```powershell
.\build\bin\expense_tracker_cli.exe
.\build\bin\expense_tracker_gui.exe
```

No credentials are needed for these checks. The current matching kit is
Qt 6.10.3, MinGW 13.1.0, and CMake 3.30.5. `-ExecutionPolicy Bypass` applies
only to the invoked PowerShell process, not the machine policy.

## Reproducible manual demonstration

1. Start an empty temporary workspace. View all expenses; expect zero rows/total.
2. Add fictional records: Food USD 10.00 on 2026-12-28; Travel USD 20.00 on
   2026-12-30; Food USD 5.00 on 2027-01-03; Food USD 99.00 on 2027-01-04.
3. Report the week containing 2026-12-30. Expect 2026-12-28 through 2027-01-03,
   total USD 35.00, count 3, daily average USD 5.00, and four zero-spending days.
   Food totals USD 15.00 and Travel USD 20.00. Next Monday is excluded.
4. Filter 2026-12-28 through 2027-01-03 plus Food. Expect two rows and USD 15.00.
   Try a reversed range; the previous active results must remain unchanged.
5. Edit the Monday amount to USD 12.00 and date to Tuesday. Expect weekly total
   USD 37.00, Monday zero, and Tuesday USD 12.00. Invalid replacement data must
   preserve the original record. Cancel a form and a deletion; nothing changes.
6. Delete Sunday's row with confirmation. Expect weekly total USD 32.00.
   Add another record and confirm its temporary ID is new.
7. Exit and reopen. Expect an empty workspace; no records have been saved.

CLI details: `/cancel` is reserved at all prompts, updates replace all fields,
empty descriptions clear previous text, and deletion requires `yes`.

## Remaining later-phase work

MySQL connection/schema application, authenticated account lifecycle, persisted
per-user CRUD, and connector integration are Phase 3. Budgets, CSV import/export,
authenticated dashboard completion, deployment packaging, and final end-to-end
checks remain Phase 3–4. No MAMP/Workbench connection success is claimed here;
the earlier pending credentials and connection checks are unchanged.
