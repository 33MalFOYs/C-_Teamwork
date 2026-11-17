#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace expense_tracker {

struct User {
    int id = 0;
    std::string username;
};

class AuthService;
class ExpenseService;

class Session {
public:
    Session() = default; // Always starts signed out.
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    bool authenticated() const noexcept { return user_.has_value(); }
    const User* currentUser() const noexcept {
        return user_ ? &*user_ : nullptr;
    }
    int requireUserId() const; // Phase 3: throw if signed out.
    // A signed-in session may temporarily have an unavailable/stale cache.
    // Reports, financial views, totals, and exports must check this shared gate.
    bool cacheReady() const noexcept { return authenticated() && cache_ready_; }
    void requireReadableCache() const; // Phase 3: throw unless cacheReady().
    std::uint64_t generation() const noexcept { return generation_; }

private:
    friend class AuthService; // Only verified authentication may establish user.
    friend class ExpenseService; // Sole coordinator of financial cache changes.
    std::optional<User> user_;
    bool cache_ready_ = false;
    std::uint64_t generation_ = 0;
    // Future friends increment generation on account/lifecycle changes and
    // cache invalidation/reload, so pending responses can reject stale results.
};

} // namespace expense_tracker
