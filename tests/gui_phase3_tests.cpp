#include "main_window.hpp"
#include "persistent_controller.hpp"
#include <QApplication>
#include <QElapsedTimer>
#include <QLabel>
#include <QPushButton>
#include <QTest>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
}
int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    using namespace expense_tracker;
    try {
        // These are intentionally invalid configuration values, not credentials.
        qputenv("EXPENSE_DB_HOST", "127.0.0.1");
        qputenv("EXPENSE_DB_PORT", "not-a-port");
        qputenv("EXPENSE_DB_USER", "fictional-test");
        qputenv("EXPENSE_DB_PASSWORD", "fictional-test-password");
        gui::MainWindow window("missing-phase3-test.config");
        window.show();
        QElapsedTimer wait; wait.start();
        do { application.processEvents(); QTest::qWait(10); }
        while ((window.controller().state().busy || window.controller().state().error.empty()) && wait.elapsed() < 10000);
        require(!window.controller().state().busy && !window.controller().state().connected,
                "Invalid configuration did not fail closed asynchronously");
        require(!window.controller().state().user, "Invalid configuration created a session");
        auto* feedback = window.findChild<QLabel*>("feedback");
        require(feedback && !feedback->text().isEmpty(), "Database error was not shown inline");
        require(!window.findChild<QWidget*>("financialWorkspace"), "Database failure opened a financial workspace");
        require(!window.findChild<QPushButton*>("signIn")->isEnabled(), "Disconnected login remained enabled");
        bool rejected = false;
        try { window.controller().workspace().matching(ExpenseFilter{}); }
        catch (const std::logic_error&) { rejected = true; }
        require(rejected, "An unauthenticated UI cache allowed reads");
        window.controller().logout();
        require(!window.controller().state().user, "Logout did not clear state immediately");
        std::cout << "Phase 3 offline GUI checks passed: async config failure, inline feedback, no financial access.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n'; return 1;
    }
}
