#include "expense_tracker/auth_service.hpp"
#include "expense_tracker/validation.hpp"

#include <sodium.h>

#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace expense_tracker {
namespace {

constexpr std::size_t maximum_password_bytes = 4096;

std::size_t passwordCodePoints(const std::string& text) {
    std::size_t count = 0;
    for (std::size_t index = 0; index < text.size();) {
        const auto first = static_cast<unsigned char>(text[index]);
        std::uint32_t point = 0;
        std::uint32_t minimum = 0;
        std::size_t length = 0;
        if (first <= 0x7f) { point = first; length = 1; }
        else if (first >= 0xc2 && first <= 0xdf) { point = first & 0x1f; minimum = 0x80; length = 2; }
        else if (first >= 0xe0 && first <= 0xef) { point = first & 0x0f; minimum = 0x800; length = 3; }
        else if (first >= 0xf0 && first <= 0xf4) { point = first & 0x07; minimum = 0x10000; length = 4; }
        else throw std::invalid_argument("Password must be valid UTF-8.");
        if (length > text.size() - index) throw std::invalid_argument("Password must be valid UTF-8.");
        for (std::size_t offset = 1; offset < length; ++offset) {
            const auto next = static_cast<unsigned char>(text[index + offset]);
            if ((next & 0xc0) != 0x80) throw std::invalid_argument("Password must be valid UTF-8.");
            point = (point << 6) | (next & 0x3f);
        }
        if (point < minimum || point > 0x10ffff || (point >= 0xd800 && point <= 0xdfff)) {
            throw std::invalid_argument("Password must be valid UTF-8.");
        }
        ++count;
        index += length;
    }
    return count;
}

std::string encodePassword(const char* password, std::size_t length) {
    std::array<char, crypto_pwhash_STRBYTES> encoded{};
    if (crypto_pwhash_str_alg(encoded.data(), password, static_cast<unsigned long long>(length),
            crypto_pwhash_OPSLIMIT_INTERACTIVE, crypto_pwhash_MEMLIMIT_INTERACTIVE,
            crypto_pwhash_ALG_ARGON2ID13) != 0) {
        throw std::runtime_error("Password hashing could not complete. Please try again.");
    }
    return encoded.data();
}

} // namespace

AuthService::AuthService(DatabaseManager& database, Session& session, ExpenseList& records)
    : database_(database), session_(session), records_(records) {
    if (sodium_init() < 0) throw std::runtime_error("Password library initialization failed.");
    // Unknown usernames still verify a real Argon2id hash. No account, bypass,
    // or plaintext credential is created or retained by this timing safeguard.
    std::array<unsigned char, 32> randomPassword{};
    randombytes_buf(randomPassword.data(), randomPassword.size());
    try {
        dummy_hash_ = encodePassword(reinterpret_cast<const char*>(randomPassword.data()), randomPassword.size());
    } catch (...) {
        sodium_memzero(randomPassword.data(), randomPassword.size());
        throw;
    }
    sodium_memzero(randomPassword.data(), randomPassword.size());
}

std::string AuthService::normalizeUsername(const std::string& username) {
    auto normalized = validation::trimAscii(username);
    if (normalized.size() < 3 || normalized.size() > 64) {
        throw std::invalid_argument("Username must contain 3 through 64 ASCII characters.");
    }
    for (auto& value : normalized) {
        if (value >= 'A' && value <= 'Z') value = static_cast<char>(value - 'A' + 'a');
        if (!((value >= 'a' && value <= 'z') || (value >= '0' && value <= '9') ||
              value == '_' || value == '.' || value == '-')) {
            throw std::invalid_argument("Username may contain letters, digits, underscore, dot, or hyphen.");
        }
    }
    return normalized;
}

void AuthService::validatePassword(const std::string& password) {
    if (password.size() > maximum_password_bytes) {
        throw std::invalid_argument("Password exceeds the explicit 4096-byte limit; it was not truncated.");
    }
    if (passwordCodePoints(password) < 12) {
        throw std::invalid_argument("Password must contain at least 12 Unicode characters.");
    }
}

void AuthService::registerAccount(const std::string& username, const std::string& password,
                                const std::string& confirmation) {
    if (session_.authenticated()) throw std::logic_error("Log out before creating another account.");
    const auto normalized = normalizeUsername(username);
    validatePassword(password);
    if (password != confirmation) throw std::invalid_argument("Password confirmation does not match.");
    const auto encoded = encodePassword(password.data(), password.size());
    (void)database_.createAccount(normalized, encoded);
    // Registration does not authenticate; the user must sign in normally.
}

std::chrono::milliseconds AuthService::retryAfter() const noexcept {
    const auto now = std::chrono::steady_clock::now();
    if (retry_at_ <= now) return std::chrono::milliseconds{0};
    return std::chrono::duration_cast<std::chrono::milliseconds>(retry_at_ - now) +
           std::chrono::milliseconds{1};
}

void AuthService::recordFailure() noexcept {
    if (++failed_attempts_ >= 3) {
        failed_attempts_ = 0;
        retry_at_ = std::chrono::steady_clock::now() + std::chrono::seconds{2};
    }
}

bool AuthService::signIn(const std::string& username, const std::string& password) {
    logout(); // Account switching/failure must never retain the previous user's records.
    if (retryAfter().count() > 0) return false;

    std::optional<DatabaseManager::AccountRecord> account;
    bool validInput = true;
    std::string normalized;
    try {
        normalized = normalizeUsername(username);
        validatePassword(password);
    } catch (const std::invalid_argument&) {
        validInput = false;
    }
    if (validInput) account = database_.findAccount(normalized);
    const auto& encoded = account ? account->encoded_password_hash : dummy_hash_;
    const char* supplied = validInput ? password.data() : "";
    const auto length = validInput ? password.size() : std::size_t{0};
    const bool verified = crypto_pwhash_str_verify(encoded.c_str(), supplied,
                               static_cast<unsigned long long>(length)) == 0;
    if (!account || !validInput || !verified) {
        recordFailure();
        return false; // The presentation always uses the same invalid-credentials message.
    }

    // A private candidate permits the scoped load, but the shared session stays
    // signed out until both authentication and the entire database load succeed.
    Session candidate;
    candidate.user_ = account->user;
    ExpenseList loaded;
    database_.loadExpenses(candidate, loaded);
    session_.user_ = std::move(candidate.user_);
    records_.swap(loaded);
    session_.cache_ready_ = true;
    ++session_.generation_;
    failed_attempts_ = 0;
    retry_at_ = {};
    return true;
}

void AuthService::logout() noexcept {
    session_.cache_ready_ = false;
    session_.user_.reset();
    ++session_.generation_;
    records_.clear();
}

} // namespace expense_tracker
