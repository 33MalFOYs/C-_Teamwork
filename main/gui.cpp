#include "main_window.hpp"
#include "theme.hpp"
#include "chart_widgets.hpp"
#include <QFontMetrics>
#include "expense_page.hpp"
#include "login_window.hpp"
#include "reports_page.hpp"
#include "expense_tracker/validation.hpp"
#include <QDate>
#include <QFrame>
#include <QGridLayout>
#include <QHeaderView>
#include <QPixmap>
#include <QProgressBar>
#include <QScrollArea>
#include <QTableWidget>
#include <algorithm>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPointer>
#include <QPushButton>
#include <QStackedWidget>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <exception>
#include <utility>

namespace expense_tracker::gui {

MainWindow::MainWindow(std::filesystem::path configuration, QWidget* parent)
    : QMainWindow(parent), configuration_(std::move(configuration)), controller_(this) {
    applyTheme();
    setWindowTitle("EX - Personal Expense Tracker");
    resize(560, 780); setMinimumSize(480, 600);
    pages_ = new QStackedWidget(this); setCentralWidget(pages_);
    login_ = new LoginWindow(pages_); pages_->addWidget(login_);
    controller_.setStateHandler([this](const GuiSessionState& state) { updateState(state); });
    login_->setReconnectHandler([this] { controller_.initialize(configuration_); });
    login_->setSubmitHandler([this](bool registration, std::string username,
                                     std::string password, std::string confirmation) {
        controller_.authenticate(registration, std::move(username), std::move(password),
            std::move(confirmation), [this, registration](const std::string& error) {
                if (registration && error.empty()) login_->registrationCompleted();
            });
    });
    QTimer::singleShot(0, this, [this] { controller_.initialize(configuration_); });
}

void MainWindow::discardFinancialViews() {
    if (!financial_) return;
    expenses_->clearSessionData(); reports_->clearSessionData();
    identity_->clear(); clearDashboard(); financial_feedback_->clear(); budget_amount_->clear();
    pages_->removeWidget(financial_); financial_->hide(); financial_->deleteLater();
    financial_ = nullptr; expenses_ = nullptr; reports_ = nullptr;
    financial_content_ = nullptr; identity_ = nullptr; dashboard_ = nullptr;
    financial_feedback_ = nullptr; budget_amount_ = nullptr; reload_ = nullptr;
    week_amount_ = nullptr; month_amount_ = nullptr; budget_status_ = nullptr;
    week_caption_ = nullptr; month_caption_ = nullptr; recent_ = nullptr; budget_progress_ = nullptr;
    trend_ = nullptr; categories_ = nullptr; entry_count_ = nullptr; budget_period_ = nullptr;
    displayed_user_ = 0;
}

void MainWindow::createFinancialViews() {
    const auto& state = controller_.state();
    financial_ = new QWidget(pages_); financial_->setObjectName("financialWorkspace");
    auto* layout = new QVBoxLayout(financial_);
    layout->setContentsMargins(22,18,22,10); layout->setSpacing(8);
    auto* header = new QFrame(financial_); header->setObjectName("brandHeader");
    auto* navigation = new QHBoxLayout(header); navigation->setContentsMargins(18,10,14,10); navigation->setSpacing(12);
    auto* mark = new QLabel(header);
    mark->setPixmap(QPixmap(":/brand/ex-icon.png").scaled(38,38,Qt::KeepAspectRatio,Qt::SmoothTransformation));
    mark->setAccessibleName("EX Expense Tracker"); navigation->addWidget(mark);
    auto* brand = new QLabel("EX",header); brand->setStyleSheet("font-size:22px;font-weight:800;color:#1c354b;"); navigation->addWidget(brand);
    auto* tagline = new QLabel("Personal expenses",header);tagline->setProperty("role","subtitle");navigation->addWidget(tagline);
    navigation->addStretch();
    navigation->addWidget(iconBadge("user",QColor("#0b9fcf"),QColor("#e6f7ff"),header,34));
    identity_ = new QLabel(header); identity_->setObjectName("accountIdentity");identity_->setTextFormat(Qt::PlainText);
    identity_->setMaximumWidth(160);navigation->addWidget(identity_);
    reload_ = new QPushButton(header); reload_->setObjectName("reloadAccount");reload_->setProperty("role","quiet");reload_->setFixedSize(38,38);
    setButtonIcon(reload_,"refresh","Refresh saved expenses");
    auto* logout = new QPushButton(header); logout->setObjectName("logout");logout->setProperty("role","quiet");logout->setFixedSize(38,38);
    setButtonIcon(logout,"logout","Sign out");
    navigation->addWidget(reload_);navigation->addWidget(logout);layout->addWidget(header);
    connect(logout,&QPushButton::clicked,this,[this]{controller_.logout();});
    connect(reload_,&QPushButton::clicked,this,[this]{controller_.reload();});
    auto* tabs = new QTabWidget(financial_); tabs->setObjectName("accountTabs");tabs->setIconSize(QSize(18,18));
    financial_content_ = tabs;
    auto* dashboardPage = new QWidget(tabs);dashboardPage->setObjectName("dashboardPage");
    auto* dashboardLayout = new QVBoxLayout(dashboardPage);dashboardLayout->setContentsMargins(0,8,0,0);
    auto* scroll = new QScrollArea(dashboardPage);scroll->setObjectName("dashboardScroll");
    scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget;auto* overview = new QVBoxLayout(content);
    overview->setSizeConstraint(QLayout::SetMinimumSize);overview->setContentsMargins(2,0,10,10);overview->setSpacing(18);
    auto card = [](QWidget* parent) {
        auto* frame=new QFrame(parent);frame->setProperty("role","card");
        auto* stack=new QVBoxLayout(frame);stack->setContentsMargins(18,16,18,16);stack->setSpacing(10);
        return std::make_pair(frame,stack);
    };
    auto heading = [](const QString& text,QWidget* parent) {
        auto* label=new QLabel(text,parent);label->setStyleSheet("font-size:15px;font-weight:700;color:#2b4057;");return label;
    };
    auto* welcome = new QHBoxLayout;
    auto* welcomeText = new QVBoxLayout;welcomeText->setSpacing(3);
    auto* title = new QLabel("A clear view of your spending",content);title->setProperty("role","title");title->setWordWrap(true);
    auto* subtitle = new QLabel("Your overview  /  USD",content);subtitle->setProperty("role","subtitle");
    welcomeText->addWidget(title);welcomeText->addWidget(subtitle);welcome->addLayout(welcomeText,1);
    auto* addExpense = new QPushButton("Add expense",content);addExpense->setObjectName("dashboardAddExpense");addExpense->setProperty("role","primary");
    setButtonIcon(addExpense,"plus","Add an expense");welcome->addWidget(addExpense);overview->addLayout(welcome);
    auto* metrics = new QHBoxLayout;metrics->setSpacing(14);
    auto metric = [&](const QString& text,const QString& glyph,const QColor& color,const QColor& tint,const char* name,QLabel*& value,QLabel*& caption) {
        auto [frame,stack]=card(content);
        auto* row=new QHBoxLayout;row->addWidget(iconBadge(glyph,color,tint,frame,36));
        auto* label=new QLabel(text,frame);label->setProperty("role","eyebrow");row->addWidget(label,1);stack->addLayout(row);
        value=new QLabel("--",frame);value->setObjectName(name);value->setProperty("role","metric");value->setWordWrap(true);stack->addWidget(value);
        caption=new QLabel(frame);caption->setProperty("role","subtitle");caption->setWordWrap(true);stack->addWidget(caption);metrics->addWidget(frame,1);
    };
    metric("THIS WEEK","calendar",QColor("#09a9e0"),QColor("#e6f8ff"),"weekAmount",week_amount_,week_caption_);
    metric("THIS MONTH","wallet",QColor("#2db99a"),QColor("#e9faf3"),"monthAmount",month_amount_,month_caption_);
    QLabel* countCaption=nullptr;
    metric("MONTHLY ENTRIES","receipt",QColor("#9a82dc"),QColor("#f2edfc"),"monthEntryCount",entry_count_,countCaption);
    countCaption->setText("Expenses recorded");overview->addLayout(metrics);

    auto* middle=new QHBoxLayout;middle->setSpacing(18);
    auto* planColumn=new QWidget(content);planColumn->setMinimumWidth(230);planColumn->setMaximumWidth(285);
    auto* planLayout=new QVBoxLayout(planColumn);planLayout->setContentsMargins(0,0,0,0);planLayout->setSpacing(12);
    auto* budgetCard=new QFrame(planColumn);budgetCard->setObjectName("budgetCard");budgetCard->setMinimumHeight(190);
    auto* budgetLayout=new QVBoxLayout(budgetCard);budgetLayout->setContentsMargins(20,18,20,18);budgetLayout->setSpacing(10);
    auto* planTop=new QHBoxLayout;
    auto* planName=new QLabel("MY MONTHLY PLAN",budgetCard);planName->setProperty("role","eyebrow");planTop->addWidget(planName,1);
    planTop->addWidget(iconBadge("wallet",QColor("#ffffff"),QColor("#21b9e7"),budgetCard,34));budgetLayout->addLayout(planTop);
    auto* leftLabel=new QLabel("Left to spend",budgetCard);leftLabel->setProperty("role","subtitle");budgetLayout->addWidget(leftLabel);
    dashboard_=new QLabel("--",budgetCard);dashboard_->setObjectName("dashboardSummary");dashboard_->setStyleSheet("font-size:28px;font-weight:700;color:white;");dashboard_->setWordWrap(true);budgetLayout->addWidget(dashboard_);
    budget_status_=new QLabel(budgetCard);budget_status_->setProperty("role","subtitle");budget_status_->setWordWrap(true);budgetLayout->addWidget(budget_status_);
    budget_period_=new QLabel(budgetCard);budget_period_->setProperty("role","eyebrow");budgetLayout->addWidget(budget_period_);budgetLayout->addStretch();
    planLayout->addWidget(budgetCard,1);
    auto [limitCard,limitLayout]=card(planColumn);
    auto* limitHeading=new QHBoxLayout;limitHeading->addWidget(heading("Monthly limit",limitCard));limitHeading->addStretch();
    auto* usd=new QLabel("USD",limitCard);usd->setProperty("role","eyebrow");limitHeading->addWidget(usd);limitLayout->addLayout(limitHeading);
    auto* limitRow=new QHBoxLayout;
    budget_amount_=new QLineEdit(limitCard);budget_amount_->setObjectName("budgetAmount");budget_amount_->setPlaceholderText("Set an amount");
    budget_amount_->setAccessibleName("Monthly spending limit in USD");
    budget_amount_->setToolTip("Set this month's spending limit in USD. Zero means no allowance.");
    budget_amount_->setMinimumWidth(100);limitRow->addWidget(budget_amount_,1);
    auto* saveBudget=new QPushButton(limitCard);saveBudget->setObjectName("saveBudget");saveBudget->setProperty("role","primary");saveBudget->setFixedSize(40,40);
    setButtonIcon(saveBudget,"check","Save monthly limit");limitRow->addWidget(saveBudget);limitLayout->addLayout(limitRow);
    budget_progress_=new QProgressBar(limitCard);budget_progress_->setRange(0,100);budget_progress_->setTextVisible(false);
    budget_progress_->setAccessibleName("Monthly budget used");limitLayout->addWidget(budget_progress_);planLayout->addWidget(limitCard);
    middle->addWidget(planColumn,1);
    auto [flowCard,flowLayout]=card(content);
    auto* flowHeading=new QHBoxLayout;flowHeading->addWidget(heading("Spending flow",flowCard),1);
    auto* weekPill=new QLabel("THIS WEEK",flowCard);weekPill->setProperty("role","eyebrow");flowHeading->addWidget(weekPill);flowLayout->addLayout(flowHeading);
    trend_=new TrendChart(flowCard);trend_->setObjectName("dashboardTrend");flowLayout->addWidget(trend_,1);middle->addWidget(flowCard,3);overview->addLayout(middle);

    auto* lower=new QHBoxLayout;lower->setSpacing(18);
    auto [mixCard,mixLayout]=card(content);
    auto* mixHeading=new QHBoxLayout;mixHeading->addWidget(heading("Where it went",mixCard),1);
    auto* monthPill=new QLabel("THIS MONTH",mixCard);monthPill->setProperty("role","eyebrow");mixHeading->addWidget(monthPill);mixLayout->addLayout(mixHeading);
    categories_=new CategoryChart(mixCard);categories_->setObjectName("dashboardCategories");mixLayout->addWidget(categories_,1);lower->addWidget(mixCard,1);
    auto [recentCard,recentLayout]=card(content);
    auto* recentHeading=new QHBoxLayout;recentHeading->addWidget(heading("Recent entries",recentCard),1);
    auto* viewEntries=new QPushButton("View all",recentCard);viewEntries->setObjectName("dashboardViewExpenses");viewEntries->setProperty("role","quiet");
    setButtonIcon(viewEntries,"arrow-right","View all expenses");recentHeading->addWidget(viewEntries);recentLayout->addLayout(recentHeading);
    recent_=new QTableWidget(0,3,recentCard);recent_->setObjectName("recentExpenses");
    recent_->setHorizontalHeaderLabels({"Date","Category","USD"});recent_->setEditTriggers(QAbstractItemView::NoEditTriggers);recent_->setSelectionMode(QAbstractItemView::NoSelection);
    recent_->verticalHeader()->hide();recent_->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);recent_->verticalHeader()->setDefaultSectionSize(52);
    recent_->setMinimumHeight(210);recent_->setShowGrid(false);recentLayout->addWidget(recent_,1);
    auto* shortcuts=new QHBoxLayout;
    auto* reportsShortcut=new QPushButton("Reports",recentCard);reportsShortcut->setObjectName("dashboardReports");reportsShortcut->setProperty("role","quiet");setButtonIcon(reportsShortcut,"chart","Explore reports");
    auto* filesShortcut=new QPushButton("Files",recentCard);filesShortcut->setObjectName("dashboardFiles");filesShortcut->setProperty("role","quiet");setButtonIcon(filesShortcut,"files","Import or export files");
    shortcuts->addWidget(reportsShortcut);shortcuts->addWidget(filesShortcut);shortcuts->addStretch();recentLayout->addLayout(shortcuts);
    lower->addWidget(recentCard,1);overview->addLayout(lower);overview->addStretch();
    scroll->setWidget(content);dashboardLayout->addWidget(scroll);
    connect(addExpense,&QPushButton::clicked,this,[this,tabs]{tabs->setCurrentIndex(1);auto* action=expenses_->findChild<QPushButton*>("addExpense");if(action)action->click();});
    connect(viewEntries,&QPushButton::clicked,this,[tabs]{tabs->setCurrentIndex(1);});
    connect(reportsShortcut,&QPushButton::clicked,this,[tabs]{tabs->setCurrentIndex(2);});
    connect(filesShortcut,&QPushButton::clicked,this,[tabs]{tabs->setCurrentIndex(3);});
    connect(saveBudget,&QPushButton::clicked,this,[this]{saveCurrentBudget();});
    connect(budget_amount_,&QLineEdit::returnPressed,this,[this]{saveCurrentBudget();});

    expenses_ = new ExpensePage(controller_.workspace(), tabs);
    expenses_->setAsyncHandlers({
        [this](std::optional<int> id, ExpenseDraft draft, ExpensePage::Completion done) { controller_.save(id,std::move(draft),std::move(done)); },
        [this](int id, ExpensePage::Completion done) { controller_.erase(id,std::move(done)); },
        [this](ExpensePage::Completion done) { controller_.reload(std::move(done)); }
    });
    reports_ = new ReportsPage(controller_.workspace(), tabs);
    expenses_->setChangedHandler([this] { refreshDashboard(); if (reports_) reports_->refresh(); });
    tabs->addTab(dashboardPage,uiIcon("dashboard"),"Overview");tabs->addTab(expenses_,uiIcon("receipt"),"Expenses");tabs->addTab(reports_,uiIcon("chart"),"Reports");
    for(int i=0;i<3;++i)tabs->setTabToolTip(i,tabs->tabText(i));
    connect(tabs,&QTabWidget::currentChanged,this,[this](int) { if(controller_.state().cache_ready){refreshDashboard();if(reports_)reports_->refresh();} });

    auto* filesPage=new QWidget(tabs);filesPage->setObjectName("filesPage");
    auto* filesOuter=new QVBoxLayout(filesPage);filesOuter->setContentsMargins(0,8,0,0);
    auto* filesScroll=new QScrollArea(filesPage);filesScroll->setObjectName("filesScroll");filesScroll->setWidgetResizable(true);
    filesScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* filesContent=new QWidget;auto* filesLayout=new QVBoxLayout(filesContent);filesLayout->setSizeConstraint(QLayout::SetMinimumSize);
    filesLayout->setContentsMargins(4,0,12,10);filesLayout->setSpacing(18);
    auto* fileHeading=new QHBoxLayout;fileHeading->addWidget(iconBadge("files",QColor("#927add"),QColor("#f0ebfc"),filesContent,52));
    auto* fileWords=new QVBoxLayout;
    auto* filesTitle=new QLabel("Your data, to go",filesContent);filesTitle->setProperty("role","title");fileWords->addWidget(filesTitle);
    auto* explanation=new QLabel("Bring expenses in. Take a copy with you.",filesContent);explanation->setProperty("role","subtitle");fileWords->addWidget(explanation);
    fileHeading->addLayout(fileWords,1);filesLayout->addLayout(fileHeading);
    auto* fileGrid=new QGridLayout;fileGrid->setHorizontalSpacing(18);fileGrid->setVerticalSpacing(18);
    const QStringList labels{"Import expenses","Export everything","A week in review","Your selection"};
    const QStringList actionLabels{"Choose CSV","Save CSV","Choose a week","Save selection"};
    const QStringList glyphs{"arrow-down","arrow-up","calendar","filter"};
    const QStringList descriptions{
        "Add expenses from an EX-format CSV file.",
        "Download every expense saved in this account.",
        "Daily totals and categories in one weekly report.",
        "Export the rows matching your Expenses filters."
    };
    const QStringList details{
        "Each import adds new rows. Reimporting creates duplicates.",
        "A CSV copy you can open in a spreadsheet.",
        "Choose any date within the week you want.",
        "Set your filters on the Expenses page first."
    };
    const QColor colors[]{QColor("#00a8df"),QColor("#32b69a"),QColor("#a18add"),QColor("#db9b39")};
    const QColor tints[]{QColor("#e5f8ff"),QColor("#e9faf4"),QColor("#f2edfc"),QColor("#fff6e7")};
    for(int action=0;action<4;++action) {
        auto [frame,stack]=card(filesContent);frame->setMinimumHeight(240);
        auto* top=new QHBoxLayout;top->addWidget(iconBadge(glyphs[action],colors[action],tints[action],frame,54));top->addStretch();
        auto* type=new QLabel(action==2?"REPORT":"CSV",frame);type->setProperty("role","eyebrow");top->addWidget(type);stack->addLayout(top);
        stack->addWidget(heading(labels[action],frame));
        auto* description=new QLabel(descriptions[action],frame);description->setWordWrap(true);description->setProperty("role","subtitle");stack->addWidget(description);
        auto* detail=new QLabel(details[action],frame);detail->setWordWrap(true);detail->setStyleSheet(action==0?"color:#aa7c35;font-size:11px;":"color:#8a9aac;font-size:11px;");stack->addWidget(detail);stack->addStretch();
        auto* button=new QPushButton(actionLabels[action],frame);button->setObjectName(QString("csvAction%1").arg(action));if(action==0)button->setProperty("role","primary");
        setButtonIcon(button,glyphs[action],labels[action]);stack->addWidget(button);
        fileGrid->addWidget(frame,action/2,action%2);
        connect(button,&QPushButton::clicked,this,[this,action] {
            try {
                QString date;
                if(action==2) { bool ok=false;date=QInputDialog::getText(this,"Weekly export","Date in week (YYYY-MM-DD)",QLineEdit::Normal,QDate::currentDate().toString("yyyy-MM-dd"),&ok);if(!ok)return; }
                const auto path=action==0?QFileDialog::getOpenFileName(this,"Import CSV",{},"CSV (*.csv)"):QFileDialog::getSaveFileName(this,"Export CSV",{},"CSV (*.csv)",nullptr,QFileDialog::DontConfirmOverwrite);
                if(path.isEmpty())return;
                const auto file=std::filesystem::u8path(path.toStdString());bool overwrite=false;
                if(action==0 && QMessageBox::question(this,"Import new records","Every imported row receives a new ID. Continue?")!=QMessageBox::Yes)return;
                if(action!=0 && std::filesystem::exists(file)){overwrite=QMessageBox::question(this,"Overwrite file","Replace the existing file?")==QMessageBox::Yes;if(!overwrite)return;}
                controller_.csv(action,file,date.toStdString(),overwrite,[this](const std::string& error){if(financial_feedback_ && error.empty())financial_feedback_->setText("CSV operation completed.");},expenses_->activeFilter());
            }catch(const std::exception& error){if(financial_feedback_)financial_feedback_->setText(QString::fromUtf8(error.what()));}
        });
    }
    fileGrid->setColumnStretch(0,1);fileGrid->setColumnStretch(1,1);filesLayout->addLayout(fileGrid);
    auto* formatToggle=new QPushButton("CSV format guide",filesContent);formatToggle->setProperty("role","quiet");setButtonIcon(formatToggle,"files","Show or hide the CSV format guide");filesLayout->addWidget(formatToggle,0,Qt::AlignLeft);
    auto* format=new QLabel("Required columns: expense_id, amount, category, expense_date, description\nUse positive USD amounts and dates as YYYY-MM-DD. Leave expense_id empty to create new records.",filesContent);
    format->setTextFormat(Qt::PlainText);format->setWordWrap(true);format->setProperty("role","card");format->hide();filesLayout->addWidget(format);
    connect(formatToggle,&QPushButton::clicked,this,[format]{format->setVisible(format->isHidden());});
    filesLayout->addStretch();filesScroll->setWidget(filesContent);filesOuter->addWidget(filesScroll);
    tabs->addTab(filesPage,uiIcon("files"),"Files");tabs->setTabToolTip(3,"Import and export CSV files");
    layout->addWidget(tabs,1);
    financial_feedback_=new QLabel(financial_);financial_feedback_->setObjectName("accountFeedback");
    financial_feedback_->setWordWrap(true);financial_feedback_->setTextFormat(Qt::PlainText);layout->addWidget(financial_feedback_);
    pages_->addWidget(financial_);displayed_user_=state.user->id;
    setMinimumSize(850,720);resize(1200,860);
}

void MainWindow::updateState(const GuiSessionState& state) {
    login_->setBusy(state.busy); login_->setConnectionReady(state.connected);
    if (!state.user) {
        const bool wasSignedIn = financial_ != nullptr;
        discardFinancialViews(); pages_->setCurrentWidget(login_);
        if (wasSignedIn) { login_->clearCredentials(); setMinimumSize(480, 600); resize(560, 720); }
        if (!state.error.empty()) login_->showFeedback(QString::fromUtf8(state.error.c_str()), state.retry_milliseconds);
        else if (state.busy) login_->showFeedback("Connecting to your account...");
        else if (state.connected) login_->showFeedback("Ready when you are. Sign in or create an account.");
        return;
    }
    if (financial_ && displayed_user_ != state.user->id) discardFinancialViews();
    if (!financial_ && state.cache_ready) createFinancialViews();
    if (!financial_) {
        login_->showFeedback(QString::fromUtf8(state.error.c_str()));
        return;
    }
    pages_->setCurrentWidget(financial_);
    const auto username=QString::fromStdString(state.user->username);
    identity_->setText(identity_->fontMetrics().elidedText(username,Qt::ElideRight,160));identity_->setToolTip(username);
    financial_content_->setEnabled(!state.busy && state.cache_ready);
    reload_->setEnabled(!state.busy);
    financial_feedback_->setText(state.busy ? "Saving and updating your view..." : QString::fromUtf8(state.error.c_str()));
    if (state.cache_ready && !state.busy) {
        expenses_->refresh(); reports_->refresh(); refreshDashboard();
    } else if (!state.cache_ready) {
        expenses_->refresh(); reports_->clearSessionData();
        clearDashboard();
        dashboard_->setText("Unavailable");
        budget_status_->setText("Refresh to load your latest data.");
    }
}

void MainWindow::clearDashboard() {
    for(auto* label:{dashboard_,week_amount_,month_amount_,budget_status_,week_caption_,month_caption_,entry_count_,budget_period_})if(label)label->clear();
    if(recent_){recent_->clearContents();recent_->setRowCount(0);}
    if(budget_progress_){budget_progress_->setValue(0);budget_progress_->hide();}
    if(trend_)trend_->clear();
    if(categories_)categories_->clear();
}
void MainWindow::refreshDashboard() {
    if(!dashboard_ || !controller_.state().cache_ready)return;
    try {
        const auto today=validation::localToday();
        const auto week=controller_.workspace().weekly(today);
        const auto& state=controller_.state();
        const auto month=controller_.workspace().monthly(state.current_month);
        auto money=[](Money value){return "USD "+QString::fromStdString(validation::formatMoney(value));};
        week_amount_->setText(money(week.total));month_amount_->setText(money(month.total));
        entry_count_->setText(QString::number(static_cast<qulonglong>(month.count)));
        const auto start=QDate::fromString(QString::fromStdString(week.dates.start),"yyyy-MM-dd");
        const auto end=QDate::fromString(QString::fromStdString(week.dates.end),"yyyy-MM-dd");
        week_caption_->setText(start.toString("MMM d")+" - "+end.toString("MMM d"));
        month_caption_->setText(QDate(month.period.year,month.period.month,1).toString("MMMM yyyy"));
        budget_period_->setText(month_caption_->text().toUpper());
        QVector<qint64> days;QStringList dates;
        for(const auto& day:week.days){days.append(day.total);dates.append(QDate::fromString(QString::fromStdString(day.date),"yyyy-MM-dd").toString("ddd"));}
        trend_->setSeries(days,dates);
        QStringList categoryNames;QVector<qint64> amounts;
        for(const auto& category:month.categories){categoryNames.append(QString::fromStdString(category.first));amounts.append(category.second);}
        categories_->setCategories(categoryNames,amounts);
        budget_progress_->setVisible(state.current_month_budget.has_value());
        if(state.current_month_budget) {
            const auto budget=*state.current_month_budget;const auto remaining=budget-month.total;
            dashboard_->setText(money(remaining));
            budget_status_->setText(remaining<0?"Over limit by "+money(-remaining):"Of your "+money(budget)+" limit");
            const int percent=budget>0?(month.total>=budget?100:static_cast<int>(static_cast<long double>(month.total)*100/budget)):(month.total>0?100:0);
            budget_progress_->setValue(percent);
            budget_progress_->setStyleSheet(remaining<0?"QProgressBar::chunk {background:#ed7f92;}":"QProgressBar::chunk {background:#09b0ec;}");
            budget_progress_->setToolTip(month.total>budget?"Your spending is over this month's limit.":QString("%1% of your monthly limit used").arg(percent));
            budget_amount_->setPlaceholderText(QString::fromStdString(validation::formatMoney(budget)));
        } else {
            dashboard_->setText("Not set");budget_status_->setText("Add a limit below to start your plan.");
            budget_amount_->setPlaceholderText("Set an amount");
        }
        auto rows=controller_.workspace().matching(ExpenseFilter{});
        const auto count=std::min<std::size_t>(rows.size(),3);recent_->setRowCount(static_cast<int>(count));
        for(std::size_t index=0;index<count;++index) {
            const auto& expense=rows[rows.size()-1-index];
            const auto date=QDate::fromString(QString::fromStdString(expense.date()),"yyyy-MM-dd");
            auto* dateItem=new QTableWidgetItem(date.toString("MMM d"));dateItem->setToolTip(date.toString("yyyy-MM-dd"));recent_->setItem(static_cast<int>(index),0,dateItem);
            auto* category=new QTableWidgetItem(uiIcon("receipt",QColor("#10ace2")),QString::fromStdString(expense.category()));
            category->setToolTip(QString::fromStdString(expense.description()));recent_->setItem(static_cast<int>(index),1,category);
            auto* amount=new QTableWidgetItem(QString::fromStdString(validation::formatMoney(expense.amount())));amount->setTextAlignment(Qt::AlignRight|Qt::AlignVCenter);recent_->setItem(static_cast<int>(index),2,amount);
        }
        if(rows.empty()){recent_->setRowCount(1);recent_->setItem(0,1,new QTableWidgetItem("No expenses yet"));}
    }catch(const std::exception& error){clearDashboard();budget_status_->setText(QString::fromUtf8(error.what()));}
}

void MainWindow::saveCurrentBudget() {
    if (!controller_.state().cache_ready || controller_.state().busy) return;
    try {
        const auto amount = validation::parseAmount(budget_amount_->text().toStdString(), true);
        QPointer<MainWindow> window(this);
        controller_.setBudget(controller_.state().current_month, amount, [window](const std::string& error) {
            if (!window || !window->financial_feedback_) return;
            if (error.empty()) {
                window->budget_amount_->clear(); window->financial_feedback_->setText("Monthly limit saved. Your plan is up to date.");
            }
        });
    } catch (const std::exception& error) { financial_feedback_->setText(QString::fromUtf8(error.what())); }
}

} // namespace expense_tracker::gui
