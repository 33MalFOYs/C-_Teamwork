#include "expense_page.hpp"
#include "theme.hpp"
#include "expense_tracker/expense_workspace.hpp"
#include "expense_tracker/validation.hpp"

#include <QAbstractItemView>
#include <QBoxLayout>
#include <QBrush>
#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QFont>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QInputDialog>
#include <QPushButton>
#include <QPointer>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <exception>
#include <utility>

namespace expense_tracker::gui {
namespace {
QDateEdit* makeDate(QWidget* parent, const char* name) {
    auto* field = new QDateEdit(QDate::currentDate(), parent);
    field->setObjectName(name);
    field->setCalendarPopup(true);
    field->setDisplayFormat("yyyy-MM-dd");
    field->setDateRange(QDate(minimum_year, 1, 1), QDate(maximum_year, 12, 31));
    field->setKeyboardTracking(false);
    return field;
}
std::string dateText(const QDateEdit* field) {
    return field->date().toString("yyyy-MM-dd").toStdString();
}
QPushButton* button(const char* text, const char* name, QWidget* parent) {
    auto* result = new QPushButton(text, parent);
    result->setObjectName(name);
    return result;
}
QString categoryGlyph(const QString& category) {
    const auto text = category.toLower();
    if (text.contains("food") || text.contains("meal") || text.contains("coffee")) return "food";
    if (text.contains("travel") || text.contains("transport") || text.contains("fuel")) return "transport";
    if (text.contains("shop") || text.contains("cloth")) return "shopping";
    if (text.contains("home") || text.contains("rent") || text.contains("bill")) return "home";
    if (text.contains("health") || text.contains("medical")) return "health";
    if (text.contains("school") || text.contains("education")) return "education";
    if (text.contains("entertain") || text.contains("movie")) return "entertainment";
    return "other";
}
}

ExpensePage::ExpensePage(ExpenseWorkspace& expenses, QWidget* parent)
    : QWidget(parent), expenses_(expenses) {
    setObjectName("expensePage");
    auto* pageLayout = new QVBoxLayout(this);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    scroll_ = new QScrollArea(this);
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scroll_);
    auto* layout = new QVBoxLayout(content);
    layout->setSizeConstraint(QLayout::SetMinimumSize);
    layout->setContentsMargins(4, 18, 4, 12);
    layout->setSpacing(18);
    scroll_->setWidget(content);
    pageLayout->addWidget(scroll_);

    auto* heading = new QHBoxLayout;
    heading->setSpacing(12);
    heading->addWidget(iconBadge("receipt", QColor("#00a9df"), QColor("#e5f7fd"), content, 46));
    auto* headingText = new QVBoxLayout;
    auto* title = new QLabel("Expenses", content);
    title->setProperty("role", "title");
    auto* hint = new QLabel("Every purchase, neatly kept.", content);
    hint->setProperty("role", "subtitle");
    headingText->addWidget(title); headingText->addWidget(hint);
    headingText->setSpacing(3);
    heading->addLayout(headingText, 1);
    auto* add = button("New expense", "addExpense", content);
    add->setProperty("role", "primary");
    setButtonIcon(add, "plus", "Add a new expense");
    heading->addWidget(add);
    layout->addLayout(heading);

    body_ = new QBoxLayout(QBoxLayout::LeftToRight);
    body_->setSpacing(18);
    auto* ledger = new QFrame(content);
    ledger->setProperty("role", "card");
    ledger->setObjectName("expenseLedger");
    auto* ledgerLayout = new QVBoxLayout(ledger);
    ledgerLayout->setContentsMargins(18, 18, 18, 16);
    ledgerLayout->setSpacing(14);
    auto* ledgerHeading = new QHBoxLayout;
    auto* ledgerTitle = new QLabel("Transactions", ledger);
    ledgerTitle->setStyleSheet("font-size:16px;font-weight:700;");
    ledgerHeading->addWidget(ledgerTitle, 1);
    auto iconAction = [ledger](const char* name, const char* glyph, const char* description) {
        auto* action = button("", name, ledger);
        action->setProperty("role", "quiet");
        action->setFixedSize(38, 38);
        setButtonIcon(action, glyph, description);
        return action;
    };
    auto* edit = iconAction("editExpense", "edit", "Edit selected expense");
    auto* remove = iconAction("deleteExpense", "trash", "Delete selected expense");
    remove->setProperty("role", "danger");
    remove->setIcon(uiIcon("trash", QColor("#dc6a7a")));
    auto* search = iconAction("findExpenseId", "search", "Find an expense by ID");
    auto* reload = iconAction("refreshExpenses", "refresh", "Refresh saved expenses");
    edit->setEnabled(false); remove->setEnabled(false);
    ledgerHeading->addWidget(search); ledgerHeading->addWidget(edit);
    ledgerHeading->addWidget(remove); ledgerHeading->addWidget(reload);
    ledgerLayout->addLayout(ledgerHeading);

    auto* filters = new QFrame(ledger);
    filters->setObjectName("expenseFilterCard");
    filters->setStyleSheet("QFrame#expenseFilterCard {background:#f6fafc;border-radius:10px;}");
    auto* filterLayout = new QVBoxLayout(filters);
    filterLayout->setContentsMargins(12, 12, 12, 12);
    filterLayout->setSpacing(9);
    auto* dateFields = new QHBoxLayout;
    dateFields->setSpacing(8);
    date_mode_ = new QComboBox(filters);
    date_mode_->setObjectName("dateMode");
    date_mode_->setAccessibleName("Filter expense dates");
    date_mode_->setToolTip("Choose all dates, an exact date, a range, this week, or this month.");
    date_mode_->addItems({"All dates", "Exact date", "Custom range", "This week", "This month"});
    date_mode_->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    date_mode_->setMinimumContentsLength(9);
    dateFields->addWidget(date_mode_, 1);
    filter_start_ = makeDate(filters, "filterStart");
    filter_start_->setAccessibleName("Filter start date");
    filter_start_->setToolTip("First date, included in the results.");
    filter_end_ = makeDate(filters, "filterEnd");
    filter_end_->setAccessibleName("Filter end date");
    filter_end_->setToolTip("Last date, included in the results.");
    filter_start_->setMinimumWidth(118); filter_end_->setMinimumWidth(118);
    dateFields->addWidget(filter_start_);
    auto* dateArrow = new QLabel(filters);
    dateArrow->setPixmap(uiIcon("chevron-right").pixmap(14, 14));
    dateFields->addWidget(dateArrow);
    dateFields->addWidget(filter_end_);
    filterLayout->addLayout(dateFields);
    auto* filterRow = new QHBoxLayout;
    filterRow->setSpacing(8);
    filter_category_ = new QLineEdit(filters);
    filter_category_->setObjectName("filterCategory");
    filter_category_->setPlaceholderText("Search a category");
    filter_category_->setAccessibleName("Filter by exact category");
    filter_category_->setToolTip("Match the category exactly, including capitals. Leave blank for all categories.");
    filter_category_->addAction(uiIcon("search"), QLineEdit::LeadingPosition);
    filterRow->addWidget(filter_category_, 1);
    auto* apply = button("Filter", "applyFilters", filters);
    apply->setProperty("role", "primary");
    setButtonIcon(apply, "filter", "Apply date and category filters");
    auto* clear = button("", "clearFilters", filters);
    clear->setProperty("role", "quiet"); clear->setFixedSize(38, 38);
    setButtonIcon(clear, "close", "Clear all filters");
    filterRow->addWidget(apply); filterRow->addWidget(clear);
    filterLayout->addLayout(filterRow);
    ledgerLayout->addWidget(filters);

    table_ = new QTableWidget(0, 5, ledger);
    table_->setObjectName("expenseTable");
    table_->setAccessibleName("Expense transactions");
    table_->setHorizontalHeaderLabels({"ID", "USD", "Category", "Date", "Note"});
    table_->setColumnHidden(0, true);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setSelectionMode(QAbstractItemView::SingleSelection);
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setAlternatingRowColors(false);
    table_->setShowGrid(false); table_->setWordWrap(false);
    table_->setToolTip("Select a transaction to edit or delete. Double-click to open its receipt.");
    table_->setIconSize(QSize(20, 20));
    table_->verticalHeader()->setDefaultSectionSize(48);
    table_->verticalHeader()->hide();
    table_->horizontalHeader()->setMinimumSectionSize(60);
    for (int column = 0; column < 4; ++column)
        table_->horizontalHeader()->setSectionResizeMode(column, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Stretch);
    table_->setMinimumHeight(250);
    table_->setStyleSheet("QTableWidget{border:0;background:white;} QHeaderView::section{background:white;}");
    ledgerLayout->addWidget(table_, 1);

    empty_state_ = new QFrame(ledger);
    auto* emptyLayout = new QHBoxLayout(empty_state_);
    emptyLayout->setContentsMargins(6, 10, 6, 10);
    emptyLayout->addWidget(iconBadge("receipt", QColor("#9bb0bf"), QColor("#f0f6f9"), empty_state_, 42));
    empty_ = new QLabel("No expenses yet. Add your first purchase.", empty_state_);
    empty_->setObjectName("expenseEmpty");
    empty_->setWordWrap(true); empty_->setProperty("role", "subtitle");
    emptyLayout->addWidget(empty_, 1); ledgerLayout->addWidget(empty_state_);
    total_ = new QLabel(ledger);
    total_->setObjectName("filteredTotal");
    total_->setWordWrap(true);
    total_->setStyleSheet("color:#3b5268;font-weight:600;padding:8px 0;");
    ledgerLayout->addWidget(total_);

    form_ = new QGroupBox("New expense", content);
    form_->setObjectName("expenseReceipt");
    form_->setProperty("role", "card");
    form_->setMinimumWidth(280);
    auto* formLayout = new QVBoxLayout(form_);
    formLayout->setSizeConstraint(QLayout::SetMinimumSize);
    formLayout->setContentsMargins(18, 16, 18, 16);
    formLayout->setSpacing(10);
    auto* receiptHeading = new QHBoxLayout;
    receiptHeading->addWidget(iconBadge("receipt", QColor("#00a9df"), QColor("#e5f7fd"), form_, 38));
    auto* receiptHint = new QLabel("A little detail.\nA clearer picture.", form_);
    receiptHint->setProperty("role", "subtitle");
    receiptHeading->addWidget(receiptHint, 1); formLayout->addLayout(receiptHeading);
    auto* amountLabel = new QLabel("&Amount", form_);
    amountLabel->setProperty("role", "eyebrow");
    amount_ = new QLineEdit(form_); amount_->setObjectName("expenseAmount");
    amount_->setPlaceholderText("0.00"); amount_->setAccessibleName("Expense amount in USD");
    amount_->setToolTip("Amount in US dollars, for example 12.50.");
    amount_->setStyleSheet("font-size:28px;font-weight:700;color:#18354e;padding:10px 8px;");
    amount_->setMinimumWidth(0);
    amountLabel->setBuddy(amount_);
    auto* amountRow = new QHBoxLayout;
    auto* currency = new QLabel("USD", form_);
    currency->setStyleSheet("color:#7d91a2;font-size:13px;font-weight:700;");
    amountRow->addWidget(currency); amountRow->addWidget(amount_, 1);
    formLayout->addWidget(amountLabel); formLayout->addLayout(amountRow);
    auto* divider = new QFrame(form_);
    divider->setFixedHeight(1);
    divider->setStyleSheet("background:#e8eff4;");
    formLayout->addWidget(divider);

    auto* categoryLabel = new QLabel("C&ategory", form_);
    categoryLabel->setProperty("role", "eyebrow");
    category_ = new QLineEdit(form_); category_->setObjectName("expenseCategory");
    category_->setPlaceholderText("Choose below or type your own");
    category_->setAccessibleName("Expense category");
    category_->addAction(uiIcon("tag"), QLineEdit::LeadingPosition);
    categoryLabel->setBuddy(category_);
    formLayout->addWidget(categoryLabel); formLayout->addWidget(category_);
    auto* choices = new QHBoxLayout;
    choices->setSpacing(5);
    const QStringList names{"Food", "Travel", "Shopping", "Home", "Health", "Other"};
    const QStringList glyphs{"food", "transport", "shopping", "home", "health", "other"};
    for (int index = 0; index < names.size(); ++index) {
        auto* choice = new QPushButton(form_);
        choice->setObjectName("categoryPreset" + names[index]);
        choice->setCheckable(true); choice->setFixedSize(35, 35);
        choice->setIcon(uiIcon(glyphs[index], QColor("#089cc9")));
        choice->setIconSize(QSize(19, 19));
        choice->setToolTip(names[index]); choice->setAccessibleName(names[index] + " category");
        choice->setStyleSheet("QPushButton{padding:4px;background:#f3f9fc;border:1px solid #e5eef4;border-radius:8px;} QPushButton:hover{background:#e4f5fc;} QPushButton:checked{background:#dff5fd;border:1px solid #00a9df;}");
        connect(choice, &QPushButton::clicked, this, [this, choice, name=names[index]] {
            category_->setText(name); choice->setChecked(true);
        });
        connect(category_, &QLineEdit::textChanged, choice, [choice, name=names[index]](const QString& text) { choice->setChecked(text.compare(name, Qt::CaseInsensitive) == 0); });
        choices->addWidget(choice);
    }
    choices->addStretch(); formLayout->addLayout(choices);
    auto* dateLabel = new QLabel("Expense &date", form_);
    dateLabel->setProperty("role", "eyebrow");
    date_ = makeDate(form_, "expenseDate");
    date_->setAccessibleName("Expense date"); dateLabel->setBuddy(date_);
    formLayout->addWidget(dateLabel); formLayout->addWidget(date_);
    auto* descriptionLabel = new QLabel("&Note  /  optional", form_);
    descriptionLabel->setProperty("role", "eyebrow");
    description_ = new QLineEdit(form_); description_->setObjectName("expenseDescription");
    description_->setPlaceholderText("What was it for?");
    description_->setAccessibleName("Optional expense note");
    description_->addAction(uiIcon("note"), QLineEdit::LeadingPosition);
    descriptionLabel->setBuddy(description_);
    formLayout->addWidget(descriptionLabel); formLayout->addWidget(description_);
    auto* formActions = new QHBoxLayout;
    save_ = button("Add expense", "saveExpense", form_);
    save_->setProperty("role", "primary");
    setButtonIcon(save_, "check", "Save expense");
    auto* cancel = button("", "cancelExpense", form_);
    cancel->setFixedSize(40, 40);
    setButtonIcon(cancel, "close", "Cancel editing and clear this receipt");
    formActions->addWidget(save_, 1); formActions->addWidget(cancel);
    formLayout->addSpacing(4); formLayout->addLayout(formActions);
    feedback_ = new QLabel(form_);
    feedback_->setObjectName("expenseFeedback");
    feedback_->setWordWrap(true); feedback_->setTextFormat(Qt::PlainText);
    feedback_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    formLayout->addWidget(feedback_); formLayout->addStretch();
    body_->addWidget(ledger, 2); body_->addWidget(form_, 1);
    layout->addLayout(body_); layout->addStretch();

    setTabOrder(amount_, category_); setTabOrder(category_, date_);
    setTabOrder(date_, description_); setTabOrder(description_, save_); setTabOrder(save_, cancel);
    connect(table_, &QTableWidget::itemSelectionChanged, this, [this, edit, remove] {
        const bool selected = !table_->selectedItems().isEmpty();
        edit->setEnabled(selected); remove->setEnabled(selected);
    });
    connect(search, &QPushButton::clicked, this, [this] {
        bool ok = false;
        const auto text = QInputDialog::getText(this, "Find expense", "Expense ID", QLineEdit::Normal, {}, &ok);
        if (!ok) return;
        try {
            const auto id = validation::parsePositiveId(text.toStdString());
            const auto found = expenses_.searchExpense(id);
            if (!found) { feedback_->setText("No expense with this ID in your account."); return; }
            clearFilters();
            for (int row = 0; row < table_->rowCount(); ++row) if (table_->item(row, 0)->text().toInt() == id) {
                table_->selectRow(row); table_->setCurrentCell(row, 1);
                table_->scrollToItem(table_->item(row, 1)); break;
            }
        } catch (const std::exception& error) { showError(error); }
    });
    connect(date_mode_, &QComboBox::currentIndexChanged, this, [this] { updateDateControls(); });
    connect(apply, &QPushButton::clicked, this, [this] { applyFilters(); });
    connect(clear, &QPushButton::clicked, this, [this] { clearFilters(); });
    connect(filter_category_, &QLineEdit::returnPressed, this, [this] { applyFilters(); });
    connect(add, &QPushButton::clicked, this, [this] { resetForm(); amount_->setFocus(); scroll_->ensureWidgetVisible(form_); });
    connect(edit, &QPushButton::clicked, this, [this] { editSelected(); });
    connect(table_, &QTableWidget::cellDoubleClicked, this, [this] { editSelected(); });
    connect(remove, &QPushButton::clicked, this, [this] { deleteSelected(); });
    connect(reload, &QPushButton::clicked, this, [this] { reloadRequested(); });
    connect(save_, &QPushButton::clicked, this, [this] { saveExpense(); });
    connect(cancel, &QPushButton::clicked, this, [this] { resetForm(); feedback_->setText("Canceled. No expense was changed."); });
    updateDateControls(); refresh();
}

void ExpensePage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    if (!body_) return;
    const bool columns = width() >= 940;
    body_->setDirection(columns ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
    body_->setStretch(0, columns ? 2 : 0);
    body_->setStretch(1, columns ? 1 : 0);
}
void ExpensePage::setChangedHandler(std::function<void()> handler) {
    changed_ = std::move(handler);
}

void ExpensePage::setAsyncHandlers(AsyncHandlers handlers) { async_ = std::move(handlers); }
void ExpensePage::reloadRequested() {
    if (!async_.reload) { refresh(); return; }
    setEnabled(false);
    QPointer<ExpensePage> page(this);
    async_.reload([page](const std::string& error) {
        if (page) page->asyncFinished(error, "Your expenses are up to date.", false);
    });
}
void ExpensePage::asyncFinished(const std::string& error, const QString& success, bool reset) {
    setEnabled(true);
    if (!error.empty()) {
        // The controller has already invalidated an unsafe cache. refresh()
        // clears stale rows if reads are unavailable; preserve the actual error.
        refresh(); feedback_->setText(QString::fromUtf8(error.c_str())); return;
    }
    if (reset) resetForm();
    if (refresh()) feedback_->setText(success);
    if (changed_) changed_();
}

void ExpensePage::showError(const std::exception& error) {
    feedback_->setText(QString::fromUtf8(error.what()));
}

bool ExpensePage::refresh() {
    try {
        const auto records = expenses_.matching(active_filter_);
        const auto total = expenses_.total(active_filter_);
        table_->setRowCount(static_cast<int>(records.size()));
        int row = 0;
        for (const auto& expense : records) {
            auto* id = new QTableWidgetItem(QString::number(expense.id()));
            id->setData(Qt::UserRole, expense.id());
            table_->setItem(row, 0, id);
            table_->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(validation::formatMoney(expense.amount()))));
            const auto category = QString::fromStdString(expense.category());
            auto* categoryItem = new QTableWidgetItem(uiIcon(categoryGlyph(category), QColor("#06a4d2")), category);
            table_->setItem(row, 2, categoryItem);
            table_->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(expense.date())));
            table_->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(expense.description())));
            table_->item(row, 1)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            auto moneyFont = table_->item(row, 1)->font(); moneyFont.setBold(true);
            table_->item(row, 1)->setFont(moneyFont);
            table_->item(row, 1)->setForeground(QBrush(QColor("#243d55")));
            table_->item(row, 2)->setToolTip(QString("Expense #%1 / %2").arg(expense.id()).arg(category));
            table_->item(row, 3)->setForeground(QBrush(QColor("#7a8d9e")));
            table_->item(row, 4)->setToolTip(QString::fromStdString(expense.description()));
            ++row;
        }
        total_->setText(QString("Entries: %1   |   Total: USD %2")
            .arg(static_cast<qulonglong>(records.size()))
            .arg(QString::fromStdString(validation::formatMoney(total))));
        empty_->setText(active_filter_.dates() || active_filter_.category()
            ? "No matching expenses. Try another filter or choose Clear filters."
            : "No expenses yet. Add your first expense below to start tracking.");
        empty_state_->setVisible(records.empty());
        empty_->setVisible(records.empty());
        feedback_->clear();
        return true;
    } catch (const std::exception& error) {
        table_->clearContents(); table_->setRowCount(0); total_->clear(); empty_->hide(); empty_state_->hide();
        showError(error); return false;
    }
}

void ExpensePage::updateDateControls() {
    const int mode = date_mode_->currentIndex();
    filter_start_->setEnabled(mode == 1 || mode == 2);
    filter_end_->setEnabled(mode == 2);
}

void ExpensePage::applyFilters() {
    try {
        std::optional<DateRange> dates;
        switch (date_mode_->currentIndex()) {
        case 1: dates = DateRange{dateText(filter_start_), dateText(filter_start_)}; break;
        case 2: dates = DateRange{dateText(filter_start_), dateText(filter_end_)}; break;
        case 3: dates = validation::weekContaining(validation::localToday()); break;
        case 4: dates = validation::monthContaining(validation::localToday()); break;
        default: break;
        }
        const auto category = validation::trimAscii(filter_category_->text().toStdString());
        std::optional<std::string> categoryFilter;
        if (!category.empty()) categoryFilter = category;
        ExpenseFilter candidate(dates, categoryFilter);
        // Construct/validate first so invalid input cannot replace the active view.
        active_filter_ = std::move(candidate);
        refresh();
    } catch (const std::exception& error) { showError(error); }
}

void ExpensePage::clearFilters() {
    date_mode_->setCurrentIndex(0);
    filter_category_->clear();
    active_filter_ = ExpenseFilter{};
    refresh();
}

std::optional<int> ExpensePage::selectedId() const {
    const auto row = table_->currentRow();
    if (row < 0 || !table_->item(row, 0)) return std::nullopt;
    return table_->item(row, 0)->data(Qt::UserRole).toInt();
}

void ExpensePage::resetForm() {
    editing_id_.reset();
    form_->setTitle("New expense");
    save_->setText("Add expense");
    amount_->clear(); category_->clear(); description_->clear();
    date_->setDate(QDate::currentDate());
}

void ExpensePage::editSelected() {
    const auto id = selectedId();
    if (!id) { feedback_->setText("Select an expense to edit."); return; }
    try {
        const auto expense = expenses_.searchExpense(*id);
        if (!expense) { feedback_->setText("That expense no longer exists. Refresh the table."); return; }
        editing_id_ = *id;
        form_->setTitle(QString("Editing expense #%1").arg(*id));
        save_->setText("Save changes");
        amount_->setText(QString::fromStdString(validation::formatMoney(expense->amount())));
        category_->setText(QString::fromStdString(expense->category()));
        date_->setDate(QDate::fromString(QString::fromStdString(expense->date()), "yyyy-MM-dd"));
        description_->setText(QString::fromStdString(expense->description()));
        feedback_->clear(); amount_->setFocus();
        scroll_->ensureWidgetVisible(form_);
    } catch (const std::exception& error) { showError(error); }
}

void ExpensePage::saveExpense() {
    try {
        const ExpenseDraft draft{validation::parseAmount(amount_->text().toStdString()),
            category_->text().toStdString(), dateText(date_), description_->text().toStdString()};
        if (async_.save) {
            Expense::validate(draft);
            setEnabled(false);
            QPointer<ExpensePage> page(this);
            async_.save(editing_id_, draft, [page](const std::string& error) {
                if (page) page->asyncFinished(error, "Expense saved. Your totals are up to date.", true);
            });
            return;
        }
        if (editing_id_) {
            if (!expenses_.update(*editing_id_, draft)) {
                feedback_->setText("That expense no longer exists. Nothing was changed."); return;
            }
        } else {
            expenses_.add(draft);
        }
        resetForm();
        if (refresh()) feedback_->setText("Expense saved in this workspace.");
        else feedback_->setText("Expense saved, but the view could not refresh: " + feedback_->text());
        if (changed_) changed_();
    } catch (const std::exception& error) { showError(error); }
}

void ExpensePage::deleteSelected() {
    const auto id = selectedId();
    if (!id) { feedback_->setText("Select an expense to delete."); return; }
    if (QMessageBox::question(this, "Delete expense",
            QString("Delete expense #%1? This cannot be undone.").arg(*id),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
        feedback_->setText("Deletion canceled. No expense was changed."); return;
    }
    try {
        if (async_.erase) {
            setEnabled(false);
            QPointer<ExpensePage> page(this);
            const bool reset = editing_id_ == id;
            async_.erase(*id, [page, reset](const std::string& error) {
                if (page) page->asyncFinished(error, "Expense deleted. Your totals are up to date.", reset);
            });
            return;
        }
        if (!expenses_.erase(*id)) {
            feedback_->setText("That expense no longer exists. Nothing was deleted."); return;
        }
        if (editing_id_ == id) resetForm();
        if (refresh()) feedback_->setText("Expense deleted.");
        else feedback_->setText("Expense deleted, but the view could not refresh: " + feedback_->text());
        if (changed_) changed_();
    } catch (const std::exception& error) { showError(error); }
}

void ExpensePage::clearSessionData() {
    resetForm();
    active_filter_ = ExpenseFilter{};
    date_mode_->setCurrentIndex(0); filter_category_->clear();
    table_->clearContents(); table_->setRowCount(0);
    total_->clear(); feedback_->clear(); empty_->setVisible(true); empty_state_->setVisible(true);
}

} // namespace expense_tracker::gui
