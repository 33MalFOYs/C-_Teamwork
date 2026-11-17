#pragma once

#include <cstdint>
#include <string>

namespace expense_tracker {

using Money = std::int64_t; // USD cents; never parse through floating point.
inline constexpr Money maximum_amount = 999999999999LL; // DECIMAL(12,2).
inline constexpr int minimum_year = 1000;
inline constexpr int maximum_year = 9999;
inline constexpr const char* currency = "USD";

// Phase 2 validates positive amount, text lengths, and a real YYYY-MM-DD date.
// A draft never accepts a user_id or a source database ID.
struct ExpenseDraft {
    Money amount = 0;
    std::string category;
    std::string date;
    std::string description;
};

struct DateRange {
    std::string start;
    std::string end; // Inclusive; both dates required and start <= end.
};

struct BudgetPeriod {
    int year = 0;
    int month = 0; // Validate 1..12 and the supported year range.
};

} // namespace expense_tracker
