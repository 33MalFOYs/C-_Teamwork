#include "theme.hpp"
#include <QApplication>
#include <QFont>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPushButton>
#include <QResource>

static void initializeBrand() { Q_INIT_RESOURCE(brand); }
namespace expense_tracker::gui {
namespace {
void drawGlyph(QPainter& p, const QString& name) {
    const auto line = [&p](qreal a, qreal b, qreal c, qreal d) { p.drawLine(QPointF(a,b),QPointF(c,d)); };
    const auto poly = [&p](std::initializer_list<QPointF> points) { p.drawPolyline(QPolygonF(QVector<QPointF>(points))); };
    if (name == "dashboard") {
        for (const auto point : {QPointF(4,4),QPointF(14,4),QPointF(4,14),QPointF(14,14)})
            p.drawRoundedRect(QRectF(point,QSizeF(6,6)),1.5,1.5);
    } else if (name == "wallet") {
        p.drawRoundedRect(QRectF(3,6,18,14),3,3); poly({{4,7},{4,5},{17,3},{17,6}});
        p.drawRoundedRect(QRectF(15,11,6,5),1,1); p.drawPoint(QPointF(17.5,13.5));
    } else if (name == "chart") {
        poly({{4,4},{4,20},{21,20}}); poly({{7,15},{11,10},{15,13},{20,6}});
    } else if (name == "files" || name == "note") {
        poly({{14,3},{6,3},{6,21},{20,21},{20,9},{14,3},{14,9},{20,9}});
        line(9,13,16,13); line(9,17,15,17); if(name=="files") poly({{3,6},{3,18}});
    } else if (name == "plus" || name == "close" || name == "check") {
        if (name=="plus") { line(5,12,19,12); line(12,5,12,19); }
        else if(name=="close") { line(6,6,18,18); line(18,6,6,18); }
        else poly({{5,12},{10,17},{20,6}});
    } else if(name=="search") {
        p.drawEllipse(QRectF(3,3,13,13));line(14.5,14.5,21,21);
    } else if(name=="filter") {
        poly({{3,5},{21,5},{14,13},{14,19},{10,21},{10,13},{3,5}});
    } else if(name=="edit") {
        poly({{5,15},{16,4},{20,8},{9,19},{4,20},{5,15},{9,19}});line(14,6,18,10);
    } else if(name=="trash") {
        line(4,6,20,6);poly({{8,6},{8,3},{16,3},{16,6}});
        poly({{6,6},{7,21},{17,21},{18,6}});line(10,10,10,17);line(14,10,14,17);
    } else if(name=="refresh") {
        p.drawArc(QRectF(4,4,16,16),45*16,280*16);poly({{15,4},{20,4},{20,9}});
    } else if(name=="logout") {
        poly({{10,4},{4,4},{4,20},{10,20}});line(9,12,21,12);poly({{17,8},{21,12},{17,16}});
    } else if(name.startsWith("chevron-")) {
        if(name.endsWith("left")) poly({{15,5},{8,12},{15,19}});
        else poly({{9,5},{16,12},{9,19}});
    } else if(name=="arrow-down" || name=="arrow-up" || name=="arrow-right") {
        if(name=="arrow-down") { line(12,3,12,19);poly({{6,13},{12,19},{18,13}}); }
        else if(name=="arrow-up") { line(12,21,12,5);poly({{6,11},{12,5},{18,11}}); }
        else { line(3,12,20,12);poly({{14,6},{20,12},{14,18}}); }
    } else if(name=="calendar") {
        p.drawRoundedRect(QRectF(3,5,18,16),2,2);line(3,10,21,10);line(8,3,8,7);line(16,3,16,7);
        p.drawPoint(QPointF(8,14));p.drawPoint(QPointF(12,14));p.drawPoint(QPointF(16,14));p.drawPoint(QPointF(8,17));
    } else if(name=="receipt") {
        poly({{5,3},{8,5},{12,3},{16,5},{19,3},{19,21},{16,19},{12,21},{8,19},{5,21},{5,3}});
        line(8,9,16,9);line(8,13,14,13);
    } else if(name=="tag") {
        poly({{3,4},{12,4},{21,13},{13,21},{3,11},{3,4}});p.drawEllipse(QRectF(6,7,2,2));
    } else if(name=="food") {
        line(6,3,6,21);poly({{3,3},{3,9},{9,9},{9,3}});poly({{18,21},{18,3},{14,7},{14,12},{18,12}});
    } else if(name=="transport") {
        p.drawRoundedRect(QRectF(4,7,16,11),2,2);poly({{6,7},{8,3},{16,3},{18,7}});
        line(5,12,19,12);p.drawPoint(QPointF(6.5,15.5));p.drawPoint(QPointF(17.5,15.5));line(6,18,6,21);line(18,18,18,21);
    } else if(name=="shopping") {
        p.drawRoundedRect(QRectF(4,7,16,14),2,2);p.drawArc(QRectF(8,2,8,10),0,180*16);
    } else if(name=="home") {
        poly({{2,11},{12,3},{22,11}});poly({{5,9},{5,21},{10,21},{10,14},{14,14},{14,21},{19,21},{19,9}});
    } else if(name=="health") {
        poly({{9,3},{15,3},{15,9},{21,9},{21,15},{15,15},{15,21},{9,21},{9,15},{3,15},{3,9},{9,9},{9,3}});
    } else if(name=="education") {
        poly({{2,8},{12,3},{22,8},{12,13},{2,8}});poly({{6,10},{6,17},{12,20},{18,17},{18,10}});line(22,8,22,17);
    } else if(name=="entertainment") {
        p.drawRoundedRect(QRectF(3,7,18,12),4,4);line(6,13,12,13);line(9,10,9,16);
        p.drawPoint(QPointF(16,11));p.drawPoint(QPointF(18,15));
    } else if(name=="user") {
        p.drawEllipse(QRectF(8,3,8,8));p.drawArc(QRectF(4,14,16,14),0,180*16);
    } else if(name=="lock") {
        p.drawRoundedRect(QRectF(5,10,14,11),2,2);p.drawArc(QRectF(8,2,8,15),0,180*16);line(12,14,12,17);
    } else if(name=="shield") {
        QPainterPath path;path.moveTo(12,2);path.lineTo(21,6);path.lineTo(20,14);path.quadTo(18,20,12,23);
        path.quadTo(6,20,4,14);path.lineTo(3,6);path.closeSubpath();p.drawPath(path);poly({{7,12},{11,16},{17,9}});
    } else if(name=="eye") {
        QPainterPath path;path.moveTo(2,12);path.quadTo(12,-1,22,12);path.quadTo(12,25,2,12);p.drawPath(path);
        p.drawEllipse(QRectF(9,9,6,6));
    } else {
        p.drawEllipse(QRectF(4,4,16,16));
        p.drawPoint(QPointF(8,12));p.drawPoint(QPointF(12,12));p.drawPoint(QPointF(16,12));
    }
}
}
QIcon uiIcon(const QString& name, const QColor& color) {
    QIcon result;
    for (int size : {16,20,24,32,48,64,96}) {
        QPixmap pixmap(size,size);pixmap.fill(Qt::transparent);
        QPainter painter(&pixmap);painter.setRenderHint(QPainter::Antialiasing);
        painter.scale(size/24.0,size/24.0);
        painter.setPen(QPen(color,1.7,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);drawGlyph(painter,name);
        result.addPixmap(pixmap);
    }
    return result;
}
QLabel* iconBadge(const QString& name, const QColor& color, const QColor& background, QWidget* parent, int size) {
    auto* result=new QLabel(parent);result->setFixedSize(size,size);result->setAlignment(Qt::AlignCenter);
    result->setPixmap(uiIcon(name,color).pixmap(size/2,size/2));
    result->setStyleSheet(QString("background:%1;border:0;border-radius:%2px;").arg(background.name()).arg(size/3));
    return result;
}
void setButtonIcon(QPushButton* button, const QString& name, const QString& accessibleText) {
    const bool primary=button->property("role").toString()=="primary";
    const QColor color=primary ? QColor("#ffffff") : button->property("role").toString()=="danger" ? QColor("#d75971") : QColor("#50829c");
    button->setIcon(uiIcon(name,color));button->setIconSize(QSize(18,18));
    const QString text=accessibleText.isEmpty()?button->text():accessibleText;
    button->setAccessibleName(text);
    if(button->toolTip().isEmpty())button->setToolTip(text);
    button->setCursor(Qt::PointingHandCursor);
}
void applyTheme() {
    static bool applied=false;if(applied)return;applied=true;
    initializeBrand();qApp->setStyle("Fusion");qApp->setFont(QFont("Segoe UI",10));
    qApp->setWindowIcon(QIcon(":/brand/ex-icon.ico"));
    QPalette palette=qApp->palette();palette.setColor(QPalette::Window,QColor("#f4f7fb"));
    palette.setColor(QPalette::WindowText,QColor("#26364b"));palette.setColor(QPalette::Base,Qt::white);
    palette.setColor(QPalette::Text,QColor("#26364b"));palette.setColor(QPalette::Highlight,QColor("#dff4ff"));
    palette.setColor(QPalette::HighlightedText,QColor("#14658c"));qApp->setPalette(palette);
    qApp->setStyleSheet(R"(
        QWidget { color:#26364b; font-family:'Segoe UI'; font-size:13px; }
        QMainWindow, QWidget#loginPage, QWidget#financialWorkspace { background:#f4f7fb; }
        QLabel { background:transparent; border:0; }
        QLabel[role="title"] { font-size:26px; font-weight:700; color:#203249; }
        QLabel[role="subtitle"] { color:#5f748a; }
        QLabel[role="metric"] { font-size:29px; font-weight:700; color:#22344b; }
        QLabel[role="eyebrow"] { font-size:11px; font-weight:600; color:#5f748a; }
        QFrame[role="card"], QWidget#loginCard { background:#ffffff; border:1px solid #e7eef5; border-radius:16px; }
        QFrame#brandHeader { background:#ffffff; border:1px solid #e7eef5; border-radius:16px; }
        QFrame#budgetCard { background:qlineargradient(x1:0,y1:1,x2:1,y2:0,stop:0 #0578b1,stop:1 #12c4ef); border:0; border-radius:18px; }
        QFrame#budgetCard QLabel { color:white; }
        QFrame#budgetCard QLabel[role="subtitle"], QFrame#budgetCard QLabel[role="eyebrow"] { color:#d1f2ff; }
        QLabel#brandMark { background:transparent; }
        QLabel[role="card"] { background:#ffffff; border:1px solid #e7eef5; border-radius:10px; padding:12px; }
        QPushButton { background:#ffffff; border:1px solid #e2eaf2; border-radius:9px; padding:9px 14px; min-height:18px; font-weight:600; }
        QPushButton:hover { background:#eef8fe; border-color:#a6ddf5; }
        QPushButton:pressed { background:#dff3fe; }
        QPushButton:focus { border:2px solid #009cdb; padding:8px 13px; }
        QPushButton[role="primary"], QPushButton#signIn, QPushButton#saveExpense, QPushButton#saveBudget {
            background:#007eae; color:white; border:1px solid #007eae;
        }
        QPushButton[role="primary"]:hover, QPushButton#signIn:hover, QPushButton#saveExpense:hover, QPushButton#saveBudget:hover { background:#006c98; border-color:#006c98; }
        QPushButton[role="danger"] { color:#c6536c; background:#fff4f6; border-color:#f8dce4; }
        QPushButton[role="quiet"] { color:#568097; background:transparent; border-color:transparent; }
        QPushButton[role="quiet"]:hover { background:#eaf6fd; }
        QPushButton:checked { color:#018fc8; background:#e7f8ff; border-color:#b4e8fb; }
        QPushButton:disabled, QPushButton[role="primary"]:disabled, QPushButton#signIn:disabled, QPushButton#saveExpense:disabled, QPushButton#saveBudget:disabled { color:#8b9baa; background:#edf2f7; border-color:#e7edf4; }
        QLineEdit, QDateEdit, QComboBox { background:#fafcfe; border:1px solid #dfe8f1; border-radius:9px; padding:9px 10px; min-height:18px; selection-background-color:#d3f0ff; selection-color:#204458; }
        QLineEdit:focus, QDateEdit:focus, QComboBox:focus { background:white; border:2px solid #08aaeb; padding:8px 9px; }
        QLineEdit:disabled, QDateEdit:disabled, QComboBox:disabled { color:#a1afbd; background:#f2f5f9; }
        QComboBox::drop-down, QDateEdit::drop-down { border:none; width:22px; }
        QComboBox QAbstractItemView { background:white; selection-background-color:#e5f6fe; selection-color:#14789e; }
        QTabWidget::pane { border:0; background:transparent; }
        QTabBar::tab { color:#5f748a; background:transparent; border:0; border-radius:8px; padding:10px 20px; margin:6px 5px 8px 0; font-weight:600; }
        QTabBar::tab:selected { color:#00719b; background:#e1f5ff; }
        QTabBar::tab:hover { background:#ebf5fc; }
        QGroupBox { background:white; border:1px solid #e7eef5; border-radius:14px; margin-top:12px; padding:16px 12px 12px; font-weight:600; }
        QGroupBox::title { subcontrol-origin:margin; left:16px; padding:0 5px; color:#3a536c; }
        QTableWidget { background:white; alternate-background-color:#fafcfe; border:0; border-radius:10px; gridline-color:#edf2f7; selection-background-color:#e4f6ff; selection-color:#1f5e80; }
        QHeaderView::section { background:#ffffff; color:#5f748a; border:0; border-bottom:1px solid #eaf0f6; padding:10px 8px; font-size:11px; font-weight:600; }
        QTableWidget::item { padding:8px; border-bottom:1px solid #f0f4f8; }
        QCheckBox { spacing:8px; }
        QCheckBox::indicator { width:17px; height:17px; }
        QProgressBar { border:0; border-radius:4px; background:#edf3f8; max-height:8px; }
        QProgressBar::chunk { background:#09b0ec; border-radius:4px; }
        QProgressBar[overBudget="true"]::chunk { background:#ec7386; }
        QScrollArea { border:0; background:transparent; }
        QScrollArea > QWidget > QWidget { background:transparent; }
        QScrollBar:vertical { width:8px; background:transparent; margin:2px; }
        QScrollBar::handle:vertical { background:#cedce8; border-radius:3px; min-height:28px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height:0; }
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background:transparent; }
        QLabel#accountFeedback, QLabel#expenseFeedback, QLabel#reportFeedback, QLabel#feedback { color:#617a91; padding:4px 0; }
        QToolTip { background:#25445c; color:white; border:0; padding:8px; }
    )");
}
}
