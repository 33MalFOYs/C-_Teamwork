#pragma once

#include <QWidget>

namespace expense_tracker {
class ExpenseWorkspace;
}

namespace expense_tracker::gui {
class ExpensePage;
class ReportsPage;

class MemoryWindow final : public QWidget {
public:
    explicit MemoryWindow(ExpenseWorkspace& expenses, QWidget* parent = nullptr);
private:
    ExpensePage* expenses_;
    ReportsPage* reports_;
};
} // namespace expense_tracker::gui

