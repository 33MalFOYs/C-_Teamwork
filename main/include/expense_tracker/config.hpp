#pragma once

#include "expense_tracker/database_manager.hpp"
#include <filesystem>

namespace expense_tracker {

// Read key=value UTF-8 configuration; environment values override file values.
// Missing connection settings fail closed. Never include a value in an error.
DatabaseConfig loadDatabaseConfig(const std::filesystem::path& file = "config.local");

} // namespace expense_tracker
