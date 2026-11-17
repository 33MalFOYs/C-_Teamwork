#include "expense_tracker/expense_tracker.hpp"
#include "expense_tracker/expense_workspace.hpp"
#include "expense_tracker/expense_service.hpp"
#include "expense_tracker/validation.hpp"

#include "expense_tracker/file_manager.hpp"
#include <iomanip>
#include <istream>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace expense_tracker {
namespace {

struct InputEnded {};
struct Canceled {};

std::string readLine(std::istream& input, std::ostream& output, const char* prompt) {
    output << prompt << std::flush;
    std::string value;
    if (!std::getline(input, value)) throw InputEnded{};
    if (validation::trimAscii(value) == "/cancel") throw Canceled{};
    return value;
}

int readId(std::istream& input, std::ostream& output) {
    return validation::parsePositiveId(readLine(input, output, "Expense ID: "));
}

ExpenseDraft readDraft(std::istream& input, std::ostream& output) {
    output << "Enter all expense fields; /cancel cancels without changing records.\n";
    const auto amount = readLine(input, output, "Amount (USD, e.g. 12.50): ");
    const auto category = readLine(input, output, "Category: ");
    const auto date = readLine(input, output, "Date (YYYY-MM-DD): ");
    const auto description = readLine(input, output, "Description (may be empty): ");
    return {validation::parseAmount(amount), category, date, description};
}

void printRows(std::ostream& output, const std::vector<Expense>& rows) {
    if (rows.empty()) {
        output << "No matching expenses.\nCount: 0 | Total: USD 0.00\n";
        return;
    }
    output << std::left << std::setw(8) << "ID" << std::setw(14) << "Date"
           << std::setw(18) << "Amount (USD)" << std::setw(24) << "Category" << " | "
           << "Description\n";
    output << std::string(82, '-') << '\n';
    Money total = 0;
    for (const auto& expense : rows) {
        output << std::left << std::setw(8) << expense.id() << std::setw(14) << expense.date()
               << std::setw(18) << validation::formatMoney(expense.amount())
               << std::setw(24) << expense.category() << " | " << expense.description() << '\n';
        total = validation::checkedAdd(total, expense.amount());
    }
    output << "Count: " << rows.size() << " | Total: USD " << validation::formatMoney(total) << '\n';
}

bool printOne(std::ostream& output, ExpenseWorkspace& expenses, int id) {
    const auto existing = expenses.searchExpense(id);
    if (!existing) {
        output << "No expense with ID " << id << ".\n";
        return false;
    }
    printRows(output, {*existing});
    return true;
}

ExpenseFilter readFilter(std::istream& input, std::ostream& output) {
    output << "Date filter: 0 All dates, 1 Exact date, 2 Custom range, 3 This week, 4 This month\n";
    const auto mode = validation::trimAscii(readLine(input, output, "Filter mode: "));
    std::optional<DateRange> dates;
    if (mode == "1") {
        const auto date = readLine(input, output, "Exact date (YYYY-MM-DD): ");
        dates = DateRange{date, date};
    } else if (mode == "2") {
        const auto start = readLine(input, output, "Start date (YYYY-MM-DD): ");
        const auto end = readLine(input, output, "End date (YYYY-MM-DD): ");
        dates = DateRange{start, end};
    } else if (mode == "3") {
        dates = validation::weekContaining(validation::localToday());
    } else if (mode == "4") {
        dates = validation::monthContaining(validation::localToday());
    } else if (mode != "0") {
        throw std::invalid_argument("Filter mode must be 0 through 4.");
    }
    const auto category = validation::trimAscii(readLine(input, output, "Category (empty for all): "));
    return ExpenseFilter(dates, category.empty() ? std::nullopt : std::optional<std::string>(category));
}

void printFilter(std::ostream& output, const ExpenseFilter& filter) {
    output << "Active filter: ";
    if (filter.dates()) output << filter.dates()->start << " through " << filter.dates()->end;
    else output << "all dates";
    if (filter.category()) output << ", category = " << *filter.category();
    else output << ", all categories";
    output << '\n';
}

void printWeek(std::ostream& output, const WeeklyReport& report) {
    output << "Week: " << report.dates.start << " through " << report.dates.end << " (Monday-Sunday)\n"
           << "Total: USD " << validation::formatMoney(report.total) << " | Count: " << report.count
           << " | Average per day: USD " << validation::formatDailyAverage(report.total) << '\n';
    for (const auto& day : report.days) {
        output << day.date << "  USD " << validation::formatMoney(day.total) << '\n';
    }
    if (report.count == 0) output << "No expenses in this week.\n";
    for (const auto& category : report.categories) {
        output << category.first << ": USD " << validation::formatMoney(category.second) << '\n';
    }
}

} // namespace

int ExpenseTracker::runMemory(std::istream& input, std::ostream& output, ExpenseWorkspace& expenses) {
    output << "Personal Expense Tracker - Phase 2 temporary workspace\n"
           << "NONPERSISTENT: all records are lost when this process closes. No account is signed in.\n"
           << "Temporary IDs are local to this workspace. Use fictional practice data.\n";
    return runWorkspace(input, output, expenses, nullptr);
}

int ExpenseTracker::runPersistent(std::istream& input, std::ostream& output, ExpenseService& expenses) {
    output << "Your expenses are saved in MySQL. Filters and reports use your scoped in-memory list.\n";
    return runWorkspace(input, output, expenses, &expenses);
}

int ExpenseTracker::runWorkspace(std::istream& input, std::ostream& output,
                                 ExpenseWorkspace& expenses, ExpenseService* persistent) {
    ExpenseFilter activeFilter;
    std::optional<std::string> selectedWeek;
    while (true) {
        output << "\n1. Add expense\n2. View expenses (active filter)\n3. Search by ID\n"
               << "4. Search by category\n5. Update expense\n6. Delete expense\n"
               << "7. Set date/category filters\n8. Clear filters\n9. Weekly report\n"
               << "10. Monthly/category report\n11. Previous week\n12. Next week\n13. This week\n";
        if (persistent) output << "14. Set monthly budget\n15. Monthly budget remaining\n16. Reload expenses\n17. Logout\n18. Export filtered CSV\n19. Import CSV (creates new records)\n20. Export weekly CSV\n";
        output << "0. Exit\n";
        try {
            const auto choice = validation::trimAscii(readLine(input, output, "Choice: "));
            if (choice == "0") {
                output << (persistent ? "Goodbye. Saved changes remain in MySQL.\n" : "Goodbye. Temporary records have not been saved.\n");
                return 0;
            }
            if (persistent && choice == "17") return 2;
            if (persistent && (choice == "18" || choice == "19" || choice == "20")) {
                const auto path = std::filesystem::u8path(readLine(input, output, "CSV file path: "));
                FileManager files(*persistent);
                if (choice == "19") {
                    if (readLine(input, output, "Import creates new IDs; repeating duplicates records. Type yes: ") != "yes") continue;
                    files.importCsv(path);
                } else {
                    bool overwrite = false;
                    if (std::filesystem::exists(path)) {
                        overwrite = readLine(input, output, "Overwrite existing file? Type yes: ") == "yes";
                        if (!overwrite) continue;
                    }
                    if (choice == "18") files.exportCsv(path, activeFilter, overwrite);
                    else files.exportWeeklyReport(path, readLine(input, output, "Date in week (YYYY-MM-DD): "), overwrite);
                }
                output << "CSV operation completed.\n";
            } else if (choice == "1") {
                const auto id = expenses.add(readDraft(input, output));
                if (persistent) output << "Saved expense ID " << id << " to MySQL.\n";
                else output << "Added temporary expense ID " << id << ". Not saved to disk.\n";
            } else if (choice == "2") {
                printFilter(output, activeFilter);
                printRows(output, expenses.matching(activeFilter));
            } else if (choice == "3") {
                printOne(output, expenses, readId(input, output));
            } else if (choice == "4") {
                const auto category = readLine(input, output, "Category to search: ");
                printRows(output, expenses.searchExpense(category));
            } else if (choice == "5") {
                const int id = readId(input, output);
                if (!printOne(output, expenses, id)) continue;
                const auto replacement = readDraft(input, output);
                if (expenses.update(id, replacement)) output << (persistent ? "Updated saved expense " : "Updated temporary expense ") << id << ".\n";
                else output << "No expense with ID " << id << ".\n";
            } else if (choice == "6") {
                const int id = readId(input, output);
                if (!printOne(output, expenses, id)) continue;
                auto confirm = validation::trimAscii(readLine(input, output, "Delete this expense? Type yes to confirm: "));
                for (auto& c : confirm) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
                if (confirm != "yes") output << "Deletion canceled.\n";
                else if (expenses.erase(id)) output << (persistent ? "Deleted saved expense " : "Deleted temporary expense ") << id << ".\n";
                else output << "No expense with ID " << id << ".\n";
            } else if (choice == "7") {
                auto candidate = readFilter(input, output); // Validate before changing active view.
                auto rows = expenses.matching(candidate);
                activeFilter = std::move(candidate);
                printFilter(output, activeFilter);
                printRows(output, rows);
            } else if (choice == "8") {
                activeFilter = ExpenseFilter{};
                output << "Filters cleared.\n";
                printRows(output, expenses.matching(activeFilter));
            } else if (choice == "9" || choice == "11" || choice == "12" || choice == "13") {
                std::string date;
                if (choice == "9") {
                    date = readLine(input, output, "Date in selected week (YYYY-MM-DD, empty for today): ");
                    if (date.empty()) date = validation::localToday();
                } else if (choice == "13") {
                    date = validation::localToday();
                } else {
                    date = selectedWeek.value_or(validation::localToday());
                    date = validation::addDays(date, choice == "11" ? -7 : 7);
                }
                const auto report = expenses.weekly(date);
                selectedWeek = date;
                printWeek(output, report);
            } else if (choice == "10") {
                const int year = validation::parsePositiveId(readLine(input, output, "Year (1000-9999): "));
                const int month = validation::parsePositiveId(readLine(input, output, "Month (1-12): "));
                const auto report = expenses.monthly({year, month});
                output << "Month: " << year << '-' << std::setfill('0') << std::right << std::setw(2) << month
                       << std::setfill(' ') << " | Total: USD " << validation::formatMoney(report.total)
                       << " | Count: " << report.count << '\n';
                for (const auto& category : report.categories)
                    output << category.first << ": USD " << validation::formatMoney(category.second) << '\n';
                if (report.count == 0) output << "No expenses in this month.\n";
            } else if (persistent && (choice == "14" || choice == "15")) {
                const int year = validation::parsePositiveId(readLine(input, output, "Year (1000-9999): "));
                const int month = validation::parsePositiveId(readLine(input, output, "Month (1-12): "));
                const BudgetPeriod period{year, month};
                validation::validatePeriod(period);
                if (choice == "14") {
                    const auto amount = validation::parseAmount(readLine(input, output, "Monthly budget (USD, zero allowed): "), true);
                    persistent->setBudget(period, amount);
                    output << "Monthly budget saved to MySQL.\n";
                }
                const auto budget = persistent->budget(period);
                if (!budget) output << "No budget is set for this month.\n";
                else {
                    const Money spent = persistent->monthly(period).total;
                    output << "Budget: USD " << validation::formatMoney(*budget)
                           << " | Spent: USD " << validation::formatMoney(spent)
                           << " | Remaining: USD " << validation::formatMoney(*budget - spent)
                           << (spent > *budget ? " (overspent)\n" : "\n");
                }
            } else if (persistent && choice == "16") {
                persistent->reload();
                output << "Expenses reloaded from MySQL.\n";
                printRows(output, expenses.matching(activeFilter));
            } else {
                output << "Invalid choice. Enter a listed menu number.\n";
            }
        } catch (const InputEnded&) {
            if (input.eof()) {
                output << (persistent ? "\nEnd of input. Goodbye. Saved changes remain in MySQL.\n" : "\nEnd of input. Goodbye. Temporary records have not been saved.\n");
                return 0;
            }
            output << "\nCould not read input. Exiting.\n";
            return 1;
        } catch (const Canceled&) {
            output << "Action canceled. No records changed.\n";
        } catch (const std::exception& error) {
            output << "Error: " << error.what() << '\n';
        }
    }
}

} // namespace expense_tracker
