#pragma once

#include "expense_tracker/expense.hpp"

#include <cstddef>
#include <functional>
#include <vector>

namespace expense_tracker {

class ExpenseFilter;

struct ExpenseNode {
    Expense expense;
    ExpenseNode* next = nullptr; // Exactly one link: this is a singly linked list.
};

// Owns every node and releases them iteratively in clear/destructor.
class ExpenseList {
public:
    ExpenseList();
    ~ExpenseList();
    ExpenseList(const ExpenseList&) = delete;
    ExpenseList& operator=(const ExpenseList&) = delete;
    ExpenseList(ExpenseList&&) = delete;
    ExpenseList& operator=(ExpenseList&&) = delete;

    void insert(const Expense& expense); // Reject invalid/duplicate IDs.
    const Expense* searchExpense(int id) const noexcept;
    std::vector<Expense> searchExpense(const std::string& category) const;
    bool update(int id, const ExpenseDraft& replacement);
    bool erase(int id) noexcept;
    // Visitors must not mutate or clear the same list while traversing it.
    void forEach(const std::function<void(const Expense&)>& visitor) const;
    std::vector<Expense> filter(const ExpenseFilter& filter) const;
    Money total(const ExpenseFilter& filter) const;
    void clear() noexcept;
    void swap(ExpenseList& other) noexcept; // Publish a fully loaded list atomically.
    std::size_t size() const noexcept { return size_; }

private:
    ExpenseNode* head_ = nullptr;
    std::size_t size_ = 0;
};

// ID lookup borrows one record (or nullptr), invalidated by mutation/clear.
// Category/filter searches return independent value snapshots. The owning
// working store is still the SLL, never a vector or a standard linked list.

} // namespace expense_tracker
