#pragma once

#include "expense_tracker/expense_workspace.hpp"

namespace expense_tracker {

// Explicit Phase 2 scratch workspace. It has no account/session, database,
// login capability, or automatic fixture loading. Destruction loses all data.
class MemoryExpenseService final : public ExpenseWorkspace {
public:
    int add(const ExpenseDraft& expense) override;
    bool update(int id, const ExpenseDraft& expense) override;
    bool erase(int id) override;
    std::optional<Expense> searchExpense(int id) const override;
    std::vector<Expense> searchExpense(const std::string& category) const override;
    std::vector<Expense> matching(const ExpenseFilter& filter) const override;
    Money total(const ExpenseFilter& filter) const override;
    WeeklyReport weekly(const std::string& selected_date) const override;
    MonthlyReport monthly(BudgetPeriod period) const override;

private:
    ExpenseList records_;
    std::int64_t next_id_ = 1; // Increment only after successful insertion.
};

} // namespace expense_tracker

