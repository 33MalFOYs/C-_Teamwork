#include "expense_tracker/expense_tracker.hpp"
#include "expense_tracker/auth_service.hpp"
#include "expense_tracker/config.hpp"
#include "expense_tracker/expense_service.hpp"
#include "expense_tracker/memory_expense_service.hpp"
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[]) {
    try {
        bool memory = false;
        bool initialize = false;
        std::filesystem::path config_file = "config.local";
        for (int index = 1; index < argc; ++index) {
            const std::string arg = argv[index];
            if (arg == "--help") {
                std::cout << "Usage: expense_tracker_cli [--config FILE] [--init-db]\n"
                          << "       expense_tracker_cli --memory-demo\n"
                          << "Normal startup requires MySQL and account login.\n"
                          << "--init-db creates/validates tables without resetting existing data.\n"
                          << "--memory-demo is the isolated, nonpersistent Phase 2 teaching workspace.\n";
                return 0;
            }
            if (arg == "--config" && index + 1 < argc) config_file = argv[++index];
            else if (arg == "--init-db") initialize = true;
            else if (arg == "--memory-demo") memory = true;
            else throw std::invalid_argument("Unknown or incomplete option. Use --help.");
        }
        expense_tracker::ExpenseTracker tracker;
        if (memory) {
            if (initialize || config_file != "config.local")
                throw std::invalid_argument("--memory-demo cannot be combined with database options.");
            expense_tracker::MemoryExpenseService temporary;
            return tracker.runMemory(std::cin, std::cout, temporary);
        }
        const auto config = expense_tracker::loadDatabaseConfig(config_file);
        if (initialize) {
            expense_tracker::DatabaseManager::createSchema(config);
            std::cout << "Schema created/verified without resetting data: " << config.database << '\n';
            return 0;
        }
        expense_tracker::DatabaseManager database;
        database.connect(config);
        database.verifySchema();
        expense_tracker::ExpenseList records;
        expense_tracker::Session session;
        expense_tracker::AuthService auth(database, session, records);
        expense_tracker::ExpenseService expenses(session, database, records);
        return tracker.run(std::cin, std::cout, auth, expenses, session);
    } catch (const std::exception& error) {
        std::cerr << "Startup error: " << error.what() << "\nNo unsaved fallback was started.\n";
        return 1;
    }
}
