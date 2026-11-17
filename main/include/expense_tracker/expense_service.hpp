#pragma once

#include "expense_tracker/database_manager.hpp"
#include "expense_tracker/expense_workspace.hpp"

namespace expense_tracker {

// Phase 3 shared GUI/CLI coordinator. Require session on every operation.
// Validate -> database commit -> update/reload SLL. If reload fails after a
// commit, invalidate cached reads until reload succeeds; never claim failure
// means the already-committed database mutation was rolled back.
class ExpenseService : public ExpenseWorkspace {
public:
    ExpenseService(Session& session, DatabaseManager& database, ExpenseList& records);
    void reload();
    int add(const ExpenseDraft& expense) override;
    bool update(int id, const ExpenseDraft& expense) override;
    bool erase(int id) override; // UI obtains deletion confirmation before calling.
    void importRows(const std::vector<ExpenseDraft>& rows); // One DB transaction.
    std::optional<Expense> searchExpense(int id) const override;
    std::vector<Expense> searchExpense(const std::string& category) const override;
    std::vector<Expense> matching(const ExpenseFilter& filter) const override;
    Money total(const ExpenseFilter& filter) const override;
    WeeklyReport weekly(const std::string& selected_date) const override;
    MonthlyReport monthly(BudgetPeriod period) const override;
    std::optional<Money> budget(BudgetPeriod period) const;
    void setBudget(BudgetPeriod period, Money amount);

private:
    void refreshAfterCommit();
    [[noreturn]] void failUnconfirmedWrite();
    Session& session_;
    DatabaseManager& database_;
    ExpenseList& records_;
    // Readiness belongs to Session and gates reports/CSV as well as this service.
};

} // namespace expense_tracker
