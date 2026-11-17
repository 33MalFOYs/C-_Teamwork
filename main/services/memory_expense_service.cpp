#include "expense_tracker/memory_expense_service.hpp"

#include <limits>
#include <stdexcept>

namespace expense_tracker {
namespace {
void requirePositiveId(int id) {
    if (id <= 0) throw std::invalid_argument("Expense ID must be positive.");
}
}

int MemoryExpenseService::add(const ExpenseDraft& draft) {
    if (next_id_ > std::numeric_limits<int>::max()) {
        throw std::overflow_error("No more temporary expense IDs are available.");
    }
    const auto id = static_cast<int>(next_id_);
    records_.insert(Expense(id, draft.amount, draft.category, draft.date, draft.description));
    ++next_id_;
    return id;
}

bool MemoryExpenseService::update(int id, const ExpenseDraft& expense) {
    requirePositiveId(id);
    return records_.update(id, expense);
}

bool MemoryExpenseService::erase(int id) {
    requirePositiveId(id);
    return records_.erase(id);
}

std::optional<Expense> MemoryExpenseService::searchExpense(int id) const {
    requirePositiveId(id);
    const auto* record = records_.searchExpense(id);
    if (!record) return std::nullopt;
    return *record;
}

std::vector<Expense> MemoryExpenseService::searchExpense(const std::string& category) const {
    return records_.searchExpense(category);
}

std::vector<Expense> MemoryExpenseService::matching(const ExpenseFilter& filter) const {
    return records_.filter(filter);
}

Money MemoryExpenseService::total(const ExpenseFilter& filter) const {
    return records_.total(filter);
}

WeeklyReport MemoryExpenseService::weekly(const std::string& selected_date) const {
    return ReportCalculator::weekly(records_, selected_date);
}

MonthlyReport MemoryExpenseService::monthly(BudgetPeriod period) const {
    return ReportCalculator::monthly(records_, period);
}

} // namespace expense_tracker

