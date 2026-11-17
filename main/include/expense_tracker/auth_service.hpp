#pragma once

#include "expense_tracker/database_manager.hpp"

#include <chrono>

namespace expense_tracker {

// Registration/sign-in are the only unauthenticated service entry points.
// This object, its session, and its database must stay on their owning thread.
class AuthService {
public:
    AuthService(DatabaseManager& database, Session& session, ExpenseList& records);
    void registerAccount(const std::string& username, const std::string& password,
                         const std::string& confirmation);
    // Wrong credentials -> false + generic message; database errors -> exception.
    // Only establish the session after verification and a successful scoped load.
    bool signIn(const std::string& username, const std::string& password);
    void logout() noexcept; // Clear session and SLL; presentation clears its views.
    static std::string normalizeUsername(const std::string& username);
    static void validatePassword(const std::string& password);
    std::chrono::milliseconds retryAfter() const noexcept;

private:
    DatabaseManager& database_;
    Session& session_;
    ExpenseList& records_;
    std::string dummy_hash_; // Salted random password hash, never a usable account.
    unsigned int failed_attempts_ = 0;
    std::chrono::steady_clock::time_point retry_at_{};
    void recordFailure() noexcept;
};

} // namespace expense_tracker
