#pragma once

#include "expense_tracker/report_service.hpp"

#include <optional>

namespace expense_tracker {

// Presentation-facing contract shared by the CLI and Qt forms. Phase 2 uses an
// explicitly temporary MemoryExpenseService. Phase 3's ExpenseService will
// implement this interface with authenticated, database-first operations.
class ExpenseWorkspace {
public:
    virtual ~ExpenseWorkspace() = default;
    virtual int add(const ExpenseDraft& expense) = 0;
    virtual bool update(int id, const ExpenseDraft& expense) = 0;
    virtual bool erase(int id) = 0;
    virtual std::optional<Expense> searchExpense(int id) const = 0;
    virtual std::vector<Expense> searchExpense(const std::string& category) const = 0;
    virtual std::vector<Expense> matching(const ExpenseFilter& filter) const = 0;
    virtual Money total(const ExpenseFilter& filter) const = 0;
    virtual WeeklyReport weekly(const std::string& selected_date) const = 0;
    virtual MonthlyReport monthly(BudgetPeriod period) const = 0;
};

} // namespace expense_tracker

