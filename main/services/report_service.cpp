#include "expense_tracker/report_service.hpp"
#include "expense_tracker/validation.hpp"

namespace expense_tracker {

WeeklyReport ReportCalculator::weekly(const ExpenseList& records,
                                      const std::string& selected_date) {
    WeeklyReport report;
    report.dates = validation::weekContaining(selected_date);
    for (std::size_t day = 0; day < report.days.size(); ++day) {
        report.days[day].date = validation::addDays(report.dates.start, static_cast<int>(day));
    }
    const ExpenseFilter filter(report.dates, std::nullopt);
    records.forEach([&](const Expense& expense) {
        if (!filter.matches(expense)) return;
        report.total = validation::checkedAdd(report.total, expense.amount());
        ++report.count;
        auto& category_total = report.categories[expense.category()];
        category_total = validation::checkedAdd(category_total, expense.amount());
        for (auto& day : report.days) {
            if (day.date == expense.date()) {
                day.total = validation::checkedAdd(day.total, expense.amount());
                break;
            }
        }
    });
    return report;
}

MonthlyReport ReportCalculator::monthly(const ExpenseList& records, BudgetPeriod period) {
    validation::validatePeriod(period);
    const std::string first_day = std::to_string(period.year) + "-" +
        (period.month < 10 ? "0" : "") + std::to_string(period.month) + "-01";
    const ExpenseFilter filter(validation::monthContaining(first_day), std::nullopt);
    MonthlyReport report;
    report.period = period;
    records.forEach([&](const Expense& expense) {
        if (!filter.matches(expense)) return;
        report.total = validation::checkedAdd(report.total, expense.amount());
        ++report.count;
        auto& category_total = report.categories[expense.category()];
        category_total = validation::checkedAdd(category_total, expense.amount());
    });
    return report;
}

ReportService::ReportService(const Session& session, const ExpenseList& records)
    : session_(session), records_(records) {}

WeeklyReport ReportService::weekly(const std::string& selected_date) const {
    session_.requireReadableCache();
    return ReportCalculator::weekly(records_, selected_date);
}

MonthlyReport ReportService::monthly(BudgetPeriod period) const {
    session_.requireReadableCache();
    return ReportCalculator::monthly(records_, period);
}

} // namespace expense_tracker

