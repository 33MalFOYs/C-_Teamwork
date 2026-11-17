#include "expense_tracker/auth_service.hpp"
#include "expense_tracker/config.hpp"
#include "expense_tracker/expense_service.hpp"
#include "expense_tracker/expense_tracker.hpp"

#include <sodium.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace expense_tracker;
int checks = 0;
const std::string password = "Fictional CLI passphrase 123!";
const std::string wrongPassword = "Wrong fictional CLI passphrase!";

void require(bool condition, const char* message) {
    ++checks;
    if (!condition) throw std::runtime_error(message);
}

void contains(const std::string& text, const std::string& expected, const char* message) {
    require(text.find(expected) != std::string::npos, message);
}

std::string uniqueName() {
    std::array<unsigned char, 8> bytes{};
    randombytes_buf(bytes.data(), bytes.size());
    static constexpr char digits[] = "0123456789abcdef";
    std::string name = "phase3_cli_";
    for (const auto byte : bytes) { name += digits[byte >> 4]; name += digits[byte & 15]; }
    return name;
}

struct Application {
    DatabaseManager database;
    Session session;
    ExpenseList records;
    AuthService auth{database, session, records};
    ExpenseService expenses{session, database, records};
    ExpenseTracker cli;

    explicit Application(const DatabaseConfig& config) {
        database.connect(config);
        database.verifySchema();
    }

    std::string run(const std::string& script) {
        std::istringstream input(script);
        std::ostringstream output;
        require(cli.run(input, output, auth, expenses, session) == 0,
                "Persistent CLI returned a failure exit code");
        require(!session.authenticated() && !session.cacheReady() && records.size() == 0,
                "CLI exit retained authenticated account data");
        auto text = output.str();
        require(text.find(password) == std::string::npos && text.find(wrongPassword) == std::string::npos,
                "CLI printed a password");
        require(text.find("$argon2") == std::string::npos, "CLI printed an encoded password hash");
        return text;
    }
};

std::vector<int> savedIds(const std::string& text) {
    const std::string marker = "Saved expense ID ";
    std::vector<int> result;
    std::size_t offset = 0;
    while ((offset = text.find(marker, offset)) != std::string::npos) {
        offset += marker.size();
        result.push_back(std::stoi(text.substr(offset)));
    }
    return result;
}

std::string login(const std::string& name) { return "1\n" + name + "\n" + password + "\n"; }

void runChecks(const DatabaseConfig& config) {
    std::string account;
    int retainedId = 0;
    int removedId = 0;
    {
        Application app(config);
        account = uniqueName();
        contains(app.run("0\n"), "Goodbye.", "Auth-menu exit failed");
        contains(app.run(""), "End of input. Goodbye.", "Auth-menu EOF failed");
        contains(app.run("1\n" + account + "\n"), "End of input. Goodbye.", "Password-prompt EOF failed");
        contains(app.run("2\n/cancel\n0\n"), "Action canceled.", "Account creation cancel failed");

        const auto output = app.run(
            "unlisted\n2\n " + account + " \n" + password + "\n" + password + "\n"
            "1\n" + account + "\n" + wrongPassword + "\n" + login(account) +
            "1\n12.50\nFood\n2026-12-28\nFictional lunch with O'Reilly\n"
            "1\n5.00\nTravel\n2026-12-29\nFictional bus\n"
            "2\n4\nFood\n9\n2026-12-30\n11\n12\n13\n10\n2026\n12\n"
            "15\n2026\n12\n14\n2026\n12\n100.00\n16\n17\n0\n");
        contains(output, "Invalid choice. Enter 0, 1, or 2.", "Invalid auth-menu choice was not handled");
        contains(output, "Account created. Sign in with your password.", "CLI account registration failed");
        contains(output, "Invalid username or password.", "CLI wrong-password failure was not generic");
        contains(output, "Signed in as " + account + ".", "CLI correct login failed");
        contains(output, "Count: 2 | Total: USD 17.50", "CLI persisted expense view has wrong totals");
        contains(output, "Count: 1 | Total: USD 12.50", "CLI category search failed");
        contains(output, "Week: 2026-12-28 through 2027-01-03", "CLI weekly report failed");
        contains(output, "Week: 2026-12-21 through 2026-12-27", "CLI previous-week navigation failed");
        contains(output, "Month: 2026-12 | Total: USD 17.50 | Count: 2", "CLI monthly report failed");
        contains(output, "No budget is set for this month.", "CLI missing budget was not distinguished");
        contains(output, "Monthly budget saved to MySQL.", "CLI budget write failed");
        contains(output, "Budget: USD 100.00 | Spent: USD 17.50 | Remaining: USD 82.50", "CLI budget remaining is wrong");
        contains(output, "Expenses reloaded from MySQL.", "CLI explicit reload failed");
        contains(output, "Logged out. All account data cleared from this session.", "CLI logout did not return to auth");
        const auto ids = savedIds(output);
        require(ids.size() == 2 && ids[0] > 0 && ids[1] > ids[0], "CLI did not display database-generated IDs");
        retainedId = ids[0];
        removedId = ids[1];
    }

    // Reconstruct the connection, SLL, session and services to verify durable data.
    {
        Application restarted(config);
        const auto kept = std::to_string(retainedId);
        const auto deleted = std::to_string(removedId);
        const auto output = restarted.run(login(account) +
            "2\n3\n" + kept + "\n3\n2147483647\n5\n2147483647\n"
            "5\n" + kept + "\n15.00\nFood\n2026-12-28\nFictional revised lunch\n"
            "6\n" + deleted + "\nno\n2\n"
            "7\n1\n2026-12-28\nFood\n8\n"
            "6\n" + deleted + "\nyes\n6\n2147483647\n"
            "1\n/cancel\n15\n2026\n12\n17\n" + login(account) + "2\n0\n");
        contains(output, "Count: 2 | Total: USD 17.50", "Fresh CLI process did not load saved expenses");
        contains(output, "Fictional lunch with O'Reilly", "CLI did not preserve quoted description");
        contains(output, "No expense with ID 2147483647.", "CLI missing-ID handling failed");
        contains(output, "Updated saved expense " + kept + ".", "CLI update failed");
        contains(output, "Deletion canceled.", "CLI delete cancellation failed");
        contains(output, "Count: 2 | Total: USD 20.00", "Canceled deletion changed saved records");
        contains(output, "Active filter: 2026-12-28 through 2026-12-28, category = Food", "CLI date/category filter failed");
        contains(output, "Filters cleared.", "CLI clear-filter command failed");
        contains(output, "Deleted saved expense " + deleted + ".", "CLI confirmed deletion failed");
        contains(output, "Action canceled. No records changed.", "CLI expense cancellation failed");
        contains(output, "Budget: USD 100.00 | Spent: USD 15.00 | Remaining: USD 85.00", "CLI budget did not persist across restart");
        contains(output, "Count: 1 | Total: USD 15.00", "CLI account switch did not reload final saved state");
        require(output.find("Error:") == std::string::npos, "CLI financial flow emitted an unexpected error");
    }
    {
        Application restarted(config);
        const auto output = restarted.run(login(account) + "2\n3\n" + std::to_string(removedId) +
                                          "\n15\n2026\n12\n");
        contains(output, "Fictional revised lunch", "CLI update did not survive another restart");
        contains(output, "Count: 1 | Total: USD 15.00", "CLI deletion did not survive another restart");
        contains(output, "No expense with ID " + std::to_string(removedId) + ".", "Deleted ID remained after restart");
        contains(output, "Remaining: USD 85.00", "Saved budget was unavailable after restart");
        contains(output, "End of input. Goodbye. Saved changes remain in MySQL.", "Financial-menu EOF failed");
    }
    std::cout << "Persistent CLI account/CRUD/report/budget checks passed: " << checks << ".\n";
}
} // namespace

int main(int argc, char* argv[]) {
    const auto* environmentConfig = std::getenv("EXPENSE_TEST_CONFIG");
    const std::filesystem::path configPath = argc > 1 && argv[1][0] != '\0'
        ? argv[1] : (environmentConfig ? environmentConfig : "");
    if (configPath.empty()) {
        std::cout << "SKIP: provide an isolated test configuration using argv[1] or EXPENSE_TEST_CONFIG.\n";
        return 77;
    }
    try {
        const auto config = expense_tracker::loadDatabaseConfig(configPath);
        if (config.database.rfind("expense_tracker_test_", 0) != 0)
            throw std::runtime_error("Refusing integration writes: database must start with expense_tracker_test_.");
        runChecks(config);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
