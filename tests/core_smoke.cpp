#include "expense_tracker/auth_service.hpp"
#include "expense_tracker/expense_service.hpp"
#include "expense_tracker/expense_tracker.hpp"
#include "expense_tracker/file_manager.hpp"
#include "expense_tracker/phase_status.hpp"

#include <iostream>
#include <sstream>
#include <type_traits>

namespace {
bool check(bool condition, const char* description) {
    if (!condition) {
        std::cerr << "FAIL: " << description << '\n';
    }
    return condition;
}
}

int main() {
    using namespace expense_tracker;
    static_assert(!std::is_copy_constructible_v<ExpenseList>);
    static_assert(!std::is_copy_assignable_v<ExpenseList>);
    static_assert(!std::is_copy_constructible_v<Session>);
    static_assert(std::is_integral_v<Money>);

    bool passed = true;
    Session session;
    passed &= check(!session.authenticated() && session.currentUser() == nullptr,
                    "a new session is signed out");
    passed &= check(!session.cacheReady() && session.generation() == 0,
                    "a new session cannot expose financial cache data");
    passed &= check(authentication_available(), "Phase 3 authentication capability is built");
    DatabaseManager database;
    ExpenseList records;
    AuthService auth(database, session, records);
    ExpenseService expenses(session, database, records);

    ExpenseTracker tracker;
    std::istringstream explicit_exit(" 0 \n");
    std::ostringstream exit_output;
    passed &= check(tracker.run(explicit_exit, exit_output, auth, expenses, session) == 0 &&
                    exit_output.str().find("Goodbye.") != std::string::npos,
                    "explicit exit terminates cleanly");

    std::istringstream eof_input;
    std::ostringstream eof_output;
    passed &= check(tracker.run(eof_input, eof_output, auth, expenses, session) == 0 &&
                    eof_output.str().find("End of input") != std::string::npos,
                    "EOF terminates without a menu loop");

    std::istringstream sequence("nonsense\n\n01\n0\n");
    std::ostringstream sequence_output;
    passed &= check(tracker.run(sequence, sequence_output, auth, expenses, session) == 0,
                    "invalid input leaves the account menu usable");
    const auto text = sequence_output.str();
    passed &= check(text.find("Invalid choice.") != std::string::npos && !session.authenticated(),
                    "menu handles invalid choices without authenticating");

    std::istringstream failed_input;
    failed_input.setstate(std::ios::badbit);
    std::ostringstream failed_output;
    passed &= check(tracker.run(failed_input, failed_output, auth, expenses, session) == 1,
                    "stream failure returns a nonzero exit instead of looping");
    if (passed) {
        std::cout << "Production entry and CLI smoke checks passed.\n";
    }
    return passed ? 0 : 1;
}
