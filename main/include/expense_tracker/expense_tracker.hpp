#pragma once

#include <iosfwd>

namespace expense_tracker {

class ExpenseWorkspace;
class ExpenseService;
class AuthService;
class Session;

// CLI presentation/coordinator. Shared services supply behavior in later phases.
class ExpenseTracker {
public:
    int run(std::istream& input, std::ostream& output, AuthService& auth,
            ExpenseService& expenses, Session& session);
    int runPersistent(std::istream& input, std::ostream& output, ExpenseService& expenses);
    // Explicit Phase 2 scratch workspace; never authenticates a production user.
    int runMemory(std::istream& input, std::ostream& output, ExpenseWorkspace& expenses);
private:
    int runWorkspace(std::istream& input, std::ostream& output, ExpenseWorkspace& expenses,
                     ExpenseService* persistent);
};

} // namespace expense_tracker
