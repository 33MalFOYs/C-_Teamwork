#include "expense_tracker/config.hpp"
#include "expense_tracker/validation.hpp"

#include <cstdlib>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>

namespace expense_tracker {

DatabaseConfig loadDatabaseConfig(const std::filesystem::path& file) {
    const std::set<std::string> allowed{
        "EXPENSE_DB_HOST", "EXPENSE_DB_PORT", "EXPENSE_DB_NAME", "EXPENSE_DB_USER",
        "EXPENSE_DB_PASSWORD", "EXPENSE_CURRENCY"
    };
    std::map<std::string, std::string> values;
    if (std::filesystem::exists(file)) {
        std::ifstream input(file);
        if (!input) throw std::runtime_error("Cannot read the local database configuration file.");
        std::string line;
        std::size_t number = 0;
        while (std::getline(input, line)) {
            ++number;
            if (number == 1 && line.compare(0, 3, "\xef\xbb\xbf") == 0) line.erase(0, 3);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            const auto trimmed = validation::trimAscii(line);
            if (trimmed.empty() || trimmed.front() == '#') continue;
            const auto equals = line.find('=');
            if (equals == std::string::npos)
                throw std::runtime_error("Configuration line " + std::to_string(number) + " must use KEY=value.");
            const auto name = validation::trimAscii(line.substr(0, equals));
            if (allowed.count(name) == 0)
                throw std::runtime_error("Unknown configuration key on line " + std::to_string(number) + ".");
            if (values.count(name))
                throw std::runtime_error("Duplicate configuration key on line " + std::to_string(number) + ".");
            // Preserve password bytes exactly, including spaces and '='. This
            // format deliberately does not interpret quoting or escape sequences.
            auto value = line.substr(equals + 1);
            if (name != "EXPENSE_DB_PASSWORD") value = validation::trimAscii(value);
            values.emplace(name, std::move(value));
        }
        if (input.bad()) throw std::runtime_error("Could not finish reading the database configuration.");
    }
    for (const auto& name : allowed) {
        if (const auto* value = std::getenv(name.c_str())) values[name] = value;
    }
    auto required = [&](const char* name, bool allow_empty = false) -> std::string {
        const auto found = values.find(name);
        if (found == values.end() || (!allow_empty && found->second.empty()))
            throw std::runtime_error(std::string("Set ") + name + " in config.local or the environment.");
        if (found->second.find('\0') != std::string::npos || found->second.find('\n') != std::string::npos ||
            found->second.find('\r') != std::string::npos)
            throw std::runtime_error(std::string(name) + " contains an unsupported control character.");
        return found->second;
    };
    DatabaseConfig config;
    config.host = required("EXPENSE_DB_HOST");
    const auto port_text = required("EXPENSE_DB_PORT");
    unsigned int port = 0;
    for (char c : port_text) {
        if (c < '0' || c > '9' || port > 6553)
            throw std::runtime_error("EXPENSE_DB_PORT must be a number from 1 through 65535.");
        port = port * 10 + static_cast<unsigned int>(c - '0');
    }
    if (port == 0 || port > 65535)
        throw std::runtime_error("EXPENSE_DB_PORT must be a number from 1 through 65535.");
    config.port = port;
    config.database = values.count("EXPENSE_DB_NAME") ? required("EXPENSE_DB_NAME") : "expense_tracker";
    if (config.database.size() > 64 || config.database.empty())
        throw std::runtime_error("EXPENSE_DB_NAME must have 1 through 64 ASCII letters, digits, or underscores.");
    for (char c : config.database) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_'))
            throw std::runtime_error("EXPENSE_DB_NAME must have 1 through 64 ASCII letters, digits, or underscores.");
    }
    config.username = required("EXPENSE_DB_USER");
    config.password = required("EXPENSE_DB_PASSWORD", true); // Explicit empty is permitted; absent is not.
    if (values.count("EXPENSE_CURRENCY") && values.at("EXPENSE_CURRENCY") != "USD")
        throw std::runtime_error("Only EXPENSE_CURRENCY=USD is supported.");
    return config;
}

} // namespace expense_tracker
