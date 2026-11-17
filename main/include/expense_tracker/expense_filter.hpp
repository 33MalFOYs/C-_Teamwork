#pragma once

#include "expense_tracker/expense.hpp"

#include <optional>

namespace expense_tracker {

// Pure Phase 2 predicate: shared by table rows/counts/totals and CSV exports.
// Service callers enforce the session; isolated unit fixtures need no login.
class ExpenseFilter {
public:
    ExpenseFilter(); // All dates/categories.
    ExpenseFilter(std::optional<DateRange> dates, std::optional<std::string> category);
    static ExpenseFilter exactDate(const std::string& date);
    static ExpenseFilter thisWeek(const std::string& local_today); // Mon..Sun.
    static ExpenseFilter thisMonth(const std::string& local_today);
    bool matches(const Expense& expense) const;
    const std::optional<DateRange>& dates() const noexcept { return dates_; }
    const std::optional<std::string>& category() const noexcept { return category_; }

private:
    std::optional<DateRange> dates_;
    std::optional<std::string> category_; // AND with inclusive date criteria.
};

} // namespace expense_tracker
