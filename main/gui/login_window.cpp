#include "login_window.hpp"
#include "theme.hpp"
#include <QCheckBox>
#include <QColor>
#include <QFont>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QPixmap>
#include <QScrollArea>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <limits>
#include <utility>
namespace expense_tracker::gui {
LoginWindow::LoginWindow(QWidget* parent) : QWidget(parent) {
    applyTheme();
    setWindowTitle("EX - Personal Expense Tracker");
    setObjectName("loginPage");
    setMinimumSize(440, 560);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame); scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setObjectName("loginScroll");
    auto* content = new QWidget(scroll);
    auto* layout = new QVBoxLayout(content);
    layout->setSizeConstraint(QLayout::SetMinimumSize);
    layout->setContentsMargins(24, 20, 24, 20); layout->setSpacing(12);
    auto* brandRow = new QHBoxLayout;
    auto* mark = new QLabel("EX", content);
    mark->setObjectName("loginBrandMark"); mark->setAlignment(Qt::AlignCenter);
    mark->setFixedSize(38, 38);
    const QPixmap logo(":/brand/ex-icon.png");
    if (!logo.isNull()) mark->setPixmap(logo.scaled(38, 38, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    else mark->setStyleSheet("background:#182c42;color:white;font-size:20px;font-weight:800;border-radius:10px;");
    mark->setAccessibleName("EX Expense Tracker");
    auto* brandWords = new QVBoxLayout;
    auto* brandName = new QLabel("EX Expense Tracker", content);
    brandName->setStyleSheet("font-size:15px;font-weight:700;color:#21364d;");
    auto* tagline = new QLabel("Your personal spending space", content);
    tagline->setProperty("role", "subtitle");
    brandWords->addWidget(brandName); brandWords->addWidget(tagline); brandWords->setSpacing(2);
    brandRow->addWidget(mark); brandRow->addSpacing(8); brandRow->addLayout(brandWords); brandRow->addStretch();
    layout->addLayout(brandRow);

    auto* hero = new QFrame(content); hero->setObjectName("loginWelcomePanel");
    hero->setStyleSheet("QFrame#loginWelcomePanel { background:qlineargradient(x1:0,y1:0,x2:1,y2:1,stop:0 #08b9e9,stop:1 #087bb9); border:0; border-radius:18px; } QLabel { background:transparent; color:white; }");
    auto* heroLayout = new QHBoxLayout(hero);
    heroLayout->setContentsMargins(22, 14, 18, 14); heroLayout->setSpacing(14);
    auto* heroCopy = new QVBoxLayout; heroCopy->setSpacing(4);
    auto* heroTitle = new QLabel("A clearer view\nof your everyday.", hero);
    heroTitle->setWordWrap(true);
    heroTitle->setStyleSheet("font-size:21px;font-weight:700;color:white;");
    auto* heroCaption = new QLabel("Track. Understand. Plan.", hero);
    heroCaption->setStyleSheet("color:#e0f8ff;font-size:12px;");
    heroCopy->addWidget(heroTitle); heroCopy->addWidget(heroCaption);
    heroLayout->addLayout(heroCopy, 1);
    auto* wallet = iconBadge("wallet", QColor("#ffffff"), QColor("#28b8df"), hero, 64);
    wallet->setAccessibleName("Personal expense workspace");
    heroLayout->addWidget(wallet);
    layout->addWidget(hero);

    auto* card = new QFrame(content); card->setProperty("role", "card"); card->setObjectName("loginCard");
    auto* cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(22, 20, 22, 18); cardLayout->setSpacing(10);
    auto* accountHeading = new QHBoxLayout;
    account_badge_ = iconBadge("user", QColor("#08a9dc"), QColor("#e6f8fe"), card, 42);
    account_badge_->setAccessibleName("Sign in");
    accountHeading->addWidget(account_badge_);
    auto* accountCopy = new QVBoxLayout; accountCopy->setSpacing(3);
    heading_ = new QLabel("Welcome back", card); heading_->setProperty("role", "title");
    heading_->setStyleSheet("font-size:23px;font-weight:700;");
    heading_->setWordWrap(true); accountCopy->addWidget(heading_);
    introduction_ = new QLabel("Your spending, all in one place.", card);
    introduction_->setProperty("role", "subtitle"); introduction_->setWordWrap(true);
    accountCopy->addWidget(introduction_);
    accountHeading->addLayout(accountCopy, 1); cardLayout->addLayout(accountHeading);
    auto* form = new QFormLayout;
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    form->setRowWrapPolicy(QFormLayout::WrapAllRows);
    form->setVerticalSpacing(8); form->setHorizontalSpacing(12);
    username_ = new QLineEdit(card); username_->setObjectName("username");
    password_ = new QLineEdit(card); password_->setObjectName("password");
    confirmation_ = new QLineEdit(card); confirmation_->setObjectName("passwordConfirmation");
    username_->setPlaceholderText("Username");
    username_->setToolTip("3-64 letters, numbers, dots, hyphens or underscores.");
    password_->setPlaceholderText("Password");
    confirmation_->setPlaceholderText("Repeat your password");
    for (auto* field : {username_, password_, confirmation_}) field->setMaxLength(std::numeric_limits<int>::max());
    password_->setEchoMode(QLineEdit::Password); confirmation_->setEchoMode(QLineEdit::Password);
    username_->addAction(uiIcon("user"), QLineEdit::LeadingPosition);
    password_->addAction(uiIcon("lock"), QLineEdit::LeadingPosition);
    confirmation_->addAction(uiIcon("check"), QLineEdit::LeadingPosition);
    username_->setAccessibleName("Username");
    password_->setAccessibleName("Password");
    confirmation_->setAccessibleName("Confirm password");
    password_->setToolTip("Your account password. Use Show password to check what you typed.");
    confirmation_->setToolTip("Repeat the same password exactly.");
    form->addRow("&Username", username_); form->addRow("&Password", password_);
    confirmation_label_ = new QLabel("&Confirm password", card); confirmation_label_->setBuddy(confirmation_);
    form->addRow(confirmation_label_, confirmation_); cardLayout->addLayout(form);
    rules_ = new QLabel("12+ characters. Spaces count. Maximum 4096 UTF-8 bytes.", card);
    rules_->setProperty("role", "subtitle"); rules_->setWordWrap(true); cardLayout->addWidget(rules_);
    show_password_ = new QCheckBox("Show password", card); show_password_->setObjectName("showPassword");
    connect(show_password_, &QCheckBox::toggled, this, [this](bool show) {
        password_->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
        confirmation_->setEchoMode(show ? QLineEdit::Normal : QLineEdit::Password);
    });
    cardLayout->addWidget(show_password_);
    sign_in_ = new QPushButton("Sign in", card); sign_in_->setObjectName("signIn");
    sign_in_->setProperty("role", "primary");
    sign_in_->setMinimumHeight(30);
    setButtonIcon(sign_in_, "arrow-right", "Sign in");
    create_account_ = new QPushButton("Create an account", card); create_account_->setObjectName("createAccount");
    create_account_->setProperty("role", "quiet");
    setButtonIcon(create_account_, "plus", "Create an account");
    back_ = new QPushButton("Back to sign in", card); back_->setObjectName("backToSignIn");
    back_->setProperty("role", "quiet");
    setButtonIcon(back_, "chevron-left", "Back to sign in");
    for (auto* action : {sign_in_, create_account_, back_}) cardLayout->addWidget(action);
    feedback_ = new QLabel("Connecting to your saved expenses...", card);
    feedback_->setObjectName("feedback"); feedback_->setWordWrap(true);
    feedback_->setProperty("role", "subtitle");
    feedback_->setTextFormat(Qt::PlainText); feedback_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    cardLayout->addWidget(feedback_); layout->addWidget(card);
    auto* footer = new QHBoxLayout;
    auto* privacyIcon = iconBadge("shield", QColor("#30b892"), QColor("#e9f8f3"), content, 30);
    privacyIcon->setAccessibleName("Private account");
    footer->addWidget(privacyIcon);
    auto* privacy = new QLabel("Your account. Your records.", content);
    privacy->setProperty("role", "subtitle"); privacy->setWordWrap(true);
    footer->addWidget(privacy, 1);
    reconnect_button_ = new QPushButton("Reconnect", content); reconnect_button_->setObjectName("reconnectDatabase");
    reconnect_button_->setProperty("role", "quiet");
    setButtonIcon(reconnect_button_, "refresh", "Retry connection");
    reconnect_button_->setToolTip("Reload your local connection settings and reconnect.");
    footer->addWidget(reconnect_button_);
    layout->addLayout(footer); layout->addStretch();
    scroll->setWidget(content); outer->addWidget(scroll);
    retry_ = new QTimer(this); retry_->setSingleShot(true);
    connect(retry_, &QTimer::timeout, this, [this] { updateActions(); });
    connect(sign_in_, &QPushButton::clicked, this, [this] { submit(); });
    connect(create_account_, &QPushButton::clicked, this, [this] { setRegistration(true); });
    connect(back_, &QPushButton::clicked, this, [this] { setRegistration(false); });
    connect(reconnect_button_, &QPushButton::clicked, this, [this] { if (reconnect_) reconnect_(); });
    for (auto* field : {username_, password_, confirmation_}) connect(field, &QLineEdit::returnPressed, this, [this] { submit(); });
    setTabOrder(username_, password_); setTabOrder(password_, confirmation_);
    setTabOrder(confirmation_, show_password_); setTabOrder(show_password_, sign_in_);
    setTabOrder(sign_in_, create_account_); setTabOrder(create_account_, back_); setTabOrder(back_, reconnect_button_);
    setRegistration(false);
}
void LoginWindow::setSubmitHandler(Submit handler) { submit_ = std::move(handler); }
void LoginWindow::setReconnectHandler(std::function<void()> handler) { reconnect_ = std::move(handler); }
void LoginWindow::setConnectionReady(bool ready) { connection_ready_ = ready; updateActions(); }
void LoginWindow::setBusy(bool busy) {
    busy_ = busy;
    for (auto* field : {username_, password_, confirmation_}) field->setEnabled(!busy);
    show_password_->setEnabled(!busy); updateActions();
}
void LoginWindow::showFeedback(const QString& message, int retryMilliseconds) {
    QString friendly = message;
    if (message == "Connected to MySQL. Sign in or create an account.") friendly = "Ready when you are. Sign in or create an account.";
    else if (message == "Working...") friendly = "Please wait...";
    feedback_->setText(friendly);
    if (retryMilliseconds > 0) retry_->start(retryMilliseconds);
    updateActions();
}
void LoginWindow::clearCredentials() {
    username_->clear(); password_->clear(); confirmation_->clear(); show_password_->setChecked(false);
}
void LoginWindow::registrationCompleted() {
    setRegistration(false); showFeedback("Account created. Sign in with your new account.");
}
void LoginWindow::setRegistration(bool enabled) {
    if (busy_) return;
    registration_ = enabled;
    confirmation_->setVisible(enabled); confirmation_label_->setVisible(enabled);
    create_account_->setVisible(!enabled); back_->setVisible(enabled);
    heading_->setText(enabled ? "Make it yours" : "Welcome back");
    introduction_->setText(enabled ? "A fresh space for your expenses." : "Your spending, all in one place.");
    account_badge_->setPixmap(uiIcon(enabled ? "plus" : "user", QColor("#08a9dc")).pixmap(22, 22));
    account_badge_->setAccessibleName(enabled ? "Create account" : "Sign in");
    rules_->setVisible(enabled);
    password_->setPlaceholderText(enabled ? "At least 12 characters" : "Password");
    sign_in_->setText(enabled ? "Create account" : "Sign in");
    setButtonIcon(sign_in_, enabled ? "plus" : "arrow-right", enabled ? "Create account" : "Sign in");
    password_->clear(); confirmation_->clear(); updateActions();
}
void LoginWindow::updateActions() {
    sign_in_->setEnabled(connection_ready_ && !busy_ && !retry_->isActive());
    create_account_->setEnabled(!busy_); back_->setEnabled(!busy_);
    reconnect_button_->setEnabled(!busy_ && !retry_->isActive());
}
void LoginWindow::submit() {
    if (busy_ || retry_->isActive()) return;
    if (!connection_ready_ || !submit_) {
        password_->clear(); confirmation_->clear();
        showFeedback("Your saved data is not ready yet. Choose Reconnect to try again."); return;
    }
    auto username = username_->text().toUtf8();
    auto password = password_->text().toUtf8();
    auto confirmation = confirmation_->text().toUtf8();
    password_->clear(); confirmation_->clear();
    if (username.trimmed().isEmpty() || password.isEmpty()) { showFeedback("Enter your username and password."); return; }
    if (password.size() > 4096 || confirmation.size() > 4096) {
        showFeedback("Passwords must not exceed 4096 UTF-8 bytes. Nothing was truncated."); return;
    }
    if (registration_ && password != confirmation) { showFeedback("Password and confirmation must match exactly."); return; }
    setBusy(true);
    submit_(registration_, username.toStdString(), password.toStdString(), confirmation.toStdString());
}
}
