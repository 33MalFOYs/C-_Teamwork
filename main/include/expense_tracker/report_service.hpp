#pragma once

#include "expense_tracker/expense_filter.hpp"
#include "expense_tracker/expense_list.hpp"
#include "expense_tracker/session.hpp"

#include <array>
#include <map>

namespace expense_tracker {

struct DailyTotal {
    std::string date;
    Money total = 0;
};

struct WeeklyReport {
    DateRange dates;
    Money total = 0;
    std::size_t count = 0;
    std::array<DailyTotal, 7> days; // Include zero-spending days, Monday first.
    std::map<std::string, Money> categories;
    // Average is total / 7; retain integer total and round only at display time.
};

struct MonthlyReport {
    BudgetPeriod period;
    Money total = 0;
    std::size_t count = 0;
    std::map<std::string, Money> categories;
};

// Pure calculations over a supplied SLL. Used by the isolated Phase 2 workspace
// and by the authenticated service below; no GUI/database dependency.
class ReportCalculator {
public:
    static WeeklyReport weekly(const ExpenseList& records, const std::string& selected_date);
    static MonthlyReport monthly(const ExpenseList& records, BudgetPeriod period);
};

// Every public report checks Session::requireReadableCache(), so a failed cache
// refresh blocks reports as well as expense views until a reload succeeds.
class ReportService {
public:
    ReportService(const Session& session, const ExpenseList& records);
    WeeklyReport weekly(const std::string& selected_date) const;
    MonthlyReport monthly(BudgetPeriod period) const;

private:
    const Session& session_;
    const ExpenseList& records_;
};

} // namespace expense_tracker
