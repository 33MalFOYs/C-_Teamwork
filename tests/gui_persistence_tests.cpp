#include "main_window.hpp"
#include "expense_tracker/config.hpp"
#include "expense_tracker/validation.hpp"
#include <QApplication>
#include <QAbstractButton>
#include <QDateEdit>
#include <QDir>
#include <QElapsedTimer>
#include <QFontDatabase>
#include <QLabel>
#include <QIcon>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTest>
#include <QTimer>
#include <QInputDialog>
#include <QFileDialog>
#include <QPointer>
#include <QScrollArea>
#include <QScrollBar>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <stdexcept>

namespace {
void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
template<class T> T* control(QWidget& window, const char* name) {
    auto* result = window.findChild<T*>(name);
    if (!result) throw std::runtime_error(std::string("Missing control: ") + name);
    return result;
}
void settle(expense_tracker::gui::MainWindow& window) {
    QElapsedTimer clock; clock.start();
    do { QApplication::processEvents(); QTest::qWait(10); }
    while (window.controller().state().busy && clock.elapsed() < 15000);
    require(!window.controller().state().busy, "Worker operation timed out");
}
void click(QWidget& window, const char* name) { control<QPushButton>(window, name)->click(); }
void submitAndSettle(expense_tracker::gui::MainWindow& window) {
    int responsiveTicks = 0;
    QTimer pulse;
    QObject::connect(&pulse, &QTimer::timeout, &window, [&] {
        if (window.controller().state().busy) ++responsiveTicks;
    });
    pulse.start(0);
    click(window, "signIn"); settle(window);
    require(responsiveTicks > 0, "GUI event loop did not respond during password/database work");
}
void requireSeparate(QWidget& window, QWidget* first, QWidget* second, const char* description) {
    const QRect firstBounds(first->mapTo(&window, QPoint(0, 0)), first->size());
    const QRect secondBounds(second->mapTo(&window, QPoint(0, 0)), second->size());
    require(!firstBounds.intersects(secondBounds), description);
}
void requireIconActionsAccessible(QWidget& window) {
    for (auto* button : window.findChildren<QPushButton*>()) {
        if (!button->isVisible() || !button->text().trimmed().isEmpty()) continue;
        require(!button->icon().isNull(), "Icon-only action has no visible icon");
        require(!button->accessibleName().trimmed().isEmpty(), "Icon-only action has no accessible name");
        require(!button->toolTip().trimmed().isEmpty(), "Icon-only action has no explanatory tooltip");
    }
}
void captureCompactLogin(expense_tracker::gui::MainWindow& window, const QString& imageRoot, bool registration) {
    const QSize originalSize = window.size();
    window.resize(480, 600); QApplication::processEvents();
    auto* page = control<QWidget>(window, "loginPage");
    auto* scroll = page->findChild<QScrollArea*>();
    require(scroll != nullptr, "Compact login has no scroll area");
    const QString name = registration ? "register" : "login";
    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->minimum());
    QApplication::processEvents();
    require(window.grab().save(imageRoot + "/gui-redesign-" + name + "-compact.png"),
            "Compact login/registration screenshot could not be saved");
    requireIconActionsAccessible(window);
    const char* fields[] = {"username", "password", "signIn", "reconnectDatabase"};
    auto requireReachable = [&](QWidget* field) {
        scroll->ensureWidgetVisible(field, 8, 8); QApplication::processEvents();
        const QRect fieldBounds(field->mapTo(scroll->viewport(), QPoint(0, 0)), field->size());
        require(field->isVisible() && scroll->viewport()->rect().contains(fieldBounds),
                "Compact authentication control cannot be fully reached by scrolling");
    };
    for (const auto* field : fields) requireReachable(control<QWidget>(window, field));
    if (registration) requireReachable(control<QWidget>(window, "passwordConfirmation"));
    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->maximum());
    QApplication::processEvents();
    require(window.grab().save(imageRoot + "/gui-redesign-" + name + "-bottom-compact.png"),
            "Scrolled compact login/registration screenshot could not be saved");
    window.resize(originalSize);
    scroll->verticalScrollBar()->setValue(scroll->verticalScrollBar()->minimum());
    QApplication::processEvents();
}
void captureAccountPages(expense_tracker::gui::MainWindow& window, const QString& imageRoot) {
    auto* tabs = control<QTabWidget>(window, "accountTabs");
    const int original = tabs->currentIndex();
    const QSize originalSize = window.size();
    const char* names[] = {"dashboard", "expenses", "reports", "files"};
    const QSize sizes[] = {QSize(1200, 860), QSize(1060, 820), QSize(850, 720)};
    const char* suffixes[] = {"-wide.png", "-normal.png", "-compact.png"};
    for (int sizeIndex = 0; sizeIndex < 3; ++sizeIndex) {
        window.resize(sizes[sizeIndex]);
        for (int page = 0; page < 4; ++page) {
            tabs->setCurrentIndex(page); QApplication::processEvents();
            requireIconActionsAccessible(window);
            if (page == 1) {
                auto* amount = control<QLineEdit>(window, "expenseAmount");
                auto* description = control<QLineEdit>(window, "expenseDescription");
                auto* save = control<QPushButton>(window, "saveExpense");
                requireSeparate(window, amount, description, "Expense amount and description inputs overlap after resizing");
                requireSeparate(window, description, save, "Expense save button overlaps the description input after resizing");
            } else if (page == 2) {
                requireSeparate(window, control<QTableWidget>(window, "dailyTable"),
                                control<QTableWidget>(window, "weeklyCategories"),
                                "Weekly daily and category tables overlap after resizing");
            }
            const auto suffix = suffixes[sizeIndex];
            auto* pageScroll = tabs->widget(page)->findChild<QScrollArea*>();
            require(pageScroll != nullptr, "Scrollable account page missing");
            if (pageScroll) {
                pageScroll->verticalScrollBar()->setValue(pageScroll->verticalScrollBar()->minimum());
                QApplication::processEvents();
            }
            require(window.grab().save(imageRoot + "/gui-redesign-" + names[page] + suffix),
                    "Redesign screenshot could not be saved");
            if (pageScroll) {
                pageScroll->verticalScrollBar()->setValue(pageScroll->verticalScrollBar()->maximum());
                QApplication::processEvents();
                require(window.grab().save(imageRoot + "/gui-redesign-" + names[page] + "-bottom" + suffix),
                        "Scrolled redesign screenshot could not be saved");
                pageScroll->verticalScrollBar()->setValue(pageScroll->verticalScrollBar()->minimum());
            }
            if (page == 2) {
                auto* periods = control<QTabWidget>(window, "reportPeriods");
                periods->setCurrentIndex(1); QApplication::processEvents();
                require(window.grab().save(imageRoot + "/gui-redesign-monthly" + suffix),
                        "Monthly redesign screenshot could not be saved");
                periods->setCurrentIndex(0);
            }
        }
    }
    window.resize(originalSize); tabs->setCurrentIndex(original); QApplication::processEvents();
}
void registerAccount(expense_tracker::gui::MainWindow& window, const std::string& name) {
    click(window, "createAccount");
    QApplication::processEvents();
    const auto imageRoot = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("..");
    require(window.grab().save(imageRoot + "/gui-redesign-register.png"), "Registration image could not be saved");
    captureCompactLogin(window, imageRoot, true);
    control<QLineEdit>(window, "username")->setText(QString::fromStdString(name));
    control<QLineEdit>(window, "password")->setText("Fictional GUI password 2026!");
    control<QLineEdit>(window, "passwordConfirmation")->setText("Fictional GUI password 2026!");
    submitAndSettle(window);
    require(window.controller().state().error.empty() && !window.controller().state().user,
            "Registration failed or authenticated without login");
}
void signIn(expense_tracker::gui::MainWindow& window, const std::string& name) {
    control<QLineEdit>(window, "username")->setText(QString::fromStdString(name));
    control<QLineEdit>(window, "password")->setText("Fictional GUI password 2026!");
    submitAndSettle(window);
    require(window.controller().state().user && window.controller().state().cache_ready,
            "Real GUI sign-in did not load an authenticated account");
}
}
int main(int argc, char* argv[]) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication application(argc, argv);
    using namespace expense_tracker;
    const auto* testPath = argc > 1 && argv[1][0] ? argv[1] : std::getenv("EXPENSE_TEST_CONFIG");
    if (!testPath || !*testPath) { std::cout << "EXPENSE_TEST_CONFIG is required for real GUI integration.\n"; return 77; }
    try {
        const auto configuration = std::filesystem::u8path(testPath);
        const auto config = loadDatabaseConfig(configuration);
        require(config.database.rfind("expense_tracker_test_", 0) == 0,
                "GUI integration tests require a designated expense_tracker_test_ database");
#ifdef Q_OS_WIN
        const auto fontPath = QDir(qEnvironmentVariable("SystemRoot", "C:/Windows")).filePath("Fonts/segoeui.ttf");
        const int font = QFontDatabase::addApplicationFont(fontPath);
        require(font >= 0, "Could not load the offscreen test font");
        QApplication::setFont(QFont(QFontDatabase::applicationFontFamilies(font).front(), 10));
#endif
        const auto stamp = std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count());
        const auto accountA = "gui_a_" + stamp, accountB = "gui_b_" + stamp;
        const auto imageRoot = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("..");
        int expenseId = 0;
        {
            gui::MainWindow window(configuration); window.show();
            QApplication::processEvents(); settle(window);
            if (!window.controller().state().connected)
                throw std::runtime_error("Test MySQL connection failed: " + window.controller().state().error);
            require(window.grab().save(imageRoot + "/gui-phase3-login.png"), "Login image could not be saved");
            captureCompactLogin(window, imageRoot, false);
            registerAccount(window, accountA);
            control<QLineEdit>(window, "username")->setText(QString::fromStdString(accountA));
            control<QLineEdit>(window, "password")->setText("Wrong fictional password");
            click(window, "signIn"); settle(window);
            require(!window.controller().state().user && window.controller().state().error == "Invalid username or password.",
                    "Incorrect GUI credentials must fail generically");
            signIn(window, accountA);
            auto* tabs = control<QTabWidget>(window, "accountTabs");
            click(window, "dashboardReports");
            require(tabs->currentIndex() == 2, "Dashboard report shortcut did not navigate");
            tabs->setCurrentIndex(0); click(window, "dashboardFiles");
            require(tabs->currentIndex() == 3, "Dashboard file shortcut did not navigate");
            tabs->setCurrentIndex(0); click(window, "dashboardAddExpense");
            QApplication::processEvents();
            require(tabs->currentIndex() == 1 && control<QLineEdit>(window, "expenseAmount")->hasFocus(),
                    "Dashboard add shortcut did not open and focus the expense form");
            control<QLineEdit>(window, "expenseAmount")->setText("12.34");
            control<QLineEdit>(window, "expenseCategory")->setText("Food");
            control<QDateEdit>(window, "expenseDate")->setDate(QDate::currentDate());
            control<QLineEdit>(window, "expenseDescription")->setText("Fictional GUI meal, with 'quotes' and \"double quotes\"");
            click(window, "saveExpense"); settle(window);
            const auto rows = window.controller().workspace().matching(ExpenseFilter{});
            require(rows.size() == 1 && rows[0].amount() == 1234, "GUI save did not reload the database row");
            expenseId = rows[0].id();
            tabs->setCurrentIndex(0);
            control<QLineEdit>(window, "budgetAmount")->setText("30.00");
            click(window, "saveBudget"); settle(window);
            require(window.controller().state().current_month_budget == std::optional<Money>(3000), "GUI budget was not persisted");
            require(control<QLabel>(window, "dashboardSummary")->text().contains("17.66"), "Dashboard budget remaining is wrong");
            require(window.grab().save(imageRoot + "/gui-phase3-dashboard.png"), "Dashboard image could not be saved");
            captureAccountPages(window, imageRoot);
            QPointer<QWidget> previousFinancial = control<QWidget>(window, "financialWorkspace");
            QPointer<QLabel> previousWeek = control<QLabel>(window, "weekAmount");
            QPointer<QLabel> previousMonth = control<QLabel>(window, "monthAmount");
            QPointer<QLabel> previousRemaining = control<QLabel>(window, "dashboardSummary");
            QPointer<QWidget> previousTrend = control<QWidget>(window, "dashboardTrend");
            QPointer<QWidget> previousCategories = control<QWidget>(window, "dashboardCategories");
            click(window, "logout");
            require(!previousFinancial || !previousFinancial->isVisible(), "Logout left financial widgets visible");
            require((!previousTrend || !previousTrend->isVisible()) &&
                    (!previousCategories || !previousCategories->isVisible()),
                    "Logout left the previous account's charts visible");
            require((!previousWeek || !previousWeek->text().contains("12.34")) &&
                    (!previousMonth || !previousMonth->text().contains("12.34")) &&
                    (!previousRemaining || !previousRemaining->text().contains("17.66")),
                    "Logout retained the previous account's dashboard amounts");
            bool signedOutReadRejected = false;
            try { (void)window.controller().workspace().matching(ExpenseFilter{}); }
            catch (const std::exception&) { signedOutReadRejected = true; }
            require(signedOutReadRejected, "Logout left the GUI snapshot readable");
            require(!window.controller().state().user, "Logout did not clear account immediately");
            settle(window);
            require(!control<QLineEdit>(window, "username")->text().size(), "Logout retained old username");
            registerAccount(window, accountB); signIn(window, accountB);
            require(window.controller().workspace().matching(ExpenseFilter{}).empty(), "Account B saw Account A's rows");
            require(!control<QLabel>(window, "dashboardSummary")->text().contains("17.66"),
                    "Account B dashboard retained Account A's budget");
            require(!control<QLabel>(window, "accountIdentity")->text().contains(QString::fromStdString(accountA)),
                    "Account B header retained Account A's identity");
            require(previousTrend.isNull() && previousCategories.isNull(),
                    "Account switch retained the previous account's chart widgets");
            window.controller().save(expenseId, {999, "Other", validation::localToday(), "must fail"}, {});
            settle(window);
            require(!window.controller().state().error.empty(), "Account B edited Account A's known ID");
            // Queue a refresh, then log out before its callback arrives. The old
            // response must not reconstruct financial pages after logout.
            window.controller().reload(); window.controller().logout(); settle(window);
            require(!window.controller().state().user, "An old worker response restored a logged-out account");
            signIn(window, accountA);
            require(window.controller().workspace().searchExpense(expenseId)->amount() == 1234,
                    "Denied cross-account edit changed Account A's record");
            auto* table = control<QTableWidget>(window, "expenseTable");
            control<QTabWidget>(window, "accountTabs")->setCurrentIndex(1);
            table->setCurrentCell(0, 0); table->selectRow(0); click(window, "editExpense");
            control<QLineEdit>(window, "expenseAmount")->setText("15.75");
            click(window, "saveExpense"); settle(window);
            require(window.controller().workspace().searchExpense(expenseId)->amount() == 1575,
                    "GUI edit did not persist");
            require(window.grab().save(imageRoot + "/gui-phase3-expenses.png"), "Expense image could not be saved");
            QTimer::singleShot(0,[&] {
                auto* dialog=qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
                if(dialog){dialog->setTextValue(QString::number(expenseId));dialog->accept();}
            });
            click(window,"findExpenseId");
            require(table->currentRow()>=0 && table->item(table->currentRow(),0)->text().toInt()==expenseId,"Find ID dialog failed");
            const auto dialogExport=QDir(imageRoot).filePath("dialog-export-"+QString::fromStdString(stamp)+".csv");
            control<QTabWidget>(window,"accountTabs")->setCurrentIndex(3);
            QTimer::singleShot(0,[&] {
                auto* dialog=qobject_cast<QFileDialog*>(QApplication::activeModalWidget());
                if(dialog){dialog->selectFile(dialogExport);QMetaObject::invokeMethod(dialog,"accept",Qt::QueuedConnection);}
            });
            click(window,"csvAction1");settle(window);
            require(std::filesystem::exists(std::filesystem::u8path(dialogExport.toStdString())),"CSV export dialog failed");
            const auto exportPath=std::filesystem::u8path((imageRoot+"/gui-phase4-"+QString::fromStdString(stamp)+".csv").toStdString());
            window.controller().csv(1,exportPath,{},false,{});settle(window);
            require(window.controller().state().error.empty() && std::filesystem::exists(exportPath),"GUI worker CSV export failed");
            window.controller().csv(0,exportPath,{},false,{});settle(window);
            require(window.controller().workspace().matching(ExpenseFilter{}).size()==2,"GUI worker CSV import failed");
            const auto filteredExport=exportPath.string()+".filtered.csv";
            window.controller().csv(3,filteredExport,{},false,{},ExpenseFilter(DateRange{"2025-01-01","2025-01-01"},{}));settle(window);
            {std::ifstream exported(filteredExport);std::string line;int lines=0;while(std::getline(exported,line))++lines;require(lines==1,"GUI filtered export ignored predicate");}
            auto imported=window.controller().workspace().matching(ExpenseFilter{}).back().id();
            window.controller().erase(imported,{});settle(window);
            window.controller().csv(2,exportPath.string()+".weekly.csv",validation::localToday(),false,{});settle(window);
            require(window.controller().state().error.empty(),"GUI weekly CSV failed");
            control<QTabWidget>(window,"accountTabs")->setCurrentIndex(3);
            window.resize(850,720);QApplication::processEvents();
            require(window.grab().save(imageRoot+"/gui-phase4-csv.png"),"CSV screenshot failed");
            control<QTabWidget>(window,"accountTabs")->setCurrentIndex(1);
            require(window.grab().save(imageRoot+"/gui-phase4-expenses.png"),"Resized expense screenshot failed");
            window.close();
        }
        {
            gui::MainWindow restarted(configuration); restarted.show(); QApplication::processEvents(); settle(restarted);
            require(!restarted.controller().state().user, "Application restart should require login");
            signIn(restarted, accountA);
            require(restarted.controller().workspace().searchExpense(expenseId)->amount() == 1575,
                    "Database row did not survive GUI restart");
            require(restarted.controller().state().current_month_budget == std::optional<Money>(3000),
                    "Budget did not survive GUI restart");
            control<QTabWidget>(restarted, "accountTabs")->setCurrentIndex(1);
            auto* table = control<QTableWidget>(restarted, "expenseTable");
            table->setCurrentCell(0, 0); table->selectRow(0);
            bool confirmationSeen = false;
            QTimer::singleShot(0, [&] {
                auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
                if (dialog) { confirmationSeen = true; dialog->button(QMessageBox::Yes)->click(); }
            });
            click(restarted, "deleteExpense"); settle(restarted);
            require(confirmationSeen && restarted.controller().workspace().matching(ExpenseFilter{}).empty(),
                    "Confirmed persistent GUI deletion failed");
            restarted.controller().reload(); settle(restarted);
            require(!restarted.controller().workspace().searchExpense(expenseId), "Deleted database row reappeared after reload");
            restarted.close();
        }
        std::cout << "Real MySQL GUI integration passed: registration, login, CRUD, budgets, two-account isolation, logout/stale callbacks, restart.\n";
        return 0;
    } catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << '\n'; return 1; }
}
