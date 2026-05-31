#include "RangeSlider.h"

#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QFocusEvent>

#include <algorithm>
#include <climits>

namespace AetherSDR {

namespace {
constexpr int kHandleRadius      = 5;
constexpr int kTrackHeight       = 4;
constexpr int kLabelGap          = 4;
constexpr int kValueGap          = 4;
constexpr int kKeyboardStepSmall = 1;
constexpr int kKeyboardStepLarge = 10;
}

RangeSlider::RangeSlider(int min, int max, int low, int high,
                         const QString& label, const QString& unit,
                         QWidget* parent)
    : QWidget(parent)
    , m_min(min)
    , m_max(std::max(min + 1, max))
    , m_low(std::clamp(low, min, m_max))
    , m_high(std::clamp(high, m_low, m_max))
    , m_label(label)
    , m_unit(unit)
{
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    setMouseTracking(false);
    setAccessibleName(label.isEmpty()
        ? QStringLiteral("Range slider")
        : QStringLiteral("%1 range").arg(label));
    setAccessibleDescription(
        QStringLiteral("Use Tab to switch between low and high handle; "
                       "Left/Right or Up/Down to adjust the focused handle."));
    setToolTip(QStringLiteral("%1: %2 - %3 %4").arg(label)
        .arg(m_low).arg(m_high).arg(unit));
}

QSize RangeSlider::sizeHint() const
{
    QFontMetrics fm(font());
    const int labelW = m_label.isEmpty() ? 0 : fm.horizontalAdvance(m_label) + kLabelGap;
    const QString valueTemplate = QStringLiteral("9999-9999 %1").arg(m_unit);
    const int valueW = fm.horizontalAdvance(valueTemplate) + kValueGap;
    return {labelW + 120 + valueW, std::max(fm.height() + 4, 18)};
}

QSize RangeSlider::minimumSizeHint() const
{
    QFontMetrics fm(font());
    const int labelW = m_label.isEmpty() ? 0 : fm.horizontalAdvance(m_label) + kLabelGap;
    return {labelW + 60, fm.height() + 4};
}

int RangeSlider::trackX0() const
{
    QFontMetrics fm(font());
    const int labelW = m_label.isEmpty() ? 0 : fm.horizontalAdvance(m_label) + kLabelGap;
    return labelW + kHandleRadius;
}

int RangeSlider::trackX1() const
{
    QFontMetrics fm(font());
    const QString valueTemplate = QStringLiteral(" %1-%2 %3")
        .arg(m_min).arg(m_max).arg(m_unit);
    const int valueW = fm.horizontalAdvance(valueTemplate) + kValueGap;
    return std::max(trackX0() + 20, width() - valueW - kHandleRadius);
}

int RangeSlider::valueToX(int v) const
{
    const int x0 = trackX0();
    const int x1 = trackX1();
    if (m_max <= m_min) return x0;
    const double t = double(v - m_min) / double(m_max - m_min);
    return x0 + int(std::round(t * (x1 - x0)));
}

int RangeSlider::xToValue(int x) const
{
    const int x0 = trackX0();
    const int x1 = trackX1();
    if (x1 <= x0) return m_min;
    const double t = std::clamp(double(x - x0) / double(x1 - x0), 0.0, 1.0);
    return m_min + int(std::round(t * (m_max - m_min)));
}

RangeSlider::Handle RangeSlider::hitTest(const QPoint& p) const
{
    const int lowX  = valueToX(m_low);
    const int highX = valueToX(m_high);
    const int dLow  = std::abs(p.x() - lowX);
    const int dHigh = std::abs(p.x() - highX);
    if (lowX == highX) {
        return (p.x() < lowX) ? Handle::Low : Handle::High;
    }
    return (dLow <= dHigh) ? Handle::Low : Handle::High;
}

void RangeSlider::setLow(int v)
{
    v = std::clamp(v, m_min, m_high);
    if (v == m_low) return;
    m_low = v;
    update();
    emitChanged();
}

void RangeSlider::setHigh(int v)
{
    v = std::clamp(v, m_low, m_max);
    if (v == m_high) return;
    m_high = v;
    update();
    emitChanged();
}

void RangeSlider::setRange(int low, int high)
{
    low  = std::clamp(low,  m_min, m_max);
    high = std::clamp(high, m_min, m_max);
    if (low > high) std::swap(low, high);
    if (low == m_low && high == m_high) return;
    m_low  = low;
    m_high = high;
    update();
    emitChanged();
}

void RangeSlider::emitChanged()
{
    setToolTip(QStringLiteral("%1: %2 - %3 %4")
        .arg(m_label).arg(m_low).arg(m_high).arg(m_unit));
    if (m_low == m_lastEmittedLow && m_high == m_lastEmittedHigh) return;
    m_lastEmittedLow  = m_low;
    m_lastEmittedHigh = m_high;
    emit rangeChanged(m_low, m_high);
}

void RangeSlider::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);

    const QColor cLabel(0x80, 0x90, 0xa0);
    const QColor cTrackBg(0x20, 0x30, 0x40);
    const QColor cTrackFg(0x40, 0xa0, 0xc0);
    const QColor cHandle(0xc8, 0xd8, 0xe8);
    const QColor cHandleFocus(0x40, 0xc0, 0xff);

    QFontMetrics fm(font());
    const int midY = height() / 2;

    if (!m_label.isEmpty()) {
        p.setPen(cLabel);
        p.drawText(0, (height() + fm.ascent() - fm.descent()) / 2, m_label);
    }

    const int x0    = trackX0();
    const int x1    = trackX1();
    const int lowX  = valueToX(m_low);
    const int highX = valueToX(m_high);
    const int trackY = midY - kTrackHeight / 2;

    p.setPen(Qt::NoPen);
    p.setBrush(cTrackBg);
    p.drawRoundedRect(QRect(x0, trackY, x1 - x0, kTrackHeight),
                      kTrackHeight / 2.0, kTrackHeight / 2.0);

    p.setBrush(cTrackFg);
    p.drawRoundedRect(QRect(lowX, trackY, std::max(1, highX - lowX), kTrackHeight),
                      kTrackHeight / 2.0, kTrackHeight / 2.0);

    auto drawHandle = [&](int hx, bool focused) {
        const QColor& fill = focused ? cHandleFocus : cHandle;
        p.setBrush(fill);
        p.setPen(focused ? QPen(cHandleFocus.lighter(140), 1.5) : QPen(Qt::NoPen));
        p.drawEllipse(QPoint(hx, midY), kHandleRadius, kHandleRadius);
        if (focused) {
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(cHandleFocus, 1));
            p.drawEllipse(QPoint(hx, midY), kHandleRadius + 2, kHandleRadius + 2);
        }
    };

    const bool focused = hasFocus();
    drawHandle(lowX,  focused && m_focused == Handle::Low);
    drawHandle(highX, focused && m_focused == Handle::High);

    p.setPen(cLabel);
    const QString valueText = QStringLiteral("%1-%2 %3")
        .arg(m_low).arg(m_high).arg(m_unit);
    p.drawText(x1 + kHandleRadius + kValueGap,
               (height() + fm.ascent() - fm.descent()) / 2, valueText);
}

void RangeSlider::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(ev);
        return;
    }
    setFocus(Qt::MouseFocusReason);
    m_dragging = hitTest(ev->pos());
    m_focused = m_dragging;
    const int v = xToValue(ev->pos().x());
    if (m_dragging == Handle::Low)  setLow(v);
    else                            setHigh(v);
    update();
    ev->accept();
}

void RangeSlider::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_dragging == Handle::None) return;
    const int v = xToValue(ev->pos().x());
    if (m_dragging == Handle::Low)  setLow(v);
    else                            setHigh(v);
}

void RangeSlider::mouseReleaseEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton) m_dragging = Handle::None;
}

void RangeSlider::keyPressEvent(QKeyEvent* ev)
{
    const int step = (ev->modifiers() & Qt::ShiftModifier)
        ? kKeyboardStepLarge : kKeyboardStepSmall;
    switch (ev->key()) {
    case Qt::Key_Tab:
    case Qt::Key_Backtab:
        m_focused = (m_focused == Handle::Low) ? Handle::High : Handle::Low;
        update();
        ev->accept();
        return;
    case Qt::Key_Left:
    case Qt::Key_Down:
        if (m_focused == Handle::Low) setLow(m_low - step);
        else                          setHigh(m_high - step);
        ev->accept();
        return;
    case Qt::Key_Right:
    case Qt::Key_Up:
        if (m_focused == Handle::Low) setLow(m_low + step);
        else                          setHigh(m_high + step);
        ev->accept();
        return;
    case Qt::Key_Home:
        if (m_focused == Handle::Low) setLow(m_min);
        else                          setHigh(m_low);
        ev->accept();
        return;
    case Qt::Key_End:
        if (m_focused == Handle::Low) setLow(m_high);
        else                          setHigh(m_max);
        ev->accept();
        return;
    default:
        QWidget::keyPressEvent(ev);
    }
}

void RangeSlider::focusInEvent(QFocusEvent* ev)
{
    QWidget::focusInEvent(ev);
    update();
}

void RangeSlider::focusOutEvent(QFocusEvent* ev)
{
    QWidget::focusOutEvent(ev);
    update();
}

} // namespace AetherSDR
