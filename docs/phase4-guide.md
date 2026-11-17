# Phase 4 usage and demonstration

The application now includes CSV import/export and weekly summary export in both
interfaces, plus Find ID in the GUI. The existing date filters, reports, budgets,
account isolation, and persistent CRUD remain shared services.

## CSV contract

Expense CSV uses UTF-8, optional BOM, LF or CRLF, and this exact header:

```text
expense_id,amount,category,expense_date,description
```

Commas and double quotes are escaped using quoted fields and doubled quotes.
Embedded line breaks are explicitly rejected with a row number. Empty descriptions
are supported. Source IDs may be empty or positive integers; they are ignored for
insertion. Every imported row receives a fresh MySQL ID owned by the signed-in
account. Reimporting creates duplicates. The complete file is validated before
one transaction; a SQL failure rolls back the batch. Invalid UTF-8, extra columns,
invalid dates/amounts, missing headers, and malformed quotes are rejected.

CLI choices 18/19/20 export the active filter, import new records, and export a
selected week. GUI CSV files tab offers separate all-records and filtered-records exports, plus a selected week.
Existing destinations require confirmation; cancel leaves them unchanged.
Unwritable paths and incomplete reads/writes report errors. Export files contain
financial data and should be stored privately. Weekly CSV uses section,label,value
rows for boundaries, total, count, daily average, seven days and category totals;
it is a report, not an expense-import file. Currency is USD.

## Demonstration

1. Start the isolated MySQL server with scripts/local-mysql.ps1 -Action Start.
2. Run the CLI, create an account and sign in. Import tests/fixtures/weekly_expenses.csv
   using choice 19. The same fixture can be imported from the GUI CSV files tab.
3. View/search records; apply Food plus 2026-12-28 through 2027-01-03. Expect USD 15.00.
4. Report the week containing 2026-12-31: total 35.00, three records, average 5.00.
   The following Monday's 99.00 is excluded. Export the summary with choice 20.
5. Set December 2026 budget to 25.00. Spending 30.00 yields remaining -5.00.
6. Edit/delete an expense, confirm totals, exit and sign in again. Saved changes remain.
7. Open the GUI and sign in to the SAME account/config. Inspect the same IDs and totals,
   use Find ID, date filters, Reports, and CSV export. Logout clears the account view.
8. Export/import a CSV containing quotes/commas. Newly inserted rows have new IDs.
   Try an invalid second row and confirm that neither row is inserted.

## Requirement mapping

| Requirement | Source |
| --- | --- |
| Encapsulation / constructors / overloads | expense.hpp/.cpp and expense_list.hpp/.cpp |
| Manual singly linked list / cleanup | main/core/expense_list.cpp |
| Input validation / cents / Gregorian dates | main/core/validation.cpp |
| ID search / filtering / reports | expense_workspace.hpp, expense_filter.cpp, report_service.cpp |
| Prepared SQL / ownership / transactions | main/sql.cpp |
| Password hashes / registration / sessions | auth_service.cpp, session.cpp |
| Database-first mutations and budgets | main/services/expense_service.cpp |
| C++ file streams / CSV | main/services/file_manager.cpp |
| CLI actions | main/core/memory_cli.cpp and expense_tracker.cpp |
| Qt forms / background worker / session guards | main/gui/expense_page.cpp, main/gui.cpp, main/gui/persistent_controller.cpp |
| Repeatable verification | tests/csv_tests.cpp, persistence_tests.cpp, gui_persistence_tests.cpp, scripts/test-cli-process.ps1 |

## Delivery

Run scripts/package.ps1 after a successful build in build/app. The ZIP under build/delivery
contains the production CLI/GUI, Qt/MinGW/database/password DLLs, platform plugins,
schema, documentation and fictional fixtures. It excludes config.local, database
data, accounts and test executables. Extract it on a compatible Windows x64
computer. Install/start MySQL separately, create the schema with --init-db using
an authorized setup account, and fill config.local from config.example privately.
Run from the extracted directory. The original MAMP service remains unverified;
this workspace uses the user-approved isolated MySQL instance.

The package is a local classroom/demo distribution, not a signed installer.
Validate the target machine before presenting. Export writes detect failures but
are not an atomic file replacement: an interrupted confirmed overwrite can leave
a partial file. Very large CSVs are validated in memory before insertion.
