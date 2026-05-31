#pragma once

#include <QString>
#include <QWidget>

namespace AetherSDR {

// Reusable Qt double-ended slider with two draggable handles bounded
// to min ≤ low ≤ high ≤ max.  Renders an in-widget dim `label` prefix
// in its left margin so the toolbar layout does not need a separate
// QLabel.  Keyboard-navigable (Left/Right/Up/Down move the focused
// handle; Tab/Backtab switch between Low and High) and exposes an
// accessible name derived from `label` so the in-flight accessibility
// CI (#3289) accepts it.  Used by the CW toolbar for pitch (Hz) and
// WPM ranges; intended to be reusable wherever a bounded range needs
// to be set interactively (filter passband, scan range, AGC, …).
//
// API mirrors the issue spec (#3331):
//   RangeSlider(min, max, low, high, label, unit, parent)
//   signal:  rangeChanged(int low, int high)
//   slots :  setLow(int), setHigh(int), setRange(int low, int high)
//
// The widget has no external dependencies beyond Qt6 core/gui/widgets.

class RangeSlider : public QWidget {
    Q_OBJECT

public:
    RangeSlider(int min, int max, int low, int high,
                const QString& label, const QString& unit,
                QWidget* parent = nullptr);

    int low() const { return m_low; }
    int high() const { return m_high; }
    int minimum() const { return m_min; }
    int maximum() const { return m_max; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

public slots:
    void setLow(int v);
    void setHigh(int v);
    void setRange(int low, int high);

signals:
    void rangeChanged(int low, int high);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void keyPressEvent(QKeyEvent* ev) override;
    void focusInEvent(QFocusEvent* ev) override;
    void focusOutEvent(QFocusEvent* ev) override;

private:
    enum class Handle { None, Low, High };

    int  trackX0() const;
    int  trackX1() const;
    int  valueToX(int v) const;
    int  xToValue(int x) const;
    Handle hitTest(const QPoint& p) const;
    void emitChanged();

    int m_min;
    int m_max;
    int m_low;
    int m_high;
    QString m_label;
    QString m_unit;

    Handle m_dragging{Handle::None};
    Handle m_focused{Handle::Low};

    int m_lastEmittedLow{INT_MIN};
    int m_lastEmittedHigh{INT_MIN};
};

} // namespace AetherSDR
