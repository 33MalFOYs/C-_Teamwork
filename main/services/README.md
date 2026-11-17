# Shared services through Phase 4

- `auth_service.cpp`: canonical ASCII usernames, validated UTF-8 passwords,
  libsodium Argon2id registration/verification, generic credential failures,
  retry delay, and verified session creation only after scoped expenses load.
  Registration leaves the user signed out; logout clears the session and SLL.
- `../sql.cpp`: MySQL Connector/C++ connection/schema verification,
  prepared account/expense/budget SQL, owner-scoped IDs, generated IDs, atomic
  SLL loading, and transactional imports. Setup is explicit; startup does not
  create, migrate, or seed a database. SQL errors omit credentials and hashes.
- `expense_service.cpp`: shared authenticated CLI/Qt financial operations.
  Writes validate then commit to MySQL before reloading the SLL. A failed or
  uncertain write, or a committed write whose reload fails, blocks dependent
  reads until an explicit successful reload. There is no unsaved fallback.
- `session.cpp`: signed-out and cache-readiness guards. The session starts
  unauthenticated, and only `AuthService`/`ExpenseService` may change its private
  user, readiness, and generation fields.
- `config.cpp`: local key/value settings with environment overrides and
  sanitized validation errors. Account passwords are separate from these MySQL
  connection credentials.
- `expense_filter.cpp`: validated inclusive date and exact-category predicates.
- `report_service.cpp`: pure SLL `ReportCalculator` weekly/monthly aggregation
  and a `ReportService` facade gated by the authenticated readable session.
- `memory_expense_service.cpp`: an empty temporary SLL workspace implementing
  the same `ExpenseWorkspace` interface for explicitly selected practice mode.

The memory service does not authenticate a `Session`, read a database, or load
fixtures automatically. Its records exist only during the explicit temporary
workspace. Normal CLI/GUI entry points use MySQL and real authentication.

Declarations live in `main/include/expense_tracker/`. `FileManager` implements CSV parsing/export and weekly summary export; complete-file
validation delegates to the persistent import transaction service. Both file parsing
and transactional failure behavior have integration tests. Qt's persistent controller
constructs and uses all database/session/service resources on one worker thread.
