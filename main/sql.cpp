#include "expense_tracker/database_manager.hpp"
#include "expense_tracker/validation.hpp"

#include <mysql_driver.h>
#include <cppconn/connection.h>
#include <cppconn/exception.h>
#include <cppconn/prepared_statement.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include <algorithm>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <thread>

namespace expense_tracker {
namespace {
using Statement = std::unique_ptr<sql::Statement>;
using Prepared = std::unique_ptr<sql::PreparedStatement>;
using Results = std::unique_ptr<sql::ResultSet>;

struct DriverThreadLease {
    sql::mysql::MySQL_Driver* driver;
    explicit DriverThreadLease(sql::mysql::MySQL_Driver* value) : driver(value) { driver->threadInit(); }
    ~DriverThreadLease() { driver->threadEnd(); }
};

struct ConnectionDeleter {
    std::shared_ptr<DriverThreadLease> thread;
    void operator()(sql::Connection* connection) noexcept {
        delete connection;
        thread.reset();
    }
};
using Connection = std::unique_ptr<sql::Connection, ConnectionDeleter>;

void validateConfig(const DatabaseConfig& config) {
    if (config.host.empty() || config.username.empty() || config.port == 0 || config.port > 65535)
        throw std::invalid_argument("Database host, port, and username must be configured explicitly.");
    if (config.timeout_seconds == 0 || config.timeout_seconds > 60)
        throw std::invalid_argument("Database timeout must be 1 through 60 seconds.");
    if (config.database.empty() || config.database.size() > 64 ||
        !std::all_of(config.database.begin(), config.database.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                   (c >= '0' && c <= '9') || c == '_';
        }))
        throw std::invalid_argument("Database name must contain 1 through 64 letters, digits, or underscores.");
}

std::shared_ptr<DriverThreadLease> driverForThread() {
    static sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
    // A weak TLS slot shares one initialization among concurrent connections
    // in this thread. The final connection releases threadEnd immediately.
    // Owning TLS destructors run too late on MinGW: the process-static driver
    // can already be gone during main-thread TLS teardown.
    thread_local std::weak_ptr<DriverThreadLease> context;
    auto thread = context.lock();
    if (!thread) { thread = std::make_shared<DriverThreadLease>(driver); context = thread; }
    return thread;
}

Connection openConnection(const DatabaseConfig& config, bool select_schema) {
    validateConfig(config);
    sql::ConnectOptionsMap options;
    options["hostName"] = sql::SQLString(config.host);
    options["port"] = static_cast<int>(config.port);
    options["userName"] = sql::SQLString(config.username);
    options["password"] = sql::SQLString(config.password);
    options["OPT_CHARSET_NAME"] = sql::SQLString("utf8mb4");
    options["OPT_CONNECT_TIMEOUT"] = static_cast<int>(config.timeout_seconds);
    options["OPT_READ_TIMEOUT"] = static_cast<int>(config.timeout_seconds);
    options["OPT_WRITE_TIMEOUT"] = static_cast<int>(config.timeout_seconds);
    // Connector 8.1 and libmysql both default to no automatic reconnect. Do
    // not call deprecated OPT_RECONNECT: libmysql 8.0.34+ warns even for false.
    options["CLIENT_FOUND_ROWS"] = true; // An unchanged owned UPDATE still finds its row.
    options["OPT_LOCAL_INFILE"] = 0;
    auto thread = driverForThread();
    Connection connection(thread->driver->connect(options), ConnectionDeleter{thread});
    if (select_schema) connection->setSchema(config.database);
    Statement statement(connection->createStatement());
    statement->execute("SET SESSION sql_mode = 'STRICT_ALL_TABLES,NO_ZERO_DATE,NO_ZERO_IN_DATE,ERROR_FOR_DIVISION_BY_ZERO,NO_ENGINE_SUBSTITUTION'");
    return connection;
}

[[noreturn]] void databaseError(const char* action) {
    // Server diagnostics can echo SQL parameters, hashes, hostnames, or account
    // names. Deliberately expose only an operation-level error to either UI.
    throw std::runtime_error(std::string(action) + " failed. Check the database connection and permissions; no success was confirmed.");
}

void positiveId(int id) {
    if (id <= 0) throw std::invalid_argument("Expense ID must be positive.");
}

int lastInsertedId(sql::Connection& connection) {
    Statement query(connection.createStatement());
    Results rows(query->executeQuery("SELECT LAST_INSERT_ID()"));
    if (!rows->next()) throw std::runtime_error("The database did not return an inserted ID.");
    const auto id = rows->getUInt64(1);
    if (id == 0 || id > static_cast<std::uint64_t>(std::numeric_limits<int>::max()))
        throw std::runtime_error("The generated database ID is outside the supported range.");
    return static_cast<int>(id);
}

void bindDraft(sql::PreparedStatement& query, int first, const ExpenseDraft& draft) {
    query.setString(first, validation::formatMoney(draft.amount));
    query.setString(first + 1, validation::normalizeCategory(draft.category));
    query.setString(first + 2, draft.date);
    query.setString(first + 3, draft.description);
}

int insertRow(sql::Connection& connection, int user, const ExpenseDraft& draft) {
    Prepared query(connection.prepareStatement(
        "INSERT INTO expenses(user_id,amount,category,expense_date,description) VALUES(?,?,?,?,?)"));
    query->setInt(1, user);
    bindDraft(*query, 2, draft);
    query->executeUpdate();
    return lastInsertedId(connection);
}

struct Column {
    const char* table;
    const char* name;
    const char* type;
    int length;
    bool identity;
};
constexpr Column columns[] = {
    {"users", "user_id", "int", 0, true},
    {"users", "username", "varchar", 64, false},
    {"users", "password_hash", "varchar", 255, false},
    {"expenses", "expense_id", "int", 0, true},
    {"expenses", "user_id", "int", 0, false},
    {"expenses", "amount", "decimal", 0, false},
    {"expenses", "category", "varchar", 50, false},
    {"expenses", "expense_date", "date", 0, false},
    {"expenses", "description", "varchar", 255, false},
    {"budgets", "budget_id", "int", 0, true},
    {"budgets", "user_id", "int", 0, false},
    {"budgets", "budget_month", "int", 0, false},
    {"budgets", "budget_year", "int", 0, false},
    {"budgets", "budget_amount", "decimal", 0, false}
};

[[noreturn]] void incompatibleSchema() {
    throw std::runtime_error("Database schema is missing or incompatible. Back up and review existing tables/ownership before applying main/sql/schema.sql; no migration was performed.");
}

void verify(sql::Connection& connection, const std::string& database) {
    Prepared tableQuery(connection.prepareStatement(
        "SELECT TABLE_NAME,ENGINE FROM information_schema.TABLES WHERE TABLE_SCHEMA=? AND TABLE_NAME IN ('users','expenses','budgets')"));
    tableQuery->setString(1, database);
    Results tables(tableQuery->executeQuery());
    int tableCount = 0;
    while (tables->next()) {
        if (tables->getString("ENGINE") != "InnoDB") incompatibleSchema();
        ++tableCount;
    }
    if (tableCount != 3) incompatibleSchema();

    Prepared columnQuery(connection.prepareStatement(
        "SELECT TABLE_NAME,COLUMN_NAME,DATA_TYPE,COLUMN_TYPE,IS_NULLABLE,CHARACTER_MAXIMUM_LENGTH,NUMERIC_PRECISION,NUMERIC_SCALE,EXTRA,COLLATION_NAME FROM information_schema.COLUMNS WHERE TABLE_SCHEMA=? AND TABLE_NAME IN ('users','expenses','budgets')"));
    columnQuery->setString(1, database);
    Results actual(columnQuery->executeQuery());
    std::set<std::string> seen;
    while (actual->next()) {
        const auto table = actual->getString("TABLE_NAME").asStdString();
        const auto name = actual->getString("COLUMN_NAME").asStdString();
        const Column* expected = nullptr;
        for (const auto& candidate : columns)
            if (table == candidate.table && name == candidate.name) { expected = &candidate; break; }
        if (!expected || actual->getString("DATA_TYPE") != expected->type ||
            actual->getString("IS_NULLABLE") != "NO") incompatibleSchema();
        const auto type = actual->getString("COLUMN_TYPE").asStdString();
        if (type.find("unsigned") != std::string::npos) incompatibleSchema();
        const auto extra = actual->getString("EXTRA").asStdString();
        if ((extra.find("auto_increment") != std::string::npos) != expected->identity) incompatibleSchema();
        if (expected->length && actual->getInt("CHARACTER_MAXIMUM_LENGTH") != expected->length) incompatibleSchema();
        if (std::string(expected->type) == "decimal" &&
            (actual->getInt("NUMERIC_PRECISION") != 12 || actual->getInt("NUMERIC_SCALE") != 2)) incompatibleSchema();
        const auto collation = actual->getString("COLLATION_NAME").asStdString();
        if (name == "username" && collation != "ascii_general_ci") incompatibleSchema();
        if (name == "password_hash" && collation != "ascii_bin") incompatibleSchema();
        if ((name == "category" || name == "description") && collation.find("utf8mb4_") != 0) incompatibleSchema();
        seen.insert(table + "." + name);
    }
    if (seen.size() != sizeof(columns) / sizeof(columns[0])) incompatibleSchema();

    Prepared indexQuery(connection.prepareStatement(
        "SELECT TABLE_NAME,INDEX_NAME,NON_UNIQUE,COUNT(SUB_PART) AS prefix_parts,GROUP_CONCAT(COLUMN_NAME ORDER BY SEQ_IN_INDEX SEPARATOR ',') AS fields FROM information_schema.STATISTICS WHERE TABLE_SCHEMA=? AND TABLE_NAME IN ('users','expenses','budgets') GROUP BY TABLE_NAME,INDEX_NAME,NON_UNIQUE"));
    indexQuery->setString(1, database);
    Results indexes(indexQuery->executeQuery());
    std::set<std::string> keys;
    while (indexes->next()) {
        const auto table = indexes->getString("TABLE_NAME").asStdString();
        const auto fields = indexes->getString("fields").asStdString();
        if (indexes->getString("INDEX_NAME") == "PRIMARY") keys.insert(table + ":primary:" + fields);
        if (indexes->getInt("NON_UNIQUE") == 0) {
            // Extra unique constraints can redirect a budget upsert to a row
            // belonging to somebody else; prefix keys also change equality.
            const std::string key = table + ":unique:" + fields;
            if (indexes->getInt("prefix_parts") != 0 ||
                (key != "users:unique:user_id" && key != "users:unique:username" &&
                 key != "expenses:unique:expense_id" && key != "budgets:unique:budget_id" &&
                 key != "budgets:unique:user_id,budget_year,budget_month")) incompatibleSchema();
            keys.insert(key);
        }
        keys.insert(table + ":index:" + fields);
    }
    for (const auto* required : {"users:primary:user_id", "users:unique:username", "expenses:primary:expense_id",
        "expenses:index:user_id,expense_date", "budgets:primary:budget_id", "budgets:unique:user_id,budget_year,budget_month"})
        if (!keys.count(required)) incompatibleSchema();

    Prepared foreignQuery(connection.prepareStatement(
        "SELECT k.TABLE_NAME FROM information_schema.KEY_COLUMN_USAGE k JOIN information_schema.REFERENTIAL_CONSTRAINTS r ON r.CONSTRAINT_SCHEMA=k.CONSTRAINT_SCHEMA AND r.CONSTRAINT_NAME=k.CONSTRAINT_NAME AND r.TABLE_NAME=k.TABLE_NAME WHERE k.TABLE_SCHEMA=? AND k.COLUMN_NAME='user_id' AND k.REFERENCED_TABLE_SCHEMA=? AND k.REFERENCED_TABLE_NAME='users' AND k.REFERENCED_COLUMN_NAME='user_id' AND r.DELETE_RULE IN ('RESTRICT','NO ACTION') AND r.UPDATE_RULE IN ('RESTRICT','NO ACTION')"));
    foreignQuery->setString(1, database);
    foreignQuery->setString(2, database);
    Results foreignKeys(foreignQuery->executeQuery());
    std::set<std::string> ownedTables;
    while (foreignKeys->next()) ownedTables.insert(foreignKeys->getString(1).asStdString());
    if (!ownedTables.count("expenses") || !ownedTables.count("budgets")) incompatibleSchema();

    // A declared FK does not repair rows inserted with foreign_key_checks=0.
    // Reject orphaned legacy records rather than guessing their ownership.
    Statement ownership(connection.createStatement());
    for (const auto* table : {"expenses", "budgets"}) {
        const std::string query = std::string("SELECT 1 FROM `") + database + "`.`" + table +
            "` r LEFT JOIN `" + database + "`.users u ON u.user_id=r.user_id WHERE u.user_id IS NULL LIMIT 1";
        Results orphan(ownership->executeQuery(query));
        if (orphan->next()) incompatibleSchema();
    }
}
} // namespace

struct DatabaseManager::Implementation {
    Connection connection;
    std::string database;
    std::thread::id owner;

    sql::Connection& require() {
        if (!connection) throw std::runtime_error("Database is not connected.");
        if (owner != std::this_thread::get_id())
            throw std::logic_error("Database connection must only be used by its owning thread.");
        return *connection;
    }
};

DatabaseManager::DatabaseManager() : implementation_(std::make_unique<Implementation>()) {}
DatabaseManager::~DatabaseManager() = default;

void DatabaseManager::connect(const DatabaseConfig& config) {
    disconnect();
    try {
        auto connection = openConnection(config, true);
        implementation_->database = config.database;
        implementation_->owner = std::this_thread::get_id();
        implementation_->connection = std::move(connection);
    } catch (const sql::SQLException&) { databaseError("Database connection"); }
}

void DatabaseManager::disconnect() noexcept { implementation_->connection.reset(); }
bool DatabaseManager::connected() const noexcept { return implementation_->connection != nullptr; }

void DatabaseManager::verifySchema() {
    try { verify(implementation_->require(), implementation_->database); }
    catch (const sql::SQLException&) { databaseError("Schema verification"); }
}

void DatabaseManager::createSchema(const DatabaseConfig& config) {
    try {
        auto connection = openConnection(config, false);
        Prepared existing(connection->prepareStatement(
            "SELECT COUNT(*) FROM information_schema.TABLES WHERE TABLE_SCHEMA=?"));
        existing->setString(1, config.database);
        Results rows(existing->executeQuery());
        rows->next();
        if (rows->getInt(1) != 0) {
            verify(*connection, config.database);
            return; // Existing complete compatible schema: deliberately no DDL.
        }
        // Identifiers cannot be parameterized; validateConfig strictly whitelists
        // the database identifier before this concatenation.
        Statement schema(connection->createStatement());
        schema->execute("CREATE DATABASE IF NOT EXISTS `" + config.database + "` CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci");
        connection->setSchema(config.database);
        schema->execute("CREATE TABLE IF NOT EXISTS users (user_id INT NOT NULL AUTO_INCREMENT,username VARCHAR(64) CHARACTER SET ascii COLLATE ascii_general_ci NOT NULL,password_hash VARCHAR(255) CHARACTER SET ascii COLLATE ascii_bin NOT NULL,PRIMARY KEY(user_id),CONSTRAINT uq_users_username UNIQUE(username)) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
        schema->execute("CREATE TABLE IF NOT EXISTS expenses (expense_id INT NOT NULL AUTO_INCREMENT,user_id INT NOT NULL,amount DECIMAL(12,2) NOT NULL,category VARCHAR(50) NOT NULL,expense_date DATE NOT NULL,description VARCHAR(255) NOT NULL,PRIMARY KEY(expense_id),INDEX idx_expenses_user_date(user_id,expense_date),CONSTRAINT fk_expenses_user FOREIGN KEY(user_id) REFERENCES users(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
        schema->execute("CREATE TABLE IF NOT EXISTS budgets (budget_id INT NOT NULL AUTO_INCREMENT,user_id INT NOT NULL,budget_month INT NOT NULL,budget_year INT NOT NULL,budget_amount DECIMAL(12,2) NOT NULL,PRIMARY KEY(budget_id),CONSTRAINT uq_budgets_user_period UNIQUE(user_id,budget_year,budget_month),CONSTRAINT fk_budgets_user FOREIGN KEY(user_id) REFERENCES users(user_id) ON DELETE RESTRICT ON UPDATE RESTRICT) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4 COLLATE=utf8mb4_unicode_ci");
        verify(*connection, config.database);
    } catch (const sql::SQLException&) { databaseError("Schema setup"); }
}

void DatabaseManager::loadExpenses(const Session& session, ExpenseList& destination) {
    const int user = session.requireUserId();
    try {
        Prepared query(implementation_->require().prepareStatement(
            "SELECT expense_id,amount,category,expense_date,description FROM expenses WHERE user_id=? ORDER BY expense_id"));
        query->setInt(1, user);
        Results rows(query->executeQuery());
        ExpenseList fresh;
        while (rows->next()) {
            fresh.insert(Expense(rows->getInt("expense_id"),
                validation::parseAmount(rows->getString("amount").asStdString()),
                rows->getString("category").asStdString(), rows->getString("expense_date").asStdString(),
                rows->getString("description").asStdString()));
        }
        destination.swap(fresh);
    } catch (const sql::SQLException&) { databaseError("Expense loading"); }
}

int DatabaseManager::insertExpense(const Session& session, const ExpenseDraft& expense) {
    const int user = session.requireUserId();
    Expense::validate(expense);
    try { return insertRow(implementation_->require(), user, expense); }
    catch (const sql::SQLException&) { databaseError("Expense insertion"); }
}

bool DatabaseManager::updateExpense(const Session& session, int id, const ExpenseDraft& expense) {
    const int user = session.requireUserId();
    positiveId(id);
    Expense::validate(expense);
    try {
        Prepared query(implementation_->require().prepareStatement(
            "UPDATE expenses SET amount=?,category=?,expense_date=?,description=? WHERE expense_id=? AND user_id=?"));
        bindDraft(*query, 1, expense);
        query->setInt(5, id);
        query->setInt(6, user);
        return query->executeUpdate() == 1;
    } catch (const sql::SQLException&) { databaseError("Expense update"); }
}

bool DatabaseManager::deleteExpense(const Session& session, int id) {
    const int user = session.requireUserId();
    positiveId(id);
    try {
        Prepared query(implementation_->require().prepareStatement(
            "DELETE FROM expenses WHERE expense_id=? AND user_id=?"));
        query->setInt(1, id);
        query->setInt(2, user);
        return query->executeUpdate() == 1;
    } catch (const sql::SQLException&) { databaseError("Expense deletion"); }
}

std::optional<Money> DatabaseManager::loadBudget(const Session& session, BudgetPeriod period) {
    const int user = session.requireUserId();
    validation::validatePeriod(period);
    try {
        Prepared query(implementation_->require().prepareStatement(
            "SELECT budget_amount FROM budgets WHERE user_id=? AND budget_year=? AND budget_month=?"));
        query->setInt(1, user); query->setInt(2, period.year); query->setInt(3, period.month);
        Results rows(query->executeQuery());
        if (!rows->next()) return std::nullopt;
        return validation::parseAmount(rows->getString(1).asStdString(), true);
    } catch (const sql::SQLException&) { databaseError("Budget loading"); }
}

void DatabaseManager::saveBudget(const Session& session, BudgetPeriod period, Money amount) {
    const int user = session.requireUserId();
    validation::validatePeriod(period);
    if (amount < 0 || amount > maximum_amount) throw std::invalid_argument("Budget amount is outside the supported range.");
    try {
        Prepared query(implementation_->require().prepareStatement(
            "INSERT INTO budgets(user_id,budget_year,budget_month,budget_amount) VALUES(?,?,?,?) ON DUPLICATE KEY UPDATE budget_amount=VALUES(budget_amount)"));
        query->setInt(1, user); query->setInt(2, period.year); query->setInt(3, period.month);
        query->setString(4, validation::formatMoney(amount));
        query->executeUpdate();
    } catch (const sql::SQLException&) { databaseError("Budget saving"); }
}

std::vector<int> DatabaseManager::importExpenses(const Session& session, const std::vector<ExpenseDraft>& expenses) {
    const int user = session.requireUserId();
    for (const auto& expense : expenses) Expense::validate(expense);
    auto& connection = implementation_->require();
    std::vector<int> ids;
    ids.reserve(expenses.size());
    try {
        connection.setAutoCommit(false);
        try {
            for (const auto& expense : expenses) ids.push_back(insertRow(connection, user, expense));
            connection.commit();
        } catch (...) {
            try { connection.rollback(); } catch (...) {}
            try { connection.setAutoCommit(true); } catch (...) {}
            throw;
        }
        connection.setAutoCommit(true);
        return ids;
    } catch (const sql::SQLException&) { databaseError("Expense import transaction"); }
}

std::optional<DatabaseManager::AccountRecord> DatabaseManager::findAccount(const std::string& normalized_name) {
    try {
        Prepared query(implementation_->require().prepareStatement(
            "SELECT user_id,username,password_hash FROM users WHERE username=?"));
        query->setString(1, normalized_name);
        Results rows(query->executeQuery());
        if (!rows->next()) return std::nullopt;
        return AccountRecord{User{rows->getInt("user_id"), rows->getString("username").asStdString()},
                             rows->getString("password_hash").asStdString()};
    } catch (const sql::SQLException&) { databaseError("Account lookup"); }
}

User DatabaseManager::createAccount(const std::string& normalized_name, const std::string& encoded_password_hash) {
    if (normalized_name.size() < 3 || normalized_name.size() > 64 ||
        !std::all_of(normalized_name.begin(), normalized_name.end(), [](unsigned char c) {
            return (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '_' || c == '.' || c == '-';
        }) || encoded_password_hash.size() > 255 || encoded_password_hash.rfind("$argon2id$", 0) != 0)
        throw std::invalid_argument("Invalid canonical account data.");
    try {
        auto& connection = implementation_->require();
        Prepared query(connection.prepareStatement("INSERT INTO users(username,password_hash) VALUES(?,?)"));
        query->setString(1, normalized_name);
        query->setString(2, encoded_password_hash);
        query->executeUpdate();
        return User{lastInsertedId(connection), normalized_name};
    } catch (const sql::SQLException& error) {
        if (error.getErrorCode() == 1062) throw std::runtime_error("Username is already in use.");
        databaseError("Account creation");
    }
}

} // namespace expense_tracker
