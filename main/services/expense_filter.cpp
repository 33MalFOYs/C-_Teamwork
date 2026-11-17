#include "expense_tracker/expense_filter.hpp"
#include "expense_tracker/validation.hpp"

#include <stdexcept>
#include <utility>

namespace expense_tracker {

ExpenseFilter::ExpenseFilter() = default;

ExpenseFilter::ExpenseFilter(std::optional<DateRange> dates,
                             std::optional<std::string> category) {
    if (dates) {
        validation::validateDate(dates->start);
        validation::validateDate(dates->end);
        if (dates->start > dates->end) {
            throw std::invalid_argument("Start date must not be after end date.");
        }
    }
    if (category) {
        category = validation::normalizeCategory(*category);
    }
    dates_ = std::move(dates);
    category_ = std::move(category);
}

ExpenseFilter ExpenseFilter::exactDate(const std::string& date) {
    return ExpenseFilter(DateRange{date, date}, std::nullopt);
}

ExpenseFilter ExpenseFilter::thisWeek(const std::string& local_today) {
    return ExpenseFilter(validation::weekContaining(local_today), std::nullopt);
}

ExpenseFilter ExpenseFilter::thisMonth(const std::string& local_today) {
    return ExpenseFilter(validation::monthContaining(local_today), std::nullopt);
}

bool ExpenseFilter::matches(const Expense& expense) const {
    const bool date_matches = !dates_ ||
        (expense.date() >= dates_->start && expense.date() <= dates_->end);
    const bool category_matches = !category_ || expense.category() == *category_;
    return date_matches && category_matches;
}

} // namespace expense_tracker

