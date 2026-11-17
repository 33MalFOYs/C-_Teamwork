#include "expense_tracker/session.hpp"

#include <stdexcept>

namespace expense_tracker {

int Session::requireUserId() const {
    if (!user_ || user_->id <= 0) {
        throw std::logic_error("Sign in is required for account operations.");
    }
    return user_->id;
}

void Session::requireReadableCache() const {
    requireUserId();
    if (!cache_ready_) {
        throw std::logic_error("Expense data is unavailable. Reload before continuing.");
    }
}

} // namespace expense_tracker
