#include "login_window.hpp"
#include <QApplication>
#include <QCheckBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTest>
#include <iostream>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    expense_tracker::gui::LoginWindow window;
    window.show(); application.processEvents();
    auto* username = window.findChild<QLineEdit*>("username");
    auto* password = window.findChild<QLineEdit*>("password");
    auto* confirmation = window.findChild<QLineEdit*>("passwordConfirmation");
    auto* showPassword = window.findChild<QCheckBox*>("showPassword");
    auto* signIn = window.findChild<QPushButton*>("signIn");
    auto* create = window.findChild<QPushButton*>("createAccount");
    auto* feedback = window.findChild<QLabel*>("feedback");
    auto check = [](bool value, const char* message) { if (!value) std::cerr << message << '\n'; return value; };
    if (!check(window.isVisible() && username && password && confirmation && showPassword && signIn && create && feedback, "Login controls missing")) return 1;
    if (!check(password->echoMode() == QLineEdit::Password && !signIn->isEnabled(), "Disconnected form must be masked and gated")) return 1;
    showPassword->click();
    if (!check(password->echoMode() == QLineEdit::Normal, "Show password failed")) return 1;
    showPassword->click();
    password->setText("fictional-password");
    QTest::keyClick(password, Qt::Key_Return);
    if (!check(password->text().isEmpty() && feedback->text().contains("not ready"), "Disconnected Enter must not authenticate")) return 1;
    bool submitted = false;
    window.setSubmitHandler([&](bool registration, std::string name, std::string secret, std::string) {
        submitted = !registration && name == "fictional-user" && secret == "fictional-password";
    });
    window.setConnectionReady(true);
    username->setText("fictional-user"); password->setText("fictional-password");
    QTest::keyClick(password, Qt::Key_Return);
    if (!check(submitted && password->text().isEmpty() && !signIn->isEnabled(), "Submission must dispatch once and disable repeat actions")) return 1;
    window.setBusy(false);
    create->click();
    if (!check(confirmation->isVisible(), "Registration must show password confirmation")) return 1;
    password->setText("fictional-password"); confirmation->setText("different-password"); signIn->click();
    if (!check(feedback->text().contains("match exactly") && password->text().isEmpty(), "Confirmation validation failed")) return 1;
    submitted = false;
    window.setSubmitHandler([&](bool, std::string, std::string, std::string) { submitted = true; });
    password->setText(QString(4097, QChar('x'))); confirmation->setText(QString(4097, QChar('x')));
    if (!check(password->text().size() == 4097, "Oversized passwords were silently truncated")) return 1;
    signIn->click();
    if (!check(!submitted && feedback->text().contains("4096") && password->text().isEmpty(), "Oversized passwords must be rejected explicitly")) return 1;
    window.close();
    if (!check(!window.isVisible(), "Login window failed to close")) return 1;
    std::cout << "Login presentation smoke checks passed.\n";
    return 0;
}
