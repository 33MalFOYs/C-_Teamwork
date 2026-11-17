#include "expense_tracker/expense.hpp"
#include "expense_tracker/validation.hpp"

#include <stdexcept>
#include <utility>

namespace expense_tracker {

Expense::Expense() = default;

Expense::Expense(int id, Money amount, std::string category, std::string date,
                 std::string description) : id_(id) {
    if (id <= 0) throw std::invalid_argument("Expense ID must be greater than zero.");
    update(ExpenseDraft{amount, std::move(category), std::move(date), std::move(description)});
}

void Expense::validate(const ExpenseDraft& draft) {
    if (draft.amount <= 0 || draft.amount > maximum_amount) {
        throw std::invalid_argument("Expense amount must be between 0.01 and 9999999999.99 USD.");
    }
    (void)validation::normalizeCategory(draft.category);
    validation::validateDate(draft.date);
    validation::validateDescription(draft.description);
}

void Expense::update(const ExpenseDraft& replacement) {
    validate(replacement);
    // Complete every potentially throwing validation/allocation before changing
    // this record. Swapping std::string fields is noexcept; the ID is preserved.
    auto category = validation::normalizeCategory(replacement.category);
    auto date = replacement.date;
    auto description = replacement.description;
    category_.swap(category);
    date_.swap(date);
    description_.swap(description);
    amount_ = replacement.amount;
}

} // namespace expense_tracker
