#pragma once

#include "expense_tracker/types.hpp"

namespace expense_tracker {

// A validated expense value. Private fields cannot bypass the update contract.
class Expense {
public:
    Expense(); // An unset draft: not insertable until validated.
    Expense(int id, Money amount, std::string category, std::string date,
            std::string description);

    int id() const noexcept { return id_; }
    Money amount() const noexcept { return amount_; }
    const std::string& category() const noexcept { return category_; }
    const std::string& date() const noexcept { return date_; }
    const std::string& description() const noexcept { return description_; }

    // Validate the whole replacement before changing any field; preserve ID.
    void update(const ExpenseDraft& replacement);
    static void validate(const ExpenseDraft& draft);

private:
    int id_ = 0;
    Money amount_ = 0;
    std::string category_;
    std::string date_;
    std::string description_;
};

} // namespace expense_tracker
