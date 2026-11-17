#pragma once
#include "persistent_controller.hpp"
#include <QMainWindow>
#include <filesystem>

class QStackedWidget;
class QLabel;
class QPushButton;
class QLineEdit;
class QTableWidget;
class QProgressBar;
namespace expense_tracker::gui {
class LoginWindow;
class ExpensePage;
class ReportsPage;
class TrendChart;
class CategoryChart;

class MainWindow final : public QMainWindow {
public:
    explicit MainWindow(std::filesystem::path configuration = "config.local", QWidget* parent = nullptr);
    PersistentController& controller() noexcept { return controller_; }
private:
    void updateState(const GuiSessionState& state);
    void createFinancialViews();
    void discardFinancialViews();
    void refreshDashboard();
    void saveCurrentBudget();
    void clearDashboard();
    std::filesystem::path configuration_;
    PersistentController controller_;
    QStackedWidget* pages_;
    LoginWindow* login_;
    QWidget* financial_ = nullptr;
    ExpensePage* expenses_ = nullptr;
    ReportsPage* reports_ = nullptr;
    QWidget* financial_content_ = nullptr;
    QLabel *identity_ = nullptr, *dashboard_ = nullptr, *financial_feedback_ = nullptr;
    QLineEdit* budget_amount_ = nullptr;
    QPushButton* reload_ = nullptr;
    QLabel *week_amount_ = nullptr, *month_amount_ = nullptr, *budget_status_ = nullptr;
    QLabel *week_caption_ = nullptr, *month_caption_ = nullptr;
    QTableWidget* recent_ = nullptr;
    QProgressBar* budget_progress_ = nullptr;
    TrendChart* trend_ = nullptr;
    CategoryChart* categories_ = nullptr;
    QLabel *entry_count_ = nullptr, *budget_period_ = nullptr;
    int displayed_user_ = 0;
};
}
