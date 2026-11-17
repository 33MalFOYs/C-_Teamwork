#include "expense_tracker/phase_status.hpp"

namespace expense_tracker {

const char* phase_summary() noexcept {
    return "Phase 4: persistent accounts, reports, budgets and CSV workflows are implemented.";
}

const char* authentication_status() noexcept {
    return "Connect to the configured MySQL database, then sign in or create an account.";
}

bool authentication_available() noexcept {
    return true;
}

} // namespace expense_tracker
