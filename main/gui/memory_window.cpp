#include "memory_window.hpp"
#include "theme.hpp"
#include "expense_page.hpp"
#include "reports_page.hpp"

#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

namespace expense_tracker::gui {

MemoryWindow::MemoryWindow(ExpenseWorkspace& expenses, QWidget* parent) : QWidget(parent) {
    applyTheme();
    setWindowTitle("Expense Tracker - Phase 2 temporary workspace");
    resize(1060, 820);
    setMinimumSize(850, 720);
    auto* layout = new QVBoxLayout(this);
    auto* banner = new QLabel("Phase 2 - temporary memory workspace\nNonpersistent: closing this window loses all records. No account is signed in.", this);
    banner->setObjectName("memoryBanner");
    banner->setWordWrap(true);
    banner->setStyleSheet("QLabel { background: #fff3cd; color: #4f3b00; padding: 12px; border-radius: 4px; }");
    layout->addWidget(banner);
    auto* tabs = new QTabWidget(this);
    tabs->setObjectName("workspaceTabs");
    expenses_ = new ExpensePage(expenses, tabs);
    reports_ = new ReportsPage(expenses, tabs);
    tabs->addTab(expenses_, "Expenses");
    tabs->addTab(reports_, "Reports");
    expenses_->setChangedHandler([this] { reports_->refresh(); });
    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 1) reports_->refresh();
    });
    layout->addWidget(tabs, 1);
}

} // namespace expense_tracker::gui

