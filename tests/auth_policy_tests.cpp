#include "expense_tracker/auth_service.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class Action>
void rejects(Action action, const char* message) {
    try { action(); }
    catch (const std::invalid_argument&) { return; }
    throw std::runtime_error(message);
}
}

int main() {
    using expense_tracker::AuthService;
    try {
        require(AuthService::normalizeUsername(" \tStudent.ONE-2_\r\n") == "student.one-2_", "Username canonicalization failed");
        require(AuthService::normalizeUsername(std::string(64, 'A')) == std::string(64, 'a'), "Username maximum length failed");
        for (const auto& username : {"ab", "", "a b", "student@example", "' OR 1=1", "a\nb"}) {
            rejects([&] { AuthService::normalizeUsername(username); }, "Invalid username accepted");
        }
        rejects([] { AuthService::normalizeUsername(std::string(65, 'a')); }, "Overlong username accepted");
        rejects([] { AuthService::normalizeUsername("abc\xc3\xa9"); }, "Non-ASCII username accepted");

        AuthService::validatePassword("twelvechars!");
        AuthService::validatePassword(" 1234567890 "); // Spaces are real password characters.
        AuthService::validatePassword(std::string(4096, 'p'));
        rejects([] { AuthService::validatePassword("short"); }, "Short password accepted");
        rejects([] { AuthService::validatePassword(std::string(4097, 'p')); }, "Overlong password silently accepted");
        std::string twelveKhmer;
        for (int index = 0; index < 12; ++index) twelveKhmer += "\xe1\x9e\x80";
        AuthService::validatePassword(twelveKhmer);
        rejects([&] { AuthService::validatePassword(twelveKhmer.substr(0, 33)); }, "Password length counted UTF-8 bytes instead of code points");
        rejects([] { AuthService::validatePassword("abcdefghijkl\xc0\xaf"); }, "Overlong UTF-8 accepted");
        rejects([] { AuthService::validatePassword("abcdefghijkl\xed\xa0\x80"); }, "Surrogate UTF-8 accepted");
        rejects([] { AuthService::validatePassword("abcdefghijkl\xf4\x90\x80\x80"); }, "Out-of-range UTF-8 accepted");
        rejects([] { AuthService::validatePassword("abcdefghijkl\xe1\x9e"); }, "Truncated UTF-8 accepted");
        std::cout << "Account input policy checks passed.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
