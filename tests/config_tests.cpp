#include "expense_tracker/config.hpp"
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>

namespace {
const std::array<const char*, 6> names{"EXPENSE_DB_HOST", "EXPENSE_DB_PORT", "EXPENSE_DB_NAME",
    "EXPENSE_DB_USER", "EXPENSE_DB_PASSWORD", "EXPENSE_CURRENCY"};
void setEnvironment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value ? value : "");
#else
    if (value) setenv(name, value, 1); else unsetenv(name);
#endif
}
struct Environment {
    std::map<std::string, std::optional<std::string>> previous;
    Environment() {
        for (auto* name : names) {
            const auto* value = std::getenv(name);
            previous[name] = value ? std::optional<std::string>(value) : std::nullopt;
            setEnvironment(name, nullptr);
        }
    }
    ~Environment() {
        for (const auto& item : previous) setEnvironment(item.first.c_str(), item.second ? item.second->c_str() : nullptr);
    }
};
void require(bool condition, const char* error) { if (!condition) throw std::runtime_error(error); }
}

int main() {
    Environment environment;
    const auto file = std::filesystem::current_path() / "config-tests.local";
    auto write = [&](const std::string& text) { std::ofstream output(file, std::ios::binary); output << text; };
    const std::string base = "EXPENSE_DB_HOST=127.0.0.1\nEXPENSE_DB_PORT=12345\nEXPENSE_DB_USER=fictional\n";
    auto rejects = [&](const std::string& text) {
        write(text);
        try { (void)expense_tracker::loadDatabaseConfig(file); }
        catch (const std::exception& error) {
            require(std::string(error.what()).find("fictional-secret") == std::string::npos, "Configuration error leaked a value");
            return;
        }
        throw std::runtime_error("Invalid configuration was accepted");
    };
    try {
        write("\xef\xbb\xbf# UTF-8 BOM supported\n" + base + "EXPENSE_DB_PASSWORD=  fictional-secret=with spaces  \n");
        auto config = expense_tracker::loadDatabaseConfig(file);
        require(config.port == 12345 && config.database == "expense_tracker", "Connection defaults/port wrong");
        require(config.password == "  fictional-secret=with spaces  ", "Password bytes were modified");
        setEnvironment("EXPENSE_DB_PORT", "23456");
        require(expense_tracker::loadDatabaseConfig(file).port == 23456, "Environment did not override file");
        setEnvironment("EXPENSE_DB_PORT", nullptr);
        write(base + "EXPENSE_DB_PASSWORD=\n");
        require(expense_tracker::loadDatabaseConfig(file).password.empty(), "Explicit empty password rejected");
        rejects(base); // Missing differs from explicitly empty.
        rejects(base + "EXPENSE_DB_PASSWORD=fictional-secret\nEXPENSE_DB_NAME=bad;name\n");
        rejects(base + "EXPENSE_DB_PASSWORD=fictional-secret\nEXPENSE_CURRENCY=KHR\n");
        rejects(base + "EXPENSE_DB_PASSWORD=fictional-secret\nEXPENSE_DB_USER=duplicate\n");
        rejects(base + "EXPENSE_DB_PASSWORD=fictional-secret\nfictional-secret\n");
        rejects(base + "EXPENSE_DB_PASSWORD=fictional-secret\nUNKNOWN=fictional-secret\n");
        for (const auto* port : {"0", "65536", "-1", "abc", "9999999999999999"}) {
            setEnvironment("EXPENSE_DB_PORT", port);
            rejects(base + "EXPENSE_DB_PASSWORD=fictional-secret\n");
        }
        setEnvironment("EXPENSE_DB_PORT", nullptr);
        std::filesystem::remove(file);
        try { (void)expense_tracker::loadDatabaseConfig(file); throw std::logic_error("Missing settings accepted"); }
        catch (const std::runtime_error&) { }
        std::cout << "Configuration parsing, secret handling, and environment checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove(file);
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
