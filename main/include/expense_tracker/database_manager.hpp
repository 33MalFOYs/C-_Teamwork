#pragma once

#include "expense_tracker/expense_list.hpp"
#include "expense_tracker/session.hpp"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace expense_tracker {

struct DatabaseConfig {
    std::string host;
    unsigned int port = 0; // Required; never guess a MAMP port.
    std::string database = "expense_tracker";
    std::string username;
    std::string password; // Connection secret only; never print or commit it.
    unsigned int timeout_seconds = 5;
};

// Classic MySQL Connector/C++ persistence; connection belongs to one thread.
// Exceptions report connection/query failures; false only means no scoped row.
// The implementation owns one connection and must stay on its worker thread.
class DatabaseManager {
public:
    DatabaseManager();
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    void connect(const DatabaseConfig& config);
    void disconnect() noexcept;
    bool connected() const noexcept;
    void verifySchema(); // Do not reset existing data or infer account ownership.
    // Explicit setup only: create an empty schema, or verify an existing complete
    // compatible schema. Never migrate partial/legacy tables or reassign records.
    static void createSchema(const DatabaseConfig& config);

    void loadExpenses(const Session& session, ExpenseList& destination);
    int insertExpense(const Session& session, const ExpenseDraft& expense);
    bool updateExpense(const Session& session, int id, const ExpenseDraft& expense);
    bool deleteExpense(const Session& session, int id);
    std::optional<Money> loadBudget(const Session& session, BudgetPeriod period);
    void saveBudget(const Session& session, BudgetPeriod period, Money amount);
    // Validate all rows first; one transaction, rollback on any failure.
    std::vector<int> importExpenses(const Session& session,
                                   const std::vector<ExpenseDraft>& expenses);

private:
    friend class AuthService;
    struct AccountRecord {
        User user;
        std::string encoded_password_hash;
    };
    std::optional<AccountRecord> findAccount(const std::string& normalized_name);
    User createAccount(const std::string& normalized_name,
                       const std::string& encoded_password_hash);
    struct Implementation;
    std::unique_ptr<Implementation> implementation_;
};

} // namespace expense_tracker
