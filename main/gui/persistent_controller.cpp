#include "persistent_controller.hpp"
#include "expense_tracker/auth_service.hpp"
#include "expense_tracker/config.hpp"
#include "expense_tracker/expense_service.hpp"
#include "expense_tracker/validation.hpp"
#include "expense_tracker/file_manager.hpp"
#include <QMetaObject>
#include <QPointer>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace expense_tracker::gui {
namespace {
void rejectDirectWrite() { throw std::logic_error("Use the asynchronous database operation."); }
// Queued credentials are temporary request data; erase their buffers after use.
void eraseText(std::string& value) noexcept {
    volatile char* bytes = value.empty() ? nullptr : &value[0];
    for (std::size_t i = 0; i < value.size(); ++i) bytes[i] = 0;
    value.clear();
}
struct Credentials {
    std::string username, password, confirmation;
    ~Credentials() { eraseText(password); eraseText(confirmation); }
};
}

void SnapshotWorkspace::replace(const std::vector<Expense>& values) {
    ExpenseList replacement;
    for (const auto& expense : values) replacement.insert(expense);
    records_.swap(replacement);
    ready_ = true;
}
void SnapshotWorkspace::invalidate() noexcept { ready_ = false; records_.clear(); }
void SnapshotWorkspace::requireReady() const {
    if (!ready_) throw std::logic_error("Account data is unavailable. Reload or sign in again.");
}
int SnapshotWorkspace::add(const ExpenseDraft&) { rejectDirectWrite(); return 0; }
bool SnapshotWorkspace::update(int, const ExpenseDraft&) { rejectDirectWrite(); return false; }
bool SnapshotWorkspace::erase(int) { rejectDirectWrite(); return false; }
std::optional<Expense> SnapshotWorkspace::searchExpense(int id) const {
    requireReady(); const auto* result = records_.searchExpense(id);
    return result ? std::optional<Expense>(*result) : std::nullopt;
}
std::vector<Expense> SnapshotWorkspace::searchExpense(const std::string& category) const {
    requireReady(); return records_.searchExpense(category);
}
std::vector<Expense> SnapshotWorkspace::matching(const ExpenseFilter& filter) const {
    requireReady(); return records_.filter(filter);
}
Money SnapshotWorkspace::total(const ExpenseFilter& filter) const {
    requireReady(); return records_.total(filter);
}
WeeklyReport SnapshotWorkspace::weekly(const std::string& date) const {
    requireReady(); return ReportCalculator::weekly(records_, date);
}
MonthlyReport SnapshotWorkspace::monthly(BudgetPeriod period) const {
    requireReady(); return ReportCalculator::monthly(records_, period);
}

struct PersistentController::Worker : QObject {
    struct Services {
        DatabaseManager database;
        Session session;
        ExpenseList records;
        AuthService auth{database, session, records};
        ExpenseService expenses{session, database, records};
    };
    // Services (including Argon2 initialization) are created and destroyed here,
    // on the worker thread. No SQL connection crosses thread boundaries.
    std::unique_ptr<Services> services;
    std::uint64_t epoch = 0;
};
struct PersistentController::Response {
    GuiSessionState state;
    std::vector<Expense> records;
};

PersistentController::PersistentController(QObject* parent) : QObject(parent), worker_(new Worker) {
    worker_->moveToThread(&thread_);
    connect(&thread_, &QThread::finished, worker_, &QObject::deleteLater);
    thread_.start();
}
PersistentController::~PersistentController() {
    ++epoch_;
    // Finish a bounded outstanding SQL call, then destroy worker-owned services.
    // This wait is shutdown only; normal UI operations always return immediately.
    thread_.quit(); thread_.wait();
}
void PersistentController::setStateHandler(std::function<void(const GuiSessionState&)> handler) {
    changed_ = std::move(handler);
}
void PersistentController::notify() { if (changed_) changed_(state_); }
void PersistentController::clearAccount() {
    workspace_.invalidate(); state_.user.reset(); state_.cache_ready = false;
    state_.current_month_budget.reset(); state_.current_month = {};
}
void PersistentController::post(std::function<void(Worker&)> action, Completion completion, bool lifecycle) {
    const auto request_epoch = epoch_;
    state_.busy = true; state_.error.clear(); notify();
    QPointer<PersistentController> recipient(this);
    auto* worker = worker_;
    QMetaObject::invokeMethod(worker, [worker, recipient, request_epoch, action = std::move(action),
                                     completion = std::move(completion), lifecycle]() mutable {
        Response result;
        try {
            if (lifecycle) worker->epoch = request_epoch;
            else if (worker->epoch != request_epoch) throw std::logic_error("Account session changed. Sign in again.");
            if (!worker->services) worker->services = std::make_unique<Worker::Services>();
            action(*worker);
        } catch (const std::exception& error) { result.state.error = error.what(); }
        if (worker->services) {
            auto& services = *worker->services;
            result.state.connected = services.database.connected();
            result.state.retry_milliseconds = static_cast<int>(std::min<std::int64_t>(
                services.auth.retryAfter().count(), std::numeric_limits<int>::max()));
            if (services.session.currentUser()) result.state.user = *services.session.currentUser();
            result.state.cache_ready = services.session.cacheReady();
            if (result.state.cache_ready) {
                try {
                    result.records = services.expenses.matching(ExpenseFilter{});
                    const auto today = validation::localToday();
                    result.state.current_month = {std::stoi(today.substr(0, 4)), std::stoi(today.substr(5, 2))};
                    result.state.current_month_budget = services.expenses.budget(result.state.current_month);
                } catch (const std::exception& error) {
                    result.state.cache_ready = false; result.records.clear();
                    if (result.state.error.empty()) result.state.error = error.what();
                }
            }
        }
        if (recipient) QMetaObject::invokeMethod(recipient, [recipient, request_epoch,
                    result = std::move(result), completion = std::move(completion)]() mutable {
            if (recipient) recipient->receive(std::move(result), request_epoch, std::move(completion));
        }, Qt::QueuedConnection);
    }, Qt::QueuedConnection);
}
void PersistentController::receive(Response response, std::uint64_t request_epoch, Completion completion) {
    if (request_epoch != epoch_) return; // A previous account can never repaint current views.
    state_ = std::move(response.state);
    if (state_.user && state_.cache_ready) {
        try { workspace_.replace(response.records); }
        catch (const std::exception&) {
            workspace_.invalidate(); state_.cache_ready = false;
            state_.error = "Database operation finished, but the local view could not refresh. Reload before continuing.";
        }
    } else workspace_.invalidate();
    state_.busy = false;
    notify();
    if (completion) completion(state_.error);
}
void PersistentController::initialize(const std::filesystem::path& configuration) {
    ++epoch_; clearAccount(); state_.connected = false;
    post([configuration](Worker& worker) {
        auto& services = *worker.services;
        services.auth.logout(); services.database.disconnect();
        const auto config = loadDatabaseConfig(configuration);
        services.database.connect(config);
        try { services.database.verifySchema(); }
        catch (...) { services.database.disconnect(); throw; }
    }, {}, true);
}
void PersistentController::authenticate(bool registration, std::string username, std::string password,
                                         std::string confirmation, Completion completion) {
    ++epoch_; clearAccount();
    auto credentials = std::make_shared<Credentials>();
    credentials->username = std::move(username); credentials->password = std::move(password);
    credentials->confirmation = std::move(confirmation);
    post([registration, credentials](Worker& worker) {
        auto& services = *worker.services;
        if (!services.database.connected()) throw std::runtime_error("Database is disconnected. Reconnect before signing in.");
        services.auth.logout();
        if (registration) services.auth.registerAccount(credentials->username, credentials->password, credentials->confirmation);
        else if (!services.auth.signIn(credentials->username, credentials->password))
            throw std::runtime_error("Invalid username or password.");
    }, std::move(completion), true);
}
void PersistentController::csv(int action, std::filesystem::path path, std::string date, bool overwrite, Completion completion, ExpenseFilter filter) {
    post([action,path=std::move(path),date=std::move(date),overwrite,filter=std::move(filter)](Worker& worker) {
        FileManager files(worker.services->expenses);
        if(action==0) files.importCsv(path);
        else if(action==1) files.exportCsv(path,ExpenseFilter{},overwrite);
        else if(action==3) files.exportCsv(path,filter,overwrite);
        else files.exportWeeklyReport(path,date,overwrite);
    },std::move(completion));
}
void PersistentController::logout() {
    ++epoch_; clearAccount(); state_.error.clear();
    post([](Worker& worker) { worker.services->auth.logout(); }, {}, true);
}
void PersistentController::save(std::optional<int> id, ExpenseDraft draft, Completion completion) {
    post([id, draft = std::move(draft)](Worker& worker) {
        if (id) {
            if (!worker.services->expenses.update(*id, draft)) throw std::runtime_error("Expense was not found in this account.");
        } else worker.services->expenses.add(draft);
    }, std::move(completion));
}
void PersistentController::erase(int id, Completion completion) {
    post([id](Worker& worker) {
        if (!worker.services->expenses.erase(id)) throw std::runtime_error("Expense was not found in this account.");
    }, std::move(completion));
}
void PersistentController::reload(Completion completion) {
    post([](Worker& worker) { worker.services->expenses.reload(); }, std::move(completion));
}
void PersistentController::setBudget(BudgetPeriod period, Money amount, Completion completion) {
    post([period, amount](Worker& worker) { worker.services->expenses.setBudget(period, amount); }, std::move(completion));
}

} // namespace expense_tracker::gui
