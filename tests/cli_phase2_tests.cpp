#include "expense_tracker/expense_tracker.hpp"
#include "expense_tracker/memory_expense_service.hpp"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

using namespace expense_tracker;

namespace {
void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::string run(MemoryExpenseService& workspace, const std::string& commands) {
    std::istringstream input(commands);
    std::ostringstream output;
    ExpenseTracker tracker;
    require(tracker.runMemory(input, output, workspace) == 0, "CLI returned unexpected failure");
    return output.str();
}

void seed(MemoryExpenseService& workspace) {
    workspace.add({1000, "Food", "2026-12-28", "Fictional Monday"});
    workspace.add({2000, "Travel", "2026-12-30", "Fictional Wednesday"});
    workspace.add({500, "Food", "2027-01-03", "Fictional Sunday"});
    workspace.add({9900, "Other", "2027-01-04", "Excluded next week"});
}

void crudAndInput() {
    MemoryExpenseService workspace;
    auto text = run(workspace, "nonsense\n2\n1\n10.50\n Food \n2024-02-29\nFictional lunch\n3\n1\n4\n Food \n0\n");
    require(text.find("Invalid choice") != std::string::npos, "Invalid menu input unhandled");
    require(text.find("No matching expenses") != std::string::npos, "Empty list not explained");
    require(text.find("NONPERSISTENT") != std::string::npos, "Memory lifetime not disclosed");
    require(text.find("Count: 1 | Total: USD 10.50") != std::string::npos, "Overloaded searches did not display result");
    require(workspace.searchExpense(1)->category() == "Food", "Category normalization missing");

    run(workspace, "5\n1\n12.75\nTravel\n2024-03-01\nEdited\n0\n");
    require(workspace.searchExpense(1)->amount() == 1275 && workspace.searchExpense(1)->description() == "Edited", "Update not applied");
    text = run(workspace, "5\n1\n99.00\nBad\n2023-02-29\nShould not apply\n0\n");
    require(text.find("Error:") != std::string::npos, "Invalid update not explained");
    require(workspace.searchExpense(1)->amount() == 1275 && workspace.searchExpense(1)->category() == "Travel", "Invalid update mutated original");
    text = run(workspace, "3\n999\n5\n999\n6\n999\n3\nabc\n0\n");
    require(text.find("No expense with ID 999") != std::string::npos && text.find("Error:") != std::string::npos, "Missing/invalid ID feedback absent");
    run(workspace, "6\n1\nno\n0\n");
    require(workspace.searchExpense(1).has_value(), "Canceled deletion changed data");
    run(workspace, "6\n1\nyes\n0\n");
    require(!workspace.searchExpense(1), "Confirmed deletion did not erase");

    for (const auto& amount : {"abc", "0", "-1", "1.234", "10000000000.00"}) {
        text = run(workspace, std::string("1\n") + amount + "\nFood\n2024-01-01\nInvalid amount\n0\n");
        require(text.find("Error:") != std::string::npos && workspace.matching(ExpenseFilter{}).empty(), "Bad amount accepted");
    }
    run(workspace, "1\n/cancel\n0\n");
    require(workspace.matching(ExpenseFilter{}).empty(), "Canceled add changed records");
}

void eofIsAtomic() {
    for (const auto& unfinished : {"", "1\n", "1\n3.50\n", "1\n3.50\nFood\n", "1\n3.50\nFood\n2024-01-01\n"}) {
        MemoryExpenseService workspace;
        const auto text = run(workspace, unfinished);
        require(text.find("End of input") != std::string::npos, "EOF did not exit cleanly");
        require(workspace.matching(ExpenseFilter{}).empty(), "EOF partially inserted an expense");
    }
    MemoryExpenseService workspace;
    const auto id = workspace.add({500, "Food", "2024-01-01", "Existing"});
    run(workspace, "5\n1\n2.00\nChanged\n2024-03-01\n");
    require(workspace.searchExpense(id)->amount() == 500, "EOF partially updated an expense");
    run(workspace, "6\n1\n");
    require(workspace.searchExpense(id).has_value(), "EOF confirmed a deletion");
    std::istringstream failed;
    failed.setstate(std::ios::badbit);
    std::ostringstream output;
    ExpenseTracker tracker;
    require(tracker.runMemory(failed, output, workspace) == 1, "Broken input stream should fail");
}

void filtersAndReports() {
    MemoryExpenseService workspace;
    seed(workspace);
    const auto text = run(workspace,
        "7\n2\n2026-12-28\n2027-01-03\nFood\n"
        "7\n2\n2027-01-03\n2026-12-28\nFood\n2\n"
        "9\n2027-01-01\n12\n11\n10\n2026\n12\n8\n0\n");
    const auto error = text.find("Error:");
    require(error != std::string::npos, "Reversed range was accepted");
    require(text.find("Active filter: 2026-12-28 through 2027-01-03, category = Food", error) != std::string::npos, "Invalid range replaced active filter");
    require(text.find("Count: 2 | Total: USD 15.00", error) != std::string::npos, "Category/date filter results wrong");
    require(text.find("Total: USD 35.00 | Count: 3 | Average per day: USD 5.00") != std::string::npos, "Weekly report wrong");
    require(text.find("2026-12-29  USD 0.00") != std::string::npos, "Zero day missing");
    require(text.find("Week: 2027-01-04 through 2027-01-10") != std::string::npos, "Next-week navigation wrong");
    require(text.find("Month: 2026-12 | Total: USD 30.00 | Count: 2") != std::string::npos, "Monthly report wrong");
    require(text.find("Count: 4 | Total: USD 134.00") != std::string::npos, "Clear filters failed");
}
} // namespace

int main() {
    try {
        crudAndInput();
        eofIsAtomic();
        filtersAndReports();
        std::cout << "Phase 2 CLI CRUD, input, filtering, and reporting checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
