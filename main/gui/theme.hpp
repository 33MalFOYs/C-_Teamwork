#pragma once
#include <QColor>
#include <QIcon>
#include <QString>
class QLabel;
class QPushButton;
class QWidget;
namespace expense_tracker::gui {
void applyTheme();
QIcon uiIcon(const QString& name, const QColor& color = QColor("#6d8094"));
QLabel* iconBadge(const QString& name, const QColor& color, const QColor& background,
                 QWidget* parent = nullptr, int size = 40);
void setButtonIcon(QPushButton* button, const QString& name,
                   const QString& accessibleText = QString());
}
