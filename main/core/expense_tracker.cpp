#include "expense_tracker/expense_tracker.hpp"
#include "expense_tracker/auth_service.hpp"
#include "expense_tracker/expense_service.hpp"
#include "expense_tracker/validation.hpp"
#include <iostream>
#include <string>
#include <sodium.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <termios.h>
#include <unistd.h>
#endif

namespace expense_tracker {
namespace {
struct EndOfInput {};
struct Canceled {};
struct Passwords {
    std::string password;
    std::string confirmation;
    ~Passwords() {
        if (!password.empty()) sodium_memzero(password.data(), password.size());
        if (!confirmation.empty()) sodium_memzero(confirmation.data(), confirmation.size());
    }
};

// Restore console echo on every path, including EOF and exceptions.
class HiddenInput {
public:
    explicit HiddenInput(bool console) {
#ifdef _WIN32
        handle_ = GetStdHandle(STD_INPUT_HANDLE);
        changed_ = console && GetConsoleMode(handle_, &old_mode_) &&
                   SetConsoleMode(handle_, old_mode_ & ~ENABLE_ECHO_INPUT);
#elif defined(__unix__) || defined(__APPLE__)
        changed_ = console && isatty(STDIN_FILENO) && tcgetattr(STDIN_FILENO, &old_mode_) == 0;
        if (changed_) {
            auto hidden = old_mode_;
            hidden.c_lflag &= static_cast<tcflag_t>(~ECHO);
            changed_ = tcsetattr(STDIN_FILENO, TCSANOW, &hidden) == 0;
        }
#else
        (void)console;
#endif
    }
    ~HiddenInput() {
#ifdef _WIN32
        if (changed_) SetConsoleMode(handle_, old_mode_);
#elif defined(__unix__) || defined(__APPLE__)
        if (changed_) tcsetattr(STDIN_FILENO, TCSANOW, &old_mode_);
#endif
    }
private:
#ifdef _WIN32
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    DWORD old_mode_ = 0;
#elif defined(__unix__) || defined(__APPLE__)
    termios old_mode_{};
#endif
    bool changed_ = false;
};

void read(std::istream& input, std::ostream& output, const char* prompt,
          std::string& value, bool password = false) {
    output << prompt << std::flush;
    {
        HiddenInput hidden(password && &input == &std::cin);
        if (!std::getline(input, value)) throw EndOfInput{};
    }
    if (password) output << '\n';
    if (value == "/cancel") throw Canceled{};
}
}

int ExpenseTracker::run(std::istream& input, std::ostream& output, AuthService& auth,
                        ExpenseService& expenses, Session& session) {
    output << "Personal Expense Tracker - MySQL storage\n"
           << "Sign in to access your own expenses. Password input is hidden in an interactive console.\n";
    while (true) {
        output << "\n1. Sign in\n2. Create account\n0. Exit\n";
        try {
            std::string choice;
            read(input, output, "Choice: ", choice);
            choice = validation::trimAscii(choice);
            if (choice == "0") { auth.logout(); output << "Goodbye.\n"; return 0; }
            if (choice != "1" && choice != "2") {
                output << "Invalid choice. Enter 0, 1, or 2.\n";
                continue;
            }
            std::string username;
            Passwords secret;
            read(input, output, "Username (/cancel to cancel): ", username);
            read(input, output, "Password: ", secret.password, true);
            if (choice == "2") {
                read(input, output, "Confirm password: ", secret.confirmation, true);
                auth.registerAccount(username, secret.password, secret.confirmation);
                output << "Account created. Sign in with your password.\n";
                continue;
            }
            if (!auth.signIn(username, secret.password)) {
                output << "Invalid username or password.\n";
                continue;
            }
            sodium_memzero(secret.password.data(), secret.password.size());
            secret.password.clear();
            output << "Signed in as " << session.currentUser()->username << ".\n";
            const int result = runPersistent(input, output, expenses);
            auth.logout();
            if (result == 2) {
                output << "Logged out. All account data cleared from this session.\n";
                continue;
            }
            return result;
        } catch (const EndOfInput&) {
            auth.logout();
            if (input.eof()) { output << "\nEnd of input. Goodbye.\n"; return 0; }
            output << "\nCould not read input. Exiting.\n";
            return 1;
        } catch (const Canceled&) {
            output << "Action canceled.\n";
        } catch (const std::exception& error) {
            output << "Error: " << error.what() << '\n';
        }
    }
}
} // namespace expense_tracker
