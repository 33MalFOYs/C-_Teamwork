#pragma once

#include <QColor>
#include <QStringList>
#include <QVector>
#include <QWidget>

namespace expense_tracker::gui {

// Charts receive integer cents from the same reports used by the exact tables.
// Floating-point values are used only to place marks on the screen.
class TrendChart final : public QWidget {
public:
    explicit TrendChart(QWidget* parent = nullptr);
    void setSeries(const QVector<qint64>& cents, const QStringList& labels);
    void clear();
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
private:
    QRectF plotRect() const;
    QVector<qint64> values_;
    QStringList labels_;
    int hovered_ = -1;
};

class CategoryChart final : public QWidget {
public:
    explicit CategoryChart(QWidget* parent = nullptr);
    void setCategories(const QStringList& names, const QVector<qint64>& cents);
    void clear();
    QSize sizeHint() const override;
protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
private:
    QStringList names_;
    QVector<qint64> values_;
    QVector<QRectF> legend_rows_;
    qint64 total_ = 0;
};

} // namespace expense_tracker::gui
