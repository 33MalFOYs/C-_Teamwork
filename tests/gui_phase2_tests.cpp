#include "expense_page.hpp"
#include "memory_window.hpp"
#include "reports_page.hpp"
#include "chart_widgets.hpp"
#include "expense_tracker/memory_expense_service.hpp"
#include "expense_tracker/phase_status.hpp"

#include <QAbstractButton>
#include <QApplication>
#include <QComboBox>
#include <QDateEdit>
#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPushButton>
#include <QTableWidget>
#include <QTabWidget>
#include <QTest>
#include <QTimer>
#include <QToolTip>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void require(bool condition, const char* description) {
    if (!condition) throw std::runtime_error(description);
}
template <typename T>
T* control(QWidget& window, const char* name) {
    auto* result = window.findChild<T*>(name);
    if (!result) throw std::runtime_error(std::string("Missing GUI control: ") + name);
    return result;
}
void click(QWidget& window, const char* name) {
    control<QPushButton>(window, name)->click();
}
void selectRecord(QTableWidget* table, int id) {
    for (int row = 0; row < table->rowCount(); ++row) {
        if (table->item(row, 0)->data(Qt::UserRole).toInt() == id) {
            table->selectRow(row);
            table->setCurrentCell(row, 0);
            return;
        }
    }
    throw std::runtime_error("Requested expense is missing from table");
}
void confirmDeletion(QWidget& window, QMessageBox::StandardButton answer) {
    bool dialog_seen = false;
    QTimer::singleShot(0, [&] {
        auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
        if (dialog) {
            dialog_seen = true;
            dialog->button(answer)->click();
        }
    });
    click(window, "deleteExpense");
    require(dialog_seen, "Deletion did not request confirmation");
}
void movePointer(QWidget& widget, const QPoint& position) {
    // Send the same public mouse event as hovering in a real window, without
    // relying on the offscreen platform's global cursor implementation.
    QMouseEvent motion(QEvent::MouseMove, QPointF(position), QPointF(widget.mapToGlobal(position)),
                       Qt::NoButton, Qt::NoButton, Qt::NoModifier);
    QApplication::sendEvent(&widget, &motion);
    QApplication::processEvents();
}
void waitForTooltipToClose() {
    // Qt permits a short native tooltip hide delay; let its timer finish.
    for (int attempt = 0; attempt < 30 && !QToolTip::text().isEmpty(); ++attempt) QTest::qWait(20);
    require(QToolTip::text().isEmpty(), "A previous chart value remained in the tooltip after data changed");
}
void checkStandaloneCharts(const QString& screenshots) {
    using namespace expense_tracker::gui;
    QWidget preview;
    preview.setStyleSheet("background:white;color:#20334b;");
    auto* layout = new QVBoxLayout(&preview); layout->setContentsMargins(20, 18, 20, 18);
    auto* title = new QLabel("Fictional spending / USD", &preview);
    title->setStyleSheet("font-size:18px;font-weight:600;"); layout->addWidget(title);
    auto* row = new QHBoxLayout; row->setSpacing(24);
    auto* trend = new TrendChart(&preview);
    auto* categories = new CategoryChart(&preview);
    row->addWidget(trend, 3); row->addWidget(categories, 2); layout->addLayout(row);
    const QStringList days{"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"};
    const QStringList names{"Food", "Transport", "Home", "Shopping", "Health", "Learning", "Entertainment", "Other"};
    const QVector<qint64> values{18000, 6500, 12000, 5500, 3000, 2500, 2000, 500};
    // Eight real fictional categories sum to $500. The compact legend must
    // group the last four without losing them from the accessible breakdown.
    trend->setSeries({3200, 8700, 4400, 1900, 5600, 18000, 8200}, days);
    categories->setCategories(names, values);
    preview.resize(940, 370); preview.show(); QApplication::processEvents();
    const auto categoryDescription = categories->accessibleDescription();
    for (const auto& name : names)
        require(categoryDescription.contains(name + ": USD "), "Category chart omitted an original category from its accessible breakdown");
    require(categoryDescription.contains("Other: USD 5.00") && categoryDescription.contains("Learning: USD 25.00"),
            "Small categories lost their exact amounts when the legend was grouped");
    movePointer(*categories, {3, 3});
    require(QToolTip::text().contains("USD 500.00"), "Category chart total lost money while grouping its legend");
    QToolTip::hideText();
    require(preview.grab().save(screenshots + "/gui-chart-mixed.png"), "Could not capture mixed weekly/category charts");

    CategoryChart wide;
    wide.setStyleSheet("background:white;"); wide.resize(620, 300);
    wide.setCategories(names, values); wide.show(); QApplication::processEvents();
    require(wide.grab().save(screenshots + "/gui-chart-categories-wide.png"), "Could not capture wide category layout");
    wide.close();

    movePointer(*categories, {3, 3});
    trend->setSeries({0, 0, 0, 0, 0, 0, 0}, days);
    require(trend->accessibleDescription().contains("Mon: USD 0.00") && trend->accessibleDescription().contains("Sun: USD 0.00") &&
            !trend->accessibleDescription().contains("180.00"), "Zero-period chart retained stale spending values");
    categories->setCategories({}, {});
    waitForTooltipToClose();
    require(categories->accessibleDescription() == "No spending data", "Empty category chart kept the old category breakdown");
    QApplication::processEvents();
    require(preview.grab().save(screenshots + "/gui-chart-zero.png"), "Could not render zero/empty charts");

    trend->setSeries({1234}, {"One day"});
    require(trend->accessibleDescription() == "One day: USD 12.34", "Single-point chart lost its exact amount");
    QApplication::processEvents();
    require(!trend->grab().isNull(), "Single-point trend could not render");
    movePointer(*trend, trend->rect().center());
    require(QToolTip::text().contains("USD 12.34"), "Single-point chart hover did not show its exact amount");

    // Above double's exact integer range, labels must still keep every cent.
    constexpr qint64 preciseCents = 9007199254740993LL;
    trend->setSeries({preciseCents}, {"Exact amount"});
    waitForTooltipToClose();
    categories->setCategories({"Precise", "Small"}, {preciseCents, 7});
    require(trend->accessibleDescription().contains("USD 90071992547409.93") &&
            categories->accessibleDescription().contains("Precise: USD 90071992547409.93"),
            "Chart labels rounded an exact large amount through floating point");
    QApplication::processEvents();
    movePointer(*categories, {3, 3});
    require(QToolTip::text().contains("USD 90071992547410.00"), "Large category total tooltip lost exact cents");
    QToolTip::hideText();
    const auto beforeOverflow = categories->accessibleDescription();
    bool overflowRejected = false;
    try { categories->setCategories({"Too large", "Extra cent"}, {std::numeric_limits<qint64>::max(), 1}); }
    catch (const std::overflow_error&) { overflowRejected = true; }
    require(overflowRejected && categories->accessibleDescription() == beforeOverflow,
            "Overflowing category total was accepted or replaced the last valid chart");
    movePointer(*categories, {3, 3});
    require(!QToolTip::text().isEmpty(), "Exact amount tooltip did not appear before clearing");
    categories->clear(); trend->clear(); QApplication::processEvents();
    waitForTooltipToClose();
    require(categories->accessibleDescription() == "No spending data" && trend->accessibleDescription() == "No spending data" &&
            categories->toolTip().isEmpty() && trend->toolTip().isEmpty() && QToolTip::text().isEmpty(),
            "Clearing charts left a financial value in accessible text or a tooltip");
    require(!preview.grab().isNull(), "Cleared charts could not render");
    preview.close();
}
}

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    using namespace expense_tracker;
    try {
#ifdef Q_OS_WIN
        // Qt's offscreen Windows platform may not enumerate system fonts.
        // Register a real system font for reproducible, readable test captures;
        // production startup continues to use the native platform's font choice.
        const auto fontPath = QDir(qEnvironmentVariable("SystemRoot", "C:/Windows"))
            .filePath("Fonts/segoeui.ttf");
        const auto fontId = QFontDatabase::addApplicationFont(fontPath);
        require(fontId >= 0, "Cannot load Windows Segoe UI font for GUI test captures");
        const auto families = QFontDatabase::applicationFontFamilies(fontId);
        require(!families.isEmpty(), "Loaded GUI test font has no registered family");
        application.setFont(QFont(families.first(), 10));
#endif
        MemoryExpenseService service;
        gui::MemoryWindow window(service);
        window.show();
        application.processEvents();
        auto* table = control<QTableWidget>(window, "expenseTable");
        auto* total = control<QLabel>(window, "filteredTotal");
        auto* error = control<QLabel>(window, "expenseFeedback");
        auto* amount = control<QLineEdit>(window, "expenseAmount");
        auto* category = control<QLineEdit>(window, "expenseCategory");
        auto* expenseDate = control<QDateEdit>(window, "expenseDate");
        auto* description = control<QLineEdit>(window, "expenseDescription");
        auto* mode = control<QComboBox>(window, "dateMode");
        auto* start = control<QDateEdit>(window, "filterStart");
        auto* end = control<QDateEdit>(window, "filterEnd");
        auto* categoryFilter = control<QLineEdit>(window, "filterCategory");
        auto* weekDate = control<QDateEdit>(window, "weekDate");
        auto* weekSummary = control<QLabel>(window, "weekSummary");
        auto* daily = control<QTableWidget>(window, "dailyTable");
        auto* tabs = control<QTabWidget>(window, "workspaceTabs");
        auto* weeklyTrend = control<QWidget>(window, "weeklyTrendChart");
        auto* weeklyMix = control<QWidget>(window, "weeklyCategoryChart");
        auto* monthlyMix = control<QWidget>(window, "monthlyCategoryChart");
        require(table->rowCount() == 0 && total->text().contains("USD 0.00"), "Empty expense state incorrect");
        require(control<QLabel>(window, "memoryBanner")->text().contains("Nonpersistent"), "Missing temporary-data notice");
        Session untouchedSession;
        require(!untouchedSession.authenticated(), "Memory GUI must not create an authenticated session");
        amount->setFocus();
        QTest::keyClick(amount, Qt::Key_Tab);
        require(category->hasFocus(), "Keyboard Tab did not move from amount to category");

        auto add = [&](const char* value, const char* name, QDate date, const char* details) {
            click(window, "addExpense");
            amount->setText(value); category->setText(name);
            expenseDate->setDate(date); description->setText(details);
            click(window, "saveExpense");
        };
        add("10.00", "Food", QDate(2026, 12, 28), "Fictional Monday meal");
        add("20.00", "Travel", QDate(2026, 12, 30), "Fictional Wednesday trip");
        add("5.00", "Food", QDate(2027, 1, 3), "Fictional Sunday snack");
        add("99.00", "Travel", QDate(2027, 1, 4), "Next week - excluded");
        require(table->rowCount() == 4 && total->text().contains("USD 134.00"), "Form add or table total failed");
        weekDate->setDate(QDate(2027, 1, 1));
        require(weekSummary->text().contains("Total: USD 35.00") && weekSummary->text().contains("Daily average: USD 5.00"), "Weekly total/average incorrect");
        require(daily->rowCount() == 7 && daily->item(0, 0)->text() == "2026-12-28" &&
                daily->item(6, 0)->text() == "2027-01-03" && daily->item(1, 2)->text() == "0.00", "Week boundaries/zero days incorrect");
        control<QDateEdit>(window, "reportMonth")->setDate(QDate(2026, 12, 1));
        require(control<QLabel>(window, "monthSummary")->text().contains("Total: USD 30.00"), "Monthly total incorrect");
        require(weeklyTrend->accessibleDescription().contains("Mon: USD 10.00") &&
                weeklyTrend->accessibleDescription().contains("Tue: USD 0.00") &&
                weeklyTrend->accessibleDescription().contains("Sun: USD 5.00"), "Weekly chart disagrees with dated expense records");
        require(weeklyMix->accessibleDescription().contains("Food: USD 15.00") &&
                weeklyMix->accessibleDescription().contains("Travel: USD 20.00") &&
                monthlyMix->accessibleDescription().contains("Food: USD 10.00"), "Chart category totals include the wrong period");

        const auto screenshots = QDir(QCoreApplication::applicationDirPath()).absoluteFilePath("..");
        application.processEvents();
        require(window.grab().save(screenshots + "/gui-phase2-expenses.png"), "Could not save expense layout image");
        tabs->setCurrentIndex(1);
        application.processEvents();
        require(window.grab().save(screenshots + "/gui-phase2-reports.png"), "Could not save report layout image");
        tabs->setCurrentIndex(0);

        mode->setCurrentIndex(1); start->setDate(QDate(2026, 12, 28));
        click(window, "applyFilters");
        require(table->rowCount() == 1 && total->text().contains("USD 10.00"), "Exact-date filter failed");
        mode->setCurrentIndex(2); start->setDate(QDate(2026, 12, 28)); end->setDate(QDate(2027, 1, 3));
        click(window, "applyFilters");
        require(table->rowCount() == 3 && total->text().contains("USD 35.00"), "Inclusive range filter failed");
        categoryFilter->setText("Food"); click(window, "applyFilters");
        require(table->rowCount() == 2 && total->text().contains("USD 15.00"), "Combined category/date predicate failed");
        require(weeklyMix->accessibleDescription().contains("Travel: USD 20.00"), "Expense filters incorrectly changed the all-expense report chart");
        start->setDate(QDate(2027, 1, 4)); end->setDate(QDate(2026, 12, 27));
        click(window, "applyFilters");
        require(table->rowCount() == 2 && total->text().contains("USD 15.00") && !error->text().isEmpty(), "Invalid filters changed active view or lost error");
        click(window, "refreshExpenses");
        require(table->rowCount() == 2 && total->text().contains("USD 15.00"), "Refresh lost active filter");
        click(window, "clearFilters");
        require(table->rowCount() == 4, "Clear filters did not restore all records");

        selectRecord(table, 1); click(window, "editExpense");
        amount->setText("1.234"); click(window, "saveExpense");
        require(service.searchExpense(1)->amount() == 1000 && !error->text().isEmpty(), "Invalid edit changed record or lost validation error");
        amount->setText("12.50"); click(window, "cancelExpense");
        require(service.searchExpense(1)->amount() == 1000, "Cancel changed the stored expense");
        selectRecord(table, 1); click(window, "editExpense");
        amount->setText("12.50"); click(window, "saveExpense");
        require(service.searchExpense(1)->amount() == 1250 && weekSummary->text().contains("Total: USD 37.50"), "Successful edit did not refresh reports");
        require(weeklyTrend->accessibleDescription().contains("Mon: USD 12.50") &&
                weeklyMix->accessibleDescription().contains("Food: USD 17.50") &&
                monthlyMix->accessibleDescription().contains("Food: USD 12.50"), "Editing an expense left stale chart values");

        selectRecord(table, 1); confirmDeletion(window, QMessageBox::No);
        require(service.searchExpense(1).has_value() && table->rowCount() == 4, "Canceled deletion changed records");
        selectRecord(table, 1); confirmDeletion(window, QMessageBox::Yes);
        require(!service.searchExpense(1) && table->rowCount() == 3 && weekSummary->text().contains("Total: USD 25.00"), "Confirmed deletion did not refresh table/report");
        require(weeklyTrend->accessibleDescription().contains("Mon: USD 0.00") &&
                weeklyMix->accessibleDescription().contains("Food: USD 5.00") &&
                !monthlyMix->accessibleDescription().contains("Food:"), "Deleting an expense left stale chart data");
        click(window, "nextWeek");
        require(weekSummary->text().contains("Total: USD 99.00"), "Next week navigation failed");
        require(weeklyTrend->accessibleDescription().contains("Mon: USD 99.00") &&
                weeklyMix->accessibleDescription().contains("Travel: USD 99.00") &&
                !weeklyMix->accessibleDescription().contains("Food:"), "Week navigation failed to replace chart data");
        click(window, "previousWeek");
        require(weekSummary->text().contains("Total: USD 25.00"), "Previous week navigation failed");
        weekDate->setDate(QDate(2026, 12, 1));
        require(weekSummary->text().contains("No expenses") && daily->rowCount() == 7, "Empty week state failed");
        for (int row = 0; row < 7; ++row) require(daily->item(row, 2)->text() == "0.00", "Empty week contains nonzero day");
        require(weeklyMix->accessibleDescription() == "No spending data" &&
                weeklyTrend->accessibleDescription().contains("Mon: USD 0.00") &&
                !weeklyTrend->accessibleDescription().contains("99.00"), "Empty report period retained previous chart spending");

        categoryFilter->setText("No such category"); click(window, "applyFilters");
        require(table->rowCount() == 0 && total->text().contains("USD 0.00") &&
                control<QLabel>(window, "expenseEmpty")->isVisible(), "Empty filtered view failed");
        click(window, "clearFilters");
        window.resize(850, 720);
        application.processEvents();
        require(window.grab().save(screenshots + "/gui-phase2-small.png"), "Could not save resized layout image");
        weekDate->setDate(QDate(2027, 1, 1));
        tabs->setCurrentIndex(1);
        application.processEvents();
        require(window.grab().save(screenshots + "/gui-phase2-reports-small.png"), "Could not save resized report layout image");

        auto* expensePage = static_cast<gui::ExpensePage*>(tabs->widget(0));
        auto* reportPage = static_cast<gui::ReportsPage*>(tabs->widget(1));
        expensePage->clearSessionData(); reportPage->clearSessionData();
        require(table->rowCount() == 0 && daily->rowCount() == 0 && weekSummary->text().isEmpty(), "View clearing left financial data visible");
        for (auto* chart : {weeklyTrend, weeklyMix, monthlyMix})
            require(chart->accessibleDescription() == "No spending data", "Cleared report retained private chart descriptions");
        require(service.matching(ExpenseFilter{}).size() == 3, "View clearing unexpectedly deleted records");
        window.close();
        require(!window.isVisible(), "Window did not close");
        checkStandaloneCharts(screenshots);
        std::cout << "Phase 2 GUI checks passed: forms, filters, reports, chart updates/empty/clear/exact amounts, deletion confirmation, resize, close.\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
