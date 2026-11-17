#include "chart_widgets.hpp"
#include "expense_tracker/validation.hpp"

#include <QFontMetrics>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>
#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace expense_tracker::gui {
namespace {
const QColor ink("#20334b"), muted("#5f748a"), blue("#00adef"), line("#edf2f7");
const QColor chartColors[] = {QColor("#00adef"), QColor("#5dd5c5"), QColor("#9b8aec"), QColor("#ffc76b"), QColor("#f38d9f")};
QString exactMoney(qint64 cents) {
    return QString::fromStdString(validation::formatMoney(cents));
}
QString axisMoney(double cents) {
    const double dollars = cents / 100.0;
    if (dollars >= 1000000000.0) return "$" + QString::number(dollars / 1000000000.0, 'g', 3) + "b";
    if (dollars >= 1000000.0) return "$" + QString::number(dollars / 1000000.0, 'g', 3) + "m";
    if (dollars >= 1000.0) return "$" + QString::number(dollars / 1000.0, 'g', 3) + "k";
    return "$" + QString::number(dollars, 'f', dollars < 10 && dollars != std::floor(dollars) ? 2 : 0);
}
QFont chartFont(const QWidget* widget, int pixels, bool bold = false) {
    auto font = widget->font(); font.setPixelSize(pixels); font.setBold(bold); return font;
}
qint64 addCents(qint64 left, qint64 right) {
    // Production reports are checked for overflow before reaching a chart.
    // Keep this presentation helper well-defined for any external caller too.
    if (right > std::numeric_limits<qint64>::max() - left)
        throw std::overflow_error("Category totals exceed the supported amount range.");
    return left + right;
}
}

TrendChart::TrendChart(QWidget* parent) : QWidget(parent) {
    setMinimumSize(260, 240); setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true); setAccessibleName("Spending over time, in US dollars");
}
QSize TrendChart::sizeHint() const { return {500, 280}; }
QRectF TrendChart::plotRect() const { return QRectF(52, 25, std::max(1, width() - 72), std::max(1, height() - 63)); }
void TrendChart::setSeries(const QVector<qint64>& cents, const QStringList& labels) {
    setToolTip({}); QToolTip::hideText();
    values_.clear(); labels_.clear(); hovered_ = -1;
    const auto count = std::min(cents.size(), labels.size());
    QStringList accessible;
    for (qsizetype i = 0; i < count; ++i) {
        values_.append(std::max<qint64>(0, cents[i])); labels_.append(labels[i]);
        accessible << labels[i] + ": USD " + exactMoney(values_.last());
    }
    setAccessibleDescription(accessible.join("; ")); update();
}
void TrendChart::clear() {
    values_.clear(); labels_.clear(); hovered_ = -1; setAccessibleDescription("No spending data");
    setToolTip({}); QToolTip::hideText(); update();
}
void TrendChart::paintEvent(QPaintEvent*) {
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    const auto plot = plotRect();
    const qint64 highest = values_.isEmpty() ? 0 : *std::max_element(values_.cbegin(), values_.cend());
    const double ceiling = highest > 0 ? static_cast<double>(highest) * 1.20 : 10000.0;
    painter.setFont(chartFont(this, 10));
    for (int step = 0; step < 4; ++step) {
        const double y = plot.bottom() - plot.height() * step / 3.0;
        painter.setPen(QPen(line, 1)); painter.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        painter.setPen(muted);
        painter.drawText(QRectF(0, y - 8, plot.left() - 10, 16), Qt::AlignRight | Qt::AlignVCenter,
                         axisMoney(ceiling * step / 3.0));
    }
    painter.setPen(muted); painter.drawText(QRectF(0, 0, width(), 16), Qt::AlignLeft, "USD");
    QVector<QPointF> points;
    for (qsizetype i = 0; i < values_.size(); ++i) {
        const double x = values_.size() == 1 ? plot.center().x() : plot.left() + plot.width() * static_cast<double>(i) / static_cast<double>(values_.size() - 1);
        points.append({x, plot.bottom() - plot.height() * static_cast<double>(values_[i]) / ceiling});
        // Avoid overlapping date ticks when displaying a whole month.
        const auto stride = std::max<qsizetype>(1, (values_.size() + 6) / 7);
        if (i % stride == 0 || i == values_.size() - 1) {
            const QRectF labelRect(x - 30, plot.bottom() + 12, 60, 20);
            painter.drawText(labelRect, Qt::AlignCenter, labels_[i]);
        }
    }
    if (points.isEmpty()) {
        painter.setPen(muted); painter.setFont(chartFont(this, 12));
        painter.drawText(plot, Qt::AlignCenter, "Add an expense to see your spending"); return;
    }
    QPainterPath curve; curve.moveTo(points.first());
    for (qsizetype i = 1; i < points.size(); ++i) {
        const double midpoint = (points[i - 1].x() + points[i].x()) / 2;
        curve.cubicTo(midpoint, points[i - 1].y(), midpoint, points[i].y(), points[i].x(), points[i].y());
    }
    auto area = curve;
    area.lineTo(points.last().x(), plot.bottom()); area.lineTo(points.first().x(), plot.bottom()); area.closeSubpath();
    QLinearGradient gradient(0, plot.top(), 0, plot.bottom());
    gradient.setColorAt(0, QColor(0, 173, 239, 85)); gradient.setColorAt(1, QColor(0, 173, 239, 5));
    painter.fillPath(area, gradient); painter.setPen(QPen(blue, 2.5)); painter.drawPath(curve);
    if (highest == 0) {
        painter.setPen(muted); painter.setFont(chartFont(this, 12));
        painter.drawText(plot.adjusted(0, 0, 0, -25), Qt::AlignCenter, "No expenses in this period");
    }
    const int active = hovered_ >= 0 ? hovered_ : static_cast<int>(points.size()) - 1;
    if (active >= 0 && active < points.size()) {
        if (hovered_ >= 0) {
            QPen guide(QColor("#c1eaf9"), 1, Qt::DashLine); painter.setPen(guide);
            painter.drawLine(QPointF(points[active].x(), plot.top()), QPointF(points[active].x(), plot.bottom()));
        }
        painter.setPen(QPen(blue, 2.5)); painter.setBrush(Qt::white); painter.drawEllipse(points[active], 4.5, 4.5);
    }
}
void TrendChart::mouseMoveEvent(QMouseEvent* event) {
    if (values_.isEmpty() || !plotRect().contains(event->position())) { leaveEvent(nullptr); return; }
    const auto plot = plotRect();
    hovered_ = values_.size() == 1 ? 0 : std::clamp(static_cast<int>(std::round((event->position().x() - plot.left()) / plot.width() * static_cast<double>(values_.size() - 1))), 0, static_cast<int>(values_.size()) - 1);
    const auto detail = labels_[hovered_] + "  ·  USD " + exactMoney(values_[hovered_]);
    QToolTip::showText(event->globalPosition().toPoint(), detail, this); update();
}
void TrendChart::leaveEvent(QEvent*) { hovered_ = -1; QToolTip::hideText(); update(); }

CategoryChart::CategoryChart(QWidget* parent) : QWidget(parent) {
    setMinimumSize(220, 280); setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true); setAccessibleName("Spending by category, in US dollars");
}
QSize CategoryChart::sizeHint() const { return {360, 280}; }
void CategoryChart::setCategories(const QStringList& names, const QVector<qint64>& cents) {
    QVector<QPair<QString, qint64>> entries;
    qint64 total = 0;
    for (qsizetype i = 0; i < std::min(names.size(), cents.size()); ++i) {
        if (cents[i] <= 0) continue;
        entries.append({names[i], cents[i]}); total = addCents(total, cents[i]);
    }
    std::stable_sort(entries.begin(), entries.end(), [](const auto& left, const auto& right) { return left.second > right.second; });
    setToolTip({}); QToolTip::hideText();
    names_.clear(); values_.clear(); legend_rows_.clear(); total_ = total;
    const int individual = entries.size() > 5 ? 4 : static_cast<int>(entries.size());
    for (int i = 0; i < individual; ++i) { names_.append(entries[i].first); values_.append(entries[i].second); }
    if (entries.size() > individual) {
        qint64 remainder = 0;
        for (qsizetype i = individual; i < entries.size(); ++i) remainder = addCents(remainder, entries[i].second);
        names_.append("More categories"); values_.append(remainder);
    }
    QStringList accessible;
    for (const auto& entry : entries) accessible << entry.first + ": USD " + exactMoney(entry.second);
    setAccessibleDescription(accessible.isEmpty() ? "No spending data" : accessible.join("; ")); update();
}
void CategoryChart::clear() {
    names_.clear(); values_.clear(); legend_rows_.clear(); total_ = 0;
    setAccessibleDescription("No spending data"); setToolTip({}); QToolTip::hideText(); update();
}
void CategoryChart::paintEvent(QPaintEvent*) {
    QPainter painter(this); painter.setRenderHint(QPainter::Antialiasing);
    const bool horizontal = width() >= 440;
    const double diameter = horizontal ? std::min(180.0, height() - 36.0) : 124.0;
    const QRectF ring(horizontal ? 12 : (width() - diameter) / 2, horizontal ? (height() - diameter) / 2 : 12, diameter, diameter);
    const double thickness = horizontal ? 24 : 18;
    painter.setPen(QPen(QColor("#edf3f7"), thickness)); painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(ring.adjusted(thickness/2, thickness/2, -thickness/2, -thickness/2));
    if (total_ > 0) {
        double angle = 90;
        for (qsizetype i = 0; i < values_.size(); ++i) {
            const double span = 360.0 * static_cast<double>(values_[i]) / static_cast<double>(total_);
            painter.setPen(QPen(chartColors[i % 5], thickness, Qt::SolidLine, Qt::FlatCap));
            painter.drawArc(ring.adjusted(thickness/2, thickness/2, -thickness/2, -thickness/2),
                            static_cast<int>(std::round(angle * 16)), -static_cast<int>(std::round(span * 16)));
            angle -= span;
        }
    }
    painter.setFont(chartFont(this, 10)); painter.setPen(muted);
    painter.drawText(ring.adjusted(thickness, 20, -thickness, -diameter/2 + 10), Qt::AlignCenter, "TOTAL · USD");
    auto totalFont = chartFont(this, horizontal ? 22 : 17, true); painter.setFont(totalFont); painter.setPen(ink);
    const auto amount = exactMoney(total_);
    const auto fittedAmount = QFontMetrics(totalFont).elidedText(amount, Qt::ElideRight, static_cast<int>(diameter - thickness * 2 - 6));
    painter.drawText(QRectF(ring.left() + thickness + 3, ring.center().y() - 8, diameter - thickness*2 - 6, 30), Qt::AlignCenter, fittedAmount);
    if (values_.isEmpty()) {
        painter.setFont(chartFont(this, 12)); painter.setPen(muted);
        const QRectF emptyRect = horizontal ? QRectF(ring.right() + 30, 0, width() - ring.right() - 40, height()) : QRectF(10, ring.bottom() + 18, width() - 20, 70);
        painter.drawText(emptyRect, Qt::AlignCenter | Qt::TextWordWrap, "No expenses yet\nYour categories will appear here"); legend_rows_.clear(); return;
    }
    const double legendLeft = horizontal ? ring.right() + 28 : 6;
    const double legendTop = horizontal ? (height() - values_.size() * 30.0)/2 : ring.bottom() + 15;
    const double rowHeight = horizontal ? 30 : 24;
    const double legendWidth = width() - legendLeft - 6;
    legend_rows_.clear();
    for (qsizetype i = 0; i < values_.size(); ++i) {
        const QRectF row(legendLeft, legendTop + i*rowHeight, legendWidth, rowHeight);
        legend_rows_.append(row);
        painter.setPen(Qt::NoPen); painter.setBrush(chartColors[i % 5]); painter.drawEllipse(QPointF(row.left() + 5, row.center().y()), 4, 4);
        const auto percent = QString::number(100.0 * static_cast<double>(values_[i]) / static_cast<double>(total_), 'f', 0) + "%";
        painter.setFont(chartFont(this, 11)); painter.setPen(muted);
        painter.drawText(row.adjusted(15, 0, -38, 0), Qt::AlignVCenter | Qt::AlignLeft,
                         painter.fontMetrics().elidedText(names_[i], Qt::ElideRight, static_cast<int>(legendWidth - 57)));
        painter.setFont(chartFont(this, 11, true)); painter.setPen(ink);
        painter.drawText(QRectF(row.right() - 36, row.top(), 36, row.height()), Qt::AlignRight | Qt::AlignVCenter, percent);
    }
}
void CategoryChart::mouseMoveEvent(QMouseEvent* event) {
    for (qsizetype i = 0; i < legend_rows_.size(); ++i) {
        if (legend_rows_[i].contains(event->position())) {
            QToolTip::showText(event->globalPosition().toPoint(), names_[i] + "  ·  USD " + exactMoney(values_[i]), this); return;
        }
    }
    QToolTip::showText(event->globalPosition().toPoint(), "Total · USD " + exactMoney(total_), this);
}
void CategoryChart::leaveEvent(QEvent*) { QToolTip::hideText(); }

} // namespace expense_tracker::gui
