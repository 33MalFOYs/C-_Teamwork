#include "expense_tracker/expense_service.hpp"
#include "expense_tracker/validation.hpp"

#include <stdexcept>

namespace expense_tracker {
namespace {
void requirePositiveId(int id) {
    if (id <= 0) throw std::invalid_argument("Expense ID must be positive.");
}
}

ExpenseService::ExpenseService(Session& session, DatabaseManager& database, ExpenseList& records)
    : session_(session), database_(database), records_(records) {}

void ExpenseService::reload() {
    session_.requireUserId();
    session_.cache_ready_ = false;
    ++session_.generation_;
    database_.loadExpenses(session_, records_); // DatabaseManager publishes a full list atomically.
    session_.cache_ready_ = true;
    ++session_.generation_;
}

void ExpenseService::refreshAfterCommit() {
    try {
        reload();
    } catch (const std::exception&) {
        // reload() already invalidated the shared cache. Do not imply the
        // committed database write was rolled back or allow stale reports.
        throw std::runtime_error("The database change was saved, but expense data could not reload. "
                                 "Reload successfully before continuing; do not repeat the saved change.");
    }
}

void ExpenseService::failUnconfirmedWrite() {
    // A lost connection can make the commit outcome uncertain. Keep the old
    // nodes only for safe cleanup; all financial reads are blocked until reload.
    session_.cache_ready_ = false;
    ++session_.generation_;
    throw std::runtime_error("The database save could not be confirmed. Reload before continuing "
                             "and verify the records before retrying the change.");
}

int ExpenseService::add(const ExpenseDraft& expense) {
    session_.requireReadableCache();
    Expense::validate(expense);
    int id = 0;
    try { id = database_.insertExpense(session_, expense); }
    catch (const std::exception&) { failUnconfirmedWrite(); }
    refreshAfterCommit();
    return id;
}

bool ExpenseService::update(int id, const ExpenseDraft& expense) {
    session_.requireReadableCache();
    requirePositiveId(id);
    Expense::validate(expense);
    bool updated = false;
    try { updated = database_.updateExpense(session_, id, expense); }
    catch (const std::exception&) { failUnconfirmedWrite(); }
    if (updated) refreshAfterCommit();
    return updated;
}

bool ExpenseService::erase(int id) {
    session_.requireReadableCache();
    requirePositiveId(id);
    bool erased = false;
    try { erased = database_.deleteExpense(session_, id); }
    catch (const std::exception&) { failUnconfirmedWrite(); }
    if (erased) refreshAfterCommit();
    return erased;
}

void ExpenseService::importRows(const std::vector<ExpenseDraft>& rows) {
    session_.requireReadableCache();
    for (const auto& row : rows) Expense::validate(row);
    if (rows.empty()) return;
    try { (void)database_.importExpenses(session_, rows); }
    catch (const std::exception&) { failUnconfirmedWrite(); }
    refreshAfterCommit();
}

std::optional<Expense> ExpenseService::searchExpense(int id) const {
    session_.requireReadableCache();
    requirePositiveId(id);
    const auto* expense = records_.searchExpense(id);
    if (!expense) return std::nullopt;
    return *expense;
}

std::vector<Expense> ExpenseService::searchExpense(const std::string& category) const {
    session_.requireReadableCache();
    return records_.searchExpense(category);
}

std::vector<Expense> ExpenseService::matching(const ExpenseFilter& filter) const {
    session_.requireReadableCache();
    return records_.filter(filter);
}

Money ExpenseService::total(const ExpenseFilter& filter) const {
    session_.requireReadableCache();
    return records_.total(filter);
}

WeeklyReport ExpenseService::weekly(const std::string& selected_date) const {
    session_.requireReadableCache();
    return ReportCalculator::weekly(records_, selected_date);
}

MonthlyReport ExpenseService::monthly(BudgetPeriod period) const {
    session_.requireReadableCache();
    return ReportCalculator::monthly(records_, period);
}

std::optional<Money> ExpenseService::budget(BudgetPeriod period) const {
    session_.requireReadableCache();
    validation::validatePeriod(period);
    return database_.loadBudget(session_, period);
}

void ExpenseService::setBudget(BudgetPeriod period, Money amount) {
    session_.requireReadableCache();
    validation::validatePeriod(period);
    if (amount < 0 || amount > maximum_amount) {
        throw std::invalid_argument("Budget must be between 0.00 and 9999999999.99 USD.");
    }
    try { database_.saveBudget(session_, period, amount); }
    catch (const std::exception&) { failUnconfirmedWrite(); }
    ++session_.generation_;
}

} // namespace expense_tracker
