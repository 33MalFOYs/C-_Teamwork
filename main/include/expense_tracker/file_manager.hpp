#pragma once

#include "expense_tracker/expense_service.hpp"
#include "expense_tracker/report_service.hpp"

#include <filesystem>

namespace expense_tracker {

// CSV implementation: std::ifstream/ofstream, CSV escaping, whole-file validation.
// All reads/exports are scoped to the session; imported IDs are discarded.
class FileManager {
public:
    explicit FileManager(ExpenseService& expenses);
    FileManager(const Session& session, ExpenseService& expenses, ReportService& reports);
    void importCsv(const std::filesystem::path& path);
    void exportCsv(const std::filesystem::path& path, const ExpenseFilter& filter,
                   bool overwrite_confirmed = false) const;
    // Recompute the requested week from this session's records at export time.
    void exportWeeklyReport(const std::filesystem::path& path, const std::string& selected_date,
                            bool overwrite_confirmed = false) const;

private:

    // Import delegates to the same database-first coordinator as normal CRUD.
    // Exports use gated service reads; no direct access to a potentially stale SLL.
    ExpenseService& expenses_;

};

} // namespace expense_tracker
