#pragma once
#include <QWidget>
#include <functional>
#include <string>
class QLabel; class QLineEdit; class QCheckBox; class QPushButton; class QTimer;
namespace expense_tracker::gui {
// Submissions request asynchronous authentication; this widget never creates a session.
class LoginWindow final : public QWidget {
public:
    using Submit = std::function<void(bool, std::string, std::string, std::string)>;
    explicit LoginWindow(QWidget* parent = nullptr);
    void setSubmitHandler(Submit handler);
    void setReconnectHandler(std::function<void()> handler);
    void setConnectionReady(bool ready);
    void setBusy(bool busy);
    void showFeedback(const QString& message, int retryMilliseconds = 0);
    void registrationCompleted();
    void clearCredentials();
private:
    void submit();
    void setRegistration(bool enabled);
    void updateActions();
    Submit submit_;
    std::function<void()> reconnect_;
    bool connection_ready_ = false, busy_ = false, registration_ = false;
    QLineEdit *username_, *password_, *confirmation_;
    QLabel *confirmation_label_, *feedback_, *heading_, *introduction_, *rules_, *account_badge_;
    QCheckBox* show_password_;
    QPushButton *sign_in_, *create_account_, *back_, *reconnect_button_;
    QTimer* retry_;
};
}
