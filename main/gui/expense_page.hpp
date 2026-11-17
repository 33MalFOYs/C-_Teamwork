#pragma once

#include "expense_tracker/expense_filter.hpp"
#include <QWidget>
#include <exception>
#include <functional>
#include <optional>

class QComboBox;
class QBoxLayout;
class QDateEdit;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QScrollArea;
class QResizeEvent;

namespace expense_tracker {
class ExpenseWorkspace;
}

namespace expense_tracker::gui {

// Presentation shared by the explicit temporary workspace and future
// authenticated adapters. All validation/calculation lives in the core.
class ExpensePage final : public QWidget {
public:
    using Completion = std::function<void(const std::string&)>;
    struct AsyncHandlers {
        std::function<void(std::optional<int>, ExpenseDraft, Completion)> save;
        std::function<void(int, Completion)> erase;
        std::function<void(Completion)> reload;
    };
    explicit ExpensePage(ExpenseWorkspace& expenses, QWidget* parent = nullptr);
    bool refresh();
    ExpenseFilter activeFilter() const { return active_filter_; }
    void clearSessionData();
    void setChangedHandler(std::function<void()> handler);
    void setAsyncHandlers(AsyncHandlers handlers);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void applyFilters();
    void clearFilters();
    void updateDateControls();
    void editSelected();
    void deleteSelected();
    void saveExpense();
    void resetForm();
    std::optional<int> selectedId() const;
    void showError(const std::exception& error);
    void reloadRequested();
    void asyncFinished(const std::string& error, const QString& success, bool reset);

    ExpenseWorkspace& expenses_;
    ExpenseFilter active_filter_;
    std::optional<int> editing_id_;
    std::function<void()> changed_;
    AsyncHandlers async_;
    QComboBox* date_mode_;
    QLineEdit* filter_category_;
    QDateEdit* filter_start_;
    QDateEdit* filter_end_;
    QTableWidget* table_;
    QLabel* total_;
    QLabel* empty_;
    QLabel* feedback_;
    QGroupBox* form_;
    QPushButton* save_;
    QLineEdit* amount_;
    QLineEdit* category_;
    QDateEdit* date_;
    QLineEdit* description_;
    QBoxLayout* body_ = nullptr;
    QScrollArea* scroll_ = nullptr;
    QWidget* empty_state_ = nullptr;
};

} // namespace expense_tracker::gui

