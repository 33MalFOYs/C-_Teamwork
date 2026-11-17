#include "expense_tracker/expense_list.hpp"
#include "expense_tracker/expense_filter.hpp"
#include "expense_tracker/validation.hpp"

#include <stdexcept>
#include <utility>

namespace expense_tracker {

ExpenseList::ExpenseList() = default;
ExpenseList::~ExpenseList() { clear(); }

void ExpenseList::insert(const Expense& expense) {
    if (expense.id() <= 0) throw std::invalid_argument("An unset expense cannot be inserted.");
    Expense::validate(ExpenseDraft{expense.amount(), expense.category(), expense.date(), expense.description()});
    if (searchExpense(expense.id())) throw std::invalid_argument("An expense with that ID already exists.");
    // Build the node first. An allocation/copy failure leaves the list intact.
    auto* node = new ExpenseNode{expense, nullptr};
    auto** link = &head_;
    while (*link) link = &(*link)->next;
    *link = node;
    ++size_;
}

const Expense* ExpenseList::searchExpense(int id) const noexcept {
    for (auto* node = head_; node; node = node->next) {
        if (node->expense.id() == id) return &node->expense;
    }
    return nullptr;
}

std::vector<Expense> ExpenseList::searchExpense(const std::string& category) const {
    const auto normalized = validation::normalizeCategory(category);
    std::vector<Expense> result;
    for (auto* node = head_; node; node = node->next) {
        if (node->expense.category() == normalized) result.push_back(node->expense);
    }
    return result;
}

bool ExpenseList::update(int id, const ExpenseDraft& replacement) {
    for (auto* node = head_; node; node = node->next) {
        if (node->expense.id() == id) {
            node->expense.update(replacement);
            return true;
        }
    }
    return false;
}

bool ExpenseList::erase(int id) noexcept {
    auto** link = &head_;
    while (*link) {
        if ((*link)->expense.id() == id) {
            auto* removed = *link;
            *link = removed->next;
            delete removed;
            --size_;
            return true;
        }
        link = &(*link)->next;
    }
    return false;
}

void ExpenseList::forEach(const std::function<void(const Expense&)>& visitor) const {
    for (auto* node = head_; node; node = node->next) visitor(node->expense);
}

std::vector<Expense> ExpenseList::filter(const ExpenseFilter& criteria) const {
    std::vector<Expense> result;
    for (auto* node = head_; node; node = node->next) {
        if (criteria.matches(node->expense)) result.push_back(node->expense);
    }
    return result;
}

Money ExpenseList::total(const ExpenseFilter& criteria) const {
    Money result = 0;
    for (auto* node = head_; node; node = node->next) {
        if (criteria.matches(node->expense)) result = validation::checkedAdd(result, node->expense.amount());
    }
    return result;
}

void ExpenseList::clear() noexcept {
    // Iteration avoids recursive destruction and its stack limit on large lists.
    while (head_) {
        auto* removed = head_;
        head_ = head_->next;
        delete removed;
    }
    size_ = 0;
}

void ExpenseList::swap(ExpenseList& other) noexcept {
    std::swap(head_, other.head_);
    std::swap(size_, other.size_);
}

} // namespace expense_tracker
