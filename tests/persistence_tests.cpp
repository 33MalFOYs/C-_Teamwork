#include "expense_tracker/auth_service.hpp"
#include "expense_tracker/config.hpp"
#include "expense_tracker/expense_service.hpp"
#include "expense_tracker/report_service.hpp"

#include <sodium.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

namespace {
using namespace expense_tracker;
int checks = 0;

void require(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

template<class Action>
std::string rejects(Action action, const char* message) {
    ++checks;
    try { action(); }
    catch (const std::exception& error) { return error.what(); }
    throw std::runtime_error(message);
}

std::string uniqueSuffix() {
    std::array<unsigned char, 8> bytes{};
    randombytes_buf(bytes.data(), bytes.size());
    static constexpr char digits[] = "0123456789abcdef";
    std::string text;
    for (const auto byte : bytes) { text += digits[byte >> 4]; text += digits[byte & 15]; }
    return text;
}

void checkSignedOut(ExpenseService& expenses, ReportService& reports) {
    rejects([&] { expenses.reload(); }, "Signed-out reload was allowed");
    rejects([&] { expenses.add({100, "Food", "2026-12-28", "fictional"}); }, "Signed-out insert was allowed");
    rejects([&] { expenses.update(1, {100, "Food", "2026-12-28", "fictional"}); }, "Signed-out update was allowed");
    rejects([&] { expenses.erase(1); }, "Signed-out delete was allowed");
    rejects([&] { expenses.searchExpense(1); }, "Signed-out ID search was allowed");
    rejects([&] { expenses.searchExpense(std::string("Food")); }, "Signed-out category search was allowed");
    rejects([&] { expenses.matching(ExpenseFilter{}); }, "Signed-out matching/export source was allowed");
    rejects([&] { expenses.total(ExpenseFilter{}); }, "Signed-out total was allowed");
    rejects([&] { expenses.weekly("2026-12-30"); }, "Signed-out weekly report was allowed");
    rejects([&] { expenses.monthly({2026, 12}); }, "Signed-out monthly report was allowed");
    rejects([&] { expenses.budget({2026, 12}); }, "Signed-out budget read was allowed");
    rejects([&] { expenses.setBudget({2026, 12}, 1000); }, "Signed-out budget write was allowed");
    rejects([&] { expenses.importRows({{100, "Food", "2026-12-28", "fictional"}}); }, "Signed-out import was allowed");
    rejects([&] { reports.weekly("2026-12-30"); }, "Signed-out report facade was allowed");
}

void checkConnectionLifecycle(const DatabaseConfig& config) {
    // Regression for connector thread cleanup running after the static driver
    // was destroyed. Exercise explicit close, overlapping leases, and worker
    // destruction; normal test-process exit also checks the main-thread path.
    DatabaseManager primary;
    for (int attempt = 0; attempt < 3; ++attempt) {
        primary.connect(config);
        primary.verifySchema();
        primary.disconnect();
        require(!primary.connected(), "Explicit disconnect retained a connection");
    }
    primary.connect(config);
    {
        DatabaseManager overlapping;
        overlapping.connect(config);
        primary.disconnect();
        overlapping.verifySchema();
        primary.connect(config);
        overlapping.disconnect();
        primary.verifySchema();
    }
    require(primary.connected(), "Closing another connection invalidated the surviving connection");

    std::array<std::exception_ptr, 2> errors{};
    std::array<std::thread, 2> workers;
    try {
        for (std::size_t index = 0; index < workers.size(); ++index) {
            workers[index] = std::thread([&, index] {
                try {
                    for (int attempt = 0; attempt < 2; ++attempt) {
                        DatabaseManager workerDatabase;
                        workerDatabase.connect(config);
                        workerDatabase.verifySchema();
                    } // Connection and final thread lease close on the owning worker.
                } catch (...) { errors[index] = std::current_exception(); }
            });
        }
    } catch (...) {
        for (auto& worker : workers) if (worker.joinable()) worker.join();
        throw;
    }
    for (auto& worker : workers) worker.join();
    for (const auto& error : errors) if (error) std::rethrow_exception(error);
    primary.verifySchema();
    require(primary.connected(), "Worker shutdown invalidated the main-thread connection");
    primary.disconnect();
    std::cout << "Reconnect, overlapping connections, and concurrent worker shutdown checks passed.\n";
}

void runChecks(const DatabaseConfig& config, const std::filesystem::path& writerConfigPath) {
    checkConnectionLifecycle(config);
    DatabaseManager database;
    database.connect(config);
    database.verifySchema();
    Session session;
    ExpenseList records;
    AuthService auth(database, session, records);
    ExpenseService expenses(session, database, records);
    ReportService reports(session, records);
    checkSignedOut(expenses, reports);

    const auto suffix = uniqueSuffix();
    const auto accountA = "phase3_a_" + suffix;
    const auto accountB = "phase3_b_" + suffix;
    const std::string password = "Fictional passphrase 123!";
    std::string unicodePassword = " ";
    for (int index = 0; index < 12; ++index) unicodePassword += "\xe1\x9e\x80";
    unicodePassword += " ";
    auth.registerAccount(accountA, password, password);
    auth.registerAccount(accountB, unicodePassword, unicodePassword);
    require(!session.authenticated() && records.size() == 0, "Registration unexpectedly authenticated");
    auto duplicate = accountA;
    std::transform(duplicate.begin(), duplicate.end(), duplicate.begin(), [](char ch) {
        return ch >= 'a' && ch <= 'z' ? static_cast<char>(ch - 'a' + 'A') : ch;
    });
    const auto duplicateError = rejects([&] { auth.registerAccount(" " + duplicate + " ", password, password); }, "Duplicate canonical username accepted");
    require(duplicateError.find("Username") != std::string::npos, "Duplicate username feedback unclear");
    rejects([&] { auth.registerAccount("phase3_c_" + suffix, password, "does not match"); }, "Mismatched confirmation accepted");

    require(auth.signIn(" " + duplicate + " ", password), "Correct normalized login rejected");
    require(session.authenticated() && session.cacheReady() && session.currentUser()->username == accountA,
            "Session not published after login");
    const int userA = session.requireUserId();
    const std::string punctuation = "Fictional lunch, \"shared\"; O'Reilly; DROP TABLE users; --";
    const int monday = expenses.add({1000, "Food", "2026-12-28", punctuation});
    const int wednesday = expenses.add({2000, "Travel", "2026-12-30", "Fictional ride"});
    const int sunday = expenses.add({500, "Food", "2027-01-03", "Fictional snack"});
    const int nextMonday = expenses.add({9900, "Other", "2027-01-04", "Next week"});
    const std::string unicode = "\xe1\x9e\x80\xe1\x9e\x81";
    const int unicodeId = expenses.add({123, unicode, "2024-02-29", unicode + " quote ' and comma,"});
    require(monday > 0 && wednesday > monday && sunday > wednesday && nextMonday > sunday,
            "Database-generated IDs are not positive and increasing");
    require(expenses.searchExpense(monday)->description() == punctuation, "Prepared SQL did not preserve punctuation");
    require(expenses.searchExpense(unicodeId)->category() == unicode, "UTF-8 text did not round trip");
    require(expenses.weekly("2026-12-31").total == 3500 && expenses.weekly("2026-12-31").count == 3,
            "Persistent weekly SLL report is incorrect");
    require(reports.weekly("2026-12-31").total == 3500, "Authenticated report facade disagrees");
    require(expenses.total(ExpenseFilter(DateRange{"2026-12-28", "2027-01-03"}, "Food")) == 1500,
            "Persistent category/date filter is incorrect");
    require(expenses.update(monday, {1000, "Food", "2026-12-28", punctuation}), "Unchanged owned row falsely reported missing");
    rejects([&] { expenses.update(monday, {0, "Changed", "2026-12-28", "invalid"}); }, "Invalid update accepted");
    require(session.cacheReady() && expenses.searchExpense(monday)->amount() == 1000,
            "Validation failure changed the cache or original record");
    require(expenses.update(monday, {1200, "Food", "2026-12-29", punctuation}), "Owned update failed");
    require(expenses.weekly("2026-12-31").total == 3700, "Updated report total is stale");
    require(!expenses.budget({2026, 12}).has_value(), "Missing budget confused with zero");
    expenses.setBudget({2026, 12}, 0);
    require(expenses.budget({2026, 12}) == 0, "Zero budget was not preserved");
    expenses.setBudget({2026, 12}, 5000);
    require(expenses.budget({2026, 12}) == 5000, "Budget upsert failed");
    rejects([&] { expenses.setBudget({2026, 13}, 10); }, "Invalid budget month accepted");
    rejects([&] { expenses.setBudget({2026, 12}, -1); }, "Negative budget accepted");

    const auto beforeBadImport = records.size();
    rejects([&] { expenses.importRows({{100, "Food", "2026-01-01", "valid"},
                                     {0, "Food", "2026-01-02", "invalid"}}); }, "Invalid all-or-nothing import accepted");
    require(records.size() == beforeBadImport && session.cacheReady(), "Invalid import partially changed records");

    if (const auto* rejectCategory = std::getenv("EXPENSE_TEST_REJECT_CATEGORY")) {
        const auto rowsBeforeTransaction = records.size();
        rejects([&] { expenses.importRows({{100, "Food", "2026-01-01", "must roll back"},
                                         {100, rejectCategory, "2026-01-02", "synthetic SQL rejection"}}); },
                "Test-trigger insertion unexpectedly succeeded");
        require(!session.cacheReady(), "Failed transaction left the cache readable");
        expenses.reload();
        require(records.size() == rowsBeforeTransaction, "Failed transaction partially inserted rows");
        std::cout << "Mid-transaction SQL rejection rollback check passed.\n";
    } else {
        std::cout << "Mid-transaction rejection fixture skipped: EXPENSE_TEST_REJECT_CATEGORY is not set.\n";
    }

    const auto generationBeforeLogout = session.generation();
    auth.logout();
    require(!session.authenticated() && !session.cacheReady() && records.size() == 0 &&
            session.generation() > generationBeforeLogout, "Logout did not clear all session records");
    checkSignedOut(expenses, reports);
    require(!auth.signIn(accountB, unicodePassword.substr(1, unicodePassword.size() - 2)),
            "Password verification silently trimmed whitespace");
    require(auth.signIn(accountB, unicodePassword), "Account B Unicode password login failed");
    require(session.requireUserId() != userA && records.size() == 0, "Account B loaded account A records");
    require(!expenses.searchExpense(monday) && !expenses.update(monday, {10, "Hack", "2026-01-01", "blocked"}) &&
            !expenses.erase(monday), "Account B accessed account A's known expense ID");
    require(!database.updateExpense(session, monday, {10, "Hack", "2026-01-01", "blocked"}) &&
            !database.deleteExpense(session, monday), "SQL ownership predicate was missing");
    require(expenses.weekly("2026-12-31").total == 0 && expenses.matching(ExpenseFilter{}).empty(),
            "Account B report/export source leaked account A data");
    require(!expenses.budget({2026, 12}), "Account B read account A's budget");
    expenses.setBudget({2026, 12}, 7000);
    expenses.importRows({{100, "Food", "2026-12-28", "B one"}, {200, "Other", "2026-12-30", "B two"}});
    require(expenses.total(ExpenseFilter{}) == 300 && records.size() == 2, "Imported records not scoped to account B");
    auth.logout();
    require(auth.signIn(accountA, password), "Account A relogin failed");
    require(expenses.searchExpense(monday)->amount() == 1200 && expenses.budget({2026, 12}) == 5000,
            "Account B changed account A data");
    require(records.size() == beforeBadImport, "Account B import leaked into account A list");

    require(expenses.erase(sunday), "Owned deletion failed");
    const int afterDeletion = expenses.add({1, "Other", "2026-01-01", "New ID after deletion"});
    require(afterDeletion > unicodeId && afterDeletion != sunday, "Deleted database ID reused");
    require(!expenses.searchExpense(sunday) && expenses.weekly("2026-12-31").total == 3200,
            "Deletion did not update cached records/reports");

    {
        DatabaseManager reopened;
        reopened.connect(config);
        Session newSession;
        ExpenseList newRecords;
        AuthService newAuth(reopened, newSession, newRecords);
        ExpenseService reopenedExpenses(newSession, reopened, newRecords);
        require(!newSession.authenticated(), "New process state bypassed login");
        rejects([&] { reopenedExpenses.matching(ExpenseFilter{}); }, "Restart exposed records before login");
        require(newAuth.signIn(accountA, password), "Restart login failed");
        require(reopenedExpenses.searchExpense(monday)->amount() == 1200 &&
                reopenedExpenses.searchExpense(monday)->description() == punctuation &&
                !reopenedExpenses.searchExpense(sunday) && reopenedExpenses.budget({2026, 12}) == 5000,
                "Saved CRUD/budget changes did not persist after reconnection");
    }

    const auto rowsBeforeFailure = records.size();
    database.disconnect();
    const auto failure = rejects([&] { expenses.add({400, "Other", "2026-12-31", "must not save"}); },
                                 "Disconnected write reported success");
    require(failure.find("could not be confirmed") != std::string::npos && !session.cacheReady() &&
            records.size() == rowsBeforeFailure, "Failed write changed memory or exposed stale cache");
    rejects([&] { expenses.matching(ExpenseFilter{}); }, "Failed write cache remained readable");
    rejects([&] { reports.weekly("2026-12-31"); }, "Failed write report remained readable");
    database.connect(config);
    expenses.reload();
    require(session.cacheReady() && records.size() == rowsBeforeFailure, "Reconnect/reload did not restore original rows");

    if (!writerConfigPath.empty()) {
        const auto writerConfig = loadDatabaseConfig(writerConfigPath);
        require(writerConfig.database == config.database && writerConfig.host == config.host && writerConfig.port == config.port,
                "Write-only test config points at a different database/server");
        database.disconnect(); database.connect(writerConfig);
        rejects([&] { expenses.update(monday, {999, "Other", "2026-12-29", "write denied"}); },
                "Write-only connection unexpectedly allowed UPDATE");
        require(!session.cacheReady(), "Denied SQL write left cache exposed");
        database.disconnect(); database.connect(config); expenses.reload();
        require(expenses.searchExpense(monday)->amount() == 1200, "Denied UPDATE changed the database row");
        const auto countBeforeCommittedInsert = records.size();
        database.disconnect(); database.connect(writerConfig);
        const auto refreshError = rejects([&] { expenses.add({77, "Other", "2026-12-31", "committed reload failure"}); },
                                          "Read-denied refresh unexpectedly succeeded");
        require(refreshError.find("change was saved") != std::string::npos && !session.cacheReady(),
                "Committed change/reload failure was misrepresented");
        rejects([&] { expenses.weekly("2026-12-31"); }, "Committed-change stale cache was readable");
        database.disconnect(); database.connect(config); expenses.reload();
        require(records.size() == countBeforeCommittedInsert + 1, "Committed insertion lost or duplicated after reload recovery");
        std::cout << "Confirmed-write/reload-failure and SQL permission failure checks passed.\n";
    } else {
        std::cout << "Write-only DB scenario skipped: EXPENSE_TEST_WRITE_ONLY_CONFIG is not set.\n";
    }

    require(!auth.signIn(accountA, "wrong fictional password"), "Wrong password accepted");
    require(!session.authenticated() && records.size() == 0, "Wrong password retained previous account data");
    require(!auth.signIn("missing_" + suffix, password), "Unknown username accepted");
    require(!auth.signIn("bad username", password), "Invalid username accepted");
    require(auth.retryAfter().count() > 0, "Repeated failures did not trigger retry delay");
    auth.logout();
    require(!auth.signIn(accountA, password), "Logout bypassed retry delay");
    std::this_thread::sleep_for(auth.retryAfter() + std::chrono::milliseconds{10});
    require(auth.signIn(accountA, password), "Correct password rejected after retry delay elapsed");
    database.disconnect();
    rejects([&] { auth.signIn(accountA, password); }, "Offline database produced authenticated login");
    require(!session.authenticated() && records.size() == 0, "Failed connection retained authenticated state");
    auto invalidConfig = config;
    invalidConfig.password += "invalid";
    DatabaseManager deniedConnection;
    rejects([&] { deniedConnection.connect(invalidConfig); }, "Incorrect DB credentials connected successfully");
    std::cout << "Persistence/account isolation checks passed: " << checks << ".\n";
}
} // namespace

int main(int argc, char* argv[]) {
    const auto* environmentConfig = std::getenv("EXPENSE_TEST_CONFIG");
    const std::filesystem::path configPath = argc > 1 && argv[1][0] != '\0'
        ? argv[1] : (environmentConfig ? environmentConfig : "");
    if (configPath.empty()) {
        std::cout << "SKIP: provide an isolated test configuration using argv[1] or EXPENSE_TEST_CONFIG.\n";
        return 77;
    }
    try {
        const auto config = expense_tracker::loadDatabaseConfig(configPath);
        if (config.database.rfind("expense_tracker_test_", 0) != 0) {
            throw std::runtime_error("Refusing integration writes: database must start with expense_tracker_test_.");
        }
        const auto* writerConfig = std::getenv("EXPENSE_TEST_WRITE_ONLY_CONFIG");
        runChecks(config, writerConfig ? std::filesystem::path(writerConfig) : std::filesystem::path{});
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
