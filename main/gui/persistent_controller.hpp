#pragma once

#include "expense_tracker/expense_workspace.hpp"
#include <QObject>
#include <QThread>
#include <filesystem>
#include <functional>
#include <memory>

namespace expense_tracker::gui {

// UI-thread cache populated only by a successful authenticated worker response.
// The worker's authenticated ExpenseService remains the authoritative service.
class SnapshotWorkspace final : public ExpenseWorkspace {
public:
    void replace(const std::vector<Expense>& values);
    void invalidate() noexcept;
    int add(const ExpenseDraft&) override;
    bool update(int, const ExpenseDraft&) override;
    bool erase(int) override;
    std::optional<Expense> searchExpense(int id) const override;
    std::vector<Expense> searchExpense(const std::string& category) const override;
    std::vector<Expense> matching(const ExpenseFilter& filter) const override;
    Money total(const ExpenseFilter& filter) const override;
    WeeklyReport weekly(const std::string& selected_date) const override;
    MonthlyReport monthly(BudgetPeriod period) const override;
private:
    void requireReady() const;
    ExpenseList records_;
    bool ready_ = false;
};

struct GuiSessionState {
    bool connected = false;
    bool busy = false;
    bool cache_ready = false;
    std::optional<User> user;
    std::optional<Money> current_month_budget;
    BudgetPeriod current_month;
    std::string error;
    int retry_milliseconds = 0;
};

class PersistentController final : public QObject {
public:
    using Completion = std::function<void(const std::string& error)>;
    explicit PersistentController(QObject* parent = nullptr);
    ~PersistentController() override;
    void setStateHandler(std::function<void(const GuiSessionState&)> handler);
    const GuiSessionState& state() const noexcept { return state_; }
    SnapshotWorkspace& workspace() noexcept { return workspace_; }
    void initialize(const std::filesystem::path& configuration);
    void authenticate(bool registration, std::string username, std::string password,
                      std::string confirmation, Completion completion = {});
    void csv(int action, std::filesystem::path path, std::string date, bool overwrite, Completion completion, ExpenseFilter filter = ExpenseFilter{});
    void logout();
    void save(std::optional<int> id, ExpenseDraft draft, Completion completion);
    void erase(int id, Completion completion);
    void reload(Completion completion = {});
    void setBudget(BudgetPeriod period, Money amount, Completion completion);
private:
    struct Worker;
    struct Response;
    void post(std::function<void(Worker&)> action, Completion completion, bool lifecycle = false);
    void receive(Response response, std::uint64_t epoch, Completion completion);
    void notify();
    void clearAccount();
    QThread thread_;
    Worker* worker_;
    std::uint64_t epoch_ = 0;
    SnapshotWorkspace workspace_;
    GuiSessionState state_;
    std::function<void(const GuiSessionState&)> changed_;
};

} // namespace expense_tracker::gui
