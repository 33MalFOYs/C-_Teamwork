#include "reports_page.hpp"
#include "chart_widgets.hpp"
#include "theme.hpp"
#include "expense_tracker/expense_workspace.hpp"
#include "expense_tracker/validation.hpp"

#include <QDateEdit>
#include <QFrame>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <exception>

namespace expense_tracker::gui {
namespace {
QTableWidget* reportTable(QWidget* parent, const char* name, const QStringList& headers) {
    auto* table = new QTableWidget(0, headers.size(), parent);
    table->setObjectName(name);
    table->setHorizontalHeaderLabels(headers);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setAlternatingRowColors(true);
    table->setShowGrid(false);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->verticalHeader()->setDefaultSectionSize(30);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    return table;
}
void fillCategories(QTableWidget* table, const std::map<std::string, Money>& values) {
    table->setRowCount(static_cast<int>(values.size()));
    int row = 0;
    for (const auto& entry : values) {
        table->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(entry.first)));
        table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(validation::formatMoney(entry.second))));
        table->item(row, 1)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        ++row;
    }
}
QDateEdit* dateInput(QWidget* parent, const char* name) {
    auto* input = new QDateEdit(QDate::currentDate(), parent);
    input->setObjectName(name);
    input->setCalendarPopup(true);
    input->setDisplayFormat("yyyy-MM-dd");
    input->setDateRange(QDate(minimum_year, 1, 1), QDate(maximum_year, 12, 31));
    input->setKeyboardTracking(false);
    return input;
}
QFrame* card(QWidget* parent, QVBoxLayout*& layout, const QString& title) {
    auto* result = new QFrame(parent); result->setProperty("role", "card");
    layout = new QVBoxLayout(result); layout->setContentsMargins(18, 15, 18, 15); layout->setSpacing(10);
    auto* heading = new QLabel(title, result); heading->setStyleSheet("font-size:15px;font-weight:600;color:#263950;");
    layout->addWidget(heading); return result;
}
void updateCategories(CategoryChart* chart, const std::map<std::string, Money>& categories) {
    QStringList names; QVector<qint64> values;
    for (const auto& entry : categories) { names.append(QString::fromStdString(entry.first)); values.append(entry.second); }
    chart->setCategories(names, values);
}
}

ReportsPage::ReportsPage(ExpenseWorkspace& expenses, QWidget* parent)
    : QWidget(parent), expenses_(expenses), selected_week_date_(validation::localToday()) {
    setObjectName("reportsPage");
    auto* pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scroll);
    scroll->setWidget(content);
    pageLayout->addWidget(scroll);
    auto* layout = new QVBoxLayout(content);
    layout->setSizeConstraint(QLayout::SetMinimumSize);
    layout->setContentsMargins(4, 18, 4, 14);
    layout->setSpacing(16);
    auto* headingRow = new QHBoxLayout;
    headingRow->addWidget(iconBadge("chart", QColor("#8a74d6"), QColor("#f0edff"), content, 46));
    auto* headingText = new QVBoxLayout; headingText->setSpacing(3);
    auto* heading = new QLabel("Spending insights", content);
    heading->setProperty("role", "title"); headingText->addWidget(heading);
    auto* explanation = new QLabel("See your rhythm. Find your biggest categories.", content);
    explanation->setProperty("role", "subtitle"); explanation->setWordWrap(true); headingText->addWidget(explanation);
    headingRow->addLayout(headingText); headingRow->addStretch();
    auto* scope = new QLabel("USD  /  All expenses", content); scope->setProperty("role", "subtitle");
    scope->setToolTip("Reports include all your expenses in the selected period, regardless of the filters on the Expenses page.");
    headingRow->addWidget(scope); layout->addLayout(headingRow);
    auto* periods = new QTabWidget(this);
    periods->setObjectName("reportPeriods");
    periods->setIconSize(QSize(18, 18));
    auto* weekly = new QWidget(periods);
    auto* weekLayout = new QVBoxLayout(weekly);
    weekLayout->setSizeConstraint(QLayout::SetMinimumSize);
    weekLayout->setContentsMargins(0, 16, 0, 0); weekLayout->setSpacing(14);
    auto* weekActions = new QHBoxLayout;
    week_date_ = dateInput(weekly, "weekDate");
    week_date_->setAccessibleName("Date in selected week");
    week_date_->setToolTip("Pick any date. The report covers its full Monday-to-Sunday week.");
    auto* dateLabel = new QLabel("Week of", weekly);
    dateLabel->setBuddy(week_date_);
    auto* previous = new QPushButton(weekly);
    previous->setObjectName("previousWeek");
    setButtonIcon(previous, "chevron-left", "Previous week"); previous->setFixedWidth(42);
    auto* next = new QPushButton(weekly);
    next->setObjectName("nextWeek");
    setButtonIcon(next, "chevron-right", "Next week"); next->setFixedWidth(42);
    auto* current = new QPushButton("This week", weekly);
    current->setObjectName("thisWeek");
    current->setProperty("role", "quiet"); setButtonIcon(current, "calendar", "Go to the current week");
    weekActions->addWidget(previous);
    weekActions->addWidget(dateLabel);
    weekActions->addWidget(week_date_);
    weekActions->addWidget(next);
    weekActions->addWidget(current);
    weekActions->addStretch();
    weekLayout->addLayout(weekActions);
    week_range_ = new QLabel(weekly);
    week_range_->setObjectName("weekRange");
    week_range_->setProperty("role", "subtitle");
    week_summary_ = new QLabel(weekly);
    week_summary_->setObjectName("weekSummary");
    week_summary_->setWordWrap(true);
    week_summary_->setProperty("role", "card");
    week_summary_->setStyleSheet("QLabel {background:white;border:1px solid #e7eef5;border-radius:12px;padding:15px 20px;font-size:15px;color:#263950;}");
    weekLayout->addWidget(week_range_);
    weekLayout->addWidget(week_summary_);
    auto* chartRow = new QHBoxLayout; chartRow->setSpacing(14);
    QVBoxLayout* trendLayout = nullptr;
    auto* trendCard = card(weekly, trendLayout, "Daily spending");
    weekly_trend_ = new TrendChart(trendCard); weekly_trend_->setObjectName("weeklyTrendChart");
    trendLayout->addWidget(weekly_trend_, 1); chartRow->addWidget(trendCard, 3);
    QVBoxLayout* mixLayout = nullptr;
    auto* mixCard = card(weekly, mixLayout, "Where it went");
    weekly_mix_ = new CategoryChart(mixCard); weekly_mix_->setObjectName("weeklyCategoryChart");
    mixLayout->addWidget(weekly_mix_, 1); chartRow->addWidget(mixCard, 2);
    weekLayout->addLayout(chartRow);
    auto* details = new QHBoxLayout; details->setSpacing(14);
    QVBoxLayout* dailyLayout = nullptr;
    auto* dailyCard = card(weekly, dailyLayout, "Every day, in detail");
    daily_ = reportTable(weekly, "dailyTable", {"Date", "Day", "Total (USD)"});
    daily_->setMinimumHeight(260); daily_->setAccessibleName("Daily expenses in the selected week");
    dailyLayout->addWidget(daily_, 1); details->addWidget(dailyCard, 3);
    QVBoxLayout* categoryLayout = nullptr;
    auto* categoryCard = card(weekly, categoryLayout, "Category totals");
    weekly_categories_ = reportTable(weekly, "weeklyCategories", {"Category", "Total (USD)"});
    weekly_categories_->setMinimumHeight(260); weekly_categories_->setAccessibleName("Exact category totals for the selected week");
    categoryLayout->addWidget(weekly_categories_, 1); details->addWidget(categoryCard, 2);
    weekLayout->addLayout(details); weekLayout->addStretch();
    periods->addTab(weekly, uiIcon("calendar"), "Weekly");

    auto* monthly = new QWidget(periods);
    auto* monthLayout = new QVBoxLayout(monthly);
    monthLayout->setSizeConstraint(QLayout::SetMinimumSize);
    monthLayout->setContentsMargins(0, 16, 0, 0); monthLayout->setSpacing(14);
    auto* monthActions = new QHBoxLayout;
    month_date_ = dateInput(monthly, "reportMonth");
    month_date_->setDisplayFormat("MMMM yyyy"); month_date_->setAccessibleName("Selected report month");
    auto* monthLabel = new QLabel("Month", monthly);
    monthLabel->setBuddy(month_date_);
    monthActions->addWidget(monthLabel);
    monthActions->addWidget(month_date_);
    monthActions->addStretch();
    monthLayout->addLayout(monthActions);
    month_summary_ = new QLabel(monthly);
    month_summary_->setObjectName("monthSummary");
    month_summary_->setWordWrap(true);
    month_summary_->setProperty("role", "card");
    month_summary_->setStyleSheet("QLabel {background:#f3f0ff;border:1px solid #e6e0fb;border-radius:12px;padding:20px;font-size:16px;color:#514477;}");
    monthLayout->addWidget(month_summary_);
    QVBoxLayout* monthlyMixLayout = nullptr;
    auto* monthlyMixCard = card(monthly, monthlyMixLayout, "Your monthly spending mix");
    monthly_mix_ = new CategoryChart(monthlyMixCard); monthly_mix_->setObjectName("monthlyCategoryChart");
    monthlyMixLayout->addWidget(monthly_mix_); monthLayout->addWidget(monthlyMixCard);
    QVBoxLayout* monthlyDetailsLayout = nullptr;
    auto* monthlyDetailsCard = card(monthly, monthlyDetailsLayout, "The full breakdown");
    monthly_categories_ = reportTable(monthly, "monthlyCategories", {"Category", "Total (USD)"});
    monthly_categories_->setMinimumHeight(200);
    monthly_categories_->setAccessibleName("Exact category totals for the selected month");
    monthlyDetailsLayout->addWidget(monthly_categories_, 1); monthLayout->addWidget(monthlyDetailsCard);
    monthLayout->addStretch(); periods->addTab(monthly, uiIcon("chart"), "Monthly");
    layout->addWidget(periods, 1);
    feedback_ = new QLabel(this);
    feedback_->setObjectName("reportFeedback");
    feedback_->setWordWrap(true);
    feedback_->setTextFormat(Qt::PlainText);
    layout->addWidget(feedback_);

    connect(week_date_, &QDateEdit::dateChanged, this, [this](const QDate& date) {
        selectWeek(date.toString("yyyy-MM-dd").toStdString());
    });
    connect(previous, &QPushButton::clicked, this, [this] { moveWeek(-7); });
    connect(next, &QPushButton::clicked, this, [this] { moveWeek(7); });
    connect(current, &QPushButton::clicked, this, [this] { selectWeek(validation::localToday()); });
    connect(month_date_, &QDateEdit::dateChanged, this, [this] { refreshMonth(); });
    refresh();
}

void ReportsPage::selectWeek(const std::string& date) {
    try {
        const auto report = expenses_.weekly(date);
        selected_week_date_ = date;
        const QSignalBlocker blocked(week_date_);
        week_date_->setDate(QDate::fromString(QString::fromStdString(date), "yyyy-MM-dd"));
        renderWeekly(report);
        feedback_->clear();
    } catch (const std::exception& error) {
        const QSignalBlocker blocked(week_date_);
        week_date_->setDate(QDate::fromString(QString::fromStdString(selected_week_date_), "yyyy-MM-dd"));
        feedback_->setText(QString::fromUtf8(error.what()));
    }
}

void ReportsPage::moveWeek(int days) {
    try { selectWeek(validation::addDays(selected_week_date_, days)); }
    catch (const std::exception& error) { feedback_->setText(QString::fromUtf8(error.what())); }
}

void ReportsPage::renderWeekly(const WeeklyReport& report) {
    week_range_->setText(QString("Monday %1 through Sunday %2 (inclusive)")
        .arg(QString::fromStdString(report.dates.start), QString::fromStdString(report.dates.end)));
    week_summary_->setText(QString("Total: USD %2   |   Daily average: USD %3\nEntries this week: %1%4")
        .arg(static_cast<qulonglong>(report.count))
        .arg(QString::fromStdString(validation::formatMoney(report.total)))
        .arg(QString::fromStdString(validation::formatDailyAverage(report.total)))
        .arg(report.count == 0 ? " | No expenses in this week." : ""));
    static const char* days[] = {"Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday", "Sunday"};
    daily_->setRowCount(7);
    for (int row = 0; row < 7; ++row) {
        const auto& day = report.days[static_cast<std::size_t>(row)];
        daily_->setItem(row, 0, new QTableWidgetItem(QString::fromStdString(day.date)));
        daily_->setItem(row, 1, new QTableWidgetItem(days[row]));
        daily_->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(validation::formatMoney(day.total))));
        daily_->item(row, 2)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
    fillCategories(weekly_categories_, report.categories);
    QVector<qint64> cents; QStringList labels;
    for (const auto& day : report.days) {
        cents.append(day.total);
        labels.append(QDate::fromString(QString::fromStdString(day.date), "yyyy-MM-dd").toString("ddd"));
    }
    weekly_trend_->setSeries(cents, labels); updateCategories(weekly_mix_, report.categories);
}

void ReportsPage::refreshMonth() {
    try {
        const auto date = month_date_->date();
        renderMonthly(expenses_.monthly({date.year(), date.month()}));
        feedback_->clear();
    } catch (const std::exception& error) { feedback_->setText(QString::fromUtf8(error.what())); }
}

void ReportsPage::renderMonthly(const MonthlyReport& report) {
    month_summary_->setText(QString("Total: USD %4   |   Entries: %3\nSelected month: %1-%2%5")
        .arg(report.period.year).arg(report.period.month, 2, 10, QChar('0'))
        .arg(static_cast<qulonglong>(report.count))
        .arg(QString::fromStdString(validation::formatMoney(report.total)))
        .arg(report.count == 0 ? " | No expenses in this month." : ""));
    fillCategories(monthly_categories_, report.categories);
    updateCategories(monthly_mix_, report.categories);
}

void ReportsPage::refresh() {
    selectWeek(selected_week_date_);
    const QString weeklyError = feedback_->text();
    refreshMonth();
    if (!weeklyError.isEmpty()) {
        const QString monthlyError = feedback_->text();
        feedback_->setText(monthlyError.isEmpty() ? weeklyError : weeklyError + "\n" + monthlyError);
    }
}

void ReportsPage::clearSessionData() {
    for (auto* table : {daily_, weekly_categories_, monthly_categories_}) {
        table->clearContents(); table->setRowCount(0);
    }
    week_range_->clear(); week_summary_->clear(); month_summary_->clear(); feedback_->clear();
    weekly_trend_->clear(); weekly_mix_->clear(); monthly_mix_->clear();
}

} // namespace expense_tracker::gui

