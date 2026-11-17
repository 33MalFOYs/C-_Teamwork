#pragma once

#include <QWidget>
#include <string>

class QDateEdit;
class QLabel;
class QTableWidget;

namespace expense_tracker {
class ExpenseWorkspace;
struct WeeklyReport;
struct MonthlyReport;
}

namespace expense_tracker::gui {

class TrendChart;
class CategoryChart;

class ReportsPage final : public QWidget {
public:
    explicit ReportsPage(ExpenseWorkspace& expenses, QWidget* parent = nullptr);
    void refresh();
    void clearSessionData();

private:
    void selectWeek(const std::string& date);
    void moveWeek(int days);
    void renderWeekly(const WeeklyReport& report);
    void refreshMonth();
    void renderMonthly(const MonthlyReport& report);

    ExpenseWorkspace& expenses_;
    std::string selected_week_date_;
    QDateEdit* week_date_;
    QDateEdit* month_date_;
    QLabel* week_range_;
    QLabel* week_summary_;
    QLabel* month_summary_;
    QLabel* feedback_;
    QTableWidget* daily_;
    QTableWidget* weekly_categories_;
    QTableWidget* monthly_categories_;
    TrendChart* weekly_trend_;
    CategoryChart* weekly_mix_;
    CategoryChart* monthly_mix_;
};

} // namespace expense_tracker::gui

