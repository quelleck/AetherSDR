#pragma once

// Tune-driven pan recenter write policy.
//
// While a KiwiSDR virtual antenna owns a pan's display, the radio-side pan
// geometry is deliberately frozen: zoom/center gestures stay widget-local and
// never advance PanadapterModel (#3825, #4081). That freeze makes the model's
// bandwidth stale the moment the operator zooms — so a center-ONLY write
// through RadioModel::requestPanCenter() re-broadcasts the frozen bandwidth
// via PanadapterModel::infoChanged, and the widget visibly snaps back to the
// assignment-time span mid-gesture. Every tune-driven recenter (slice-drag
// edge pan, reveal/pan-follow, center-on-slice) must therefore pick its write
// mode through this policy instead of writing through unconditionally:
//
//  - Flex-display pan → write through the radio and model; the widget
//    repaints via the optimistic model write (the pre-existing path).
//  - Kiwi-display pan → recenter the widget directly, pairing the new center
//    with the WIDGET's live bandwidth; radio and model stay untouched.
//  - Kiwi-display pan during an edge-pan slice drag → no write at all: the
//    SpectrumWidget already advanced its own view, and there is no radio-side
//    span to catch up.
//
// Radio-side recenters that exist for the radio's sake — the WFM DAX-IQ
// center (the NCO rides the pan center) and the ATU pre-tune save/restore
// (center+width written as a pair) — are NOT view recenters and stay outside
// this policy. Precedent: snapCenterLockForSlice() has applied exactly this
// split since #4116.

namespace AetherSDR::PanRecenterPolicy {

enum class Write {
    RadioAndModel,   // RadioModel::requestPanCenter(): wire command + model
    WidgetLocal,     // SpectrumWidget::setFrequencyRange(center, widget bw)
    None,            // the widget already shows the target view
};

constexpr Write recenterWrite(bool kiwiDisplayActive,
                              bool widgetOwnsViewDuringGesture)
{
    if (!kiwiDisplayActive) {
        return Write::RadioAndModel;
    }
    return widgetOwnsViewDuringGesture ? Write::None : Write::WidgetLocal;
}

// The only bandwidth a recenter may pair with its new center: the widget's
// live span when the kiwi display owns the view (the model's is frozen at
// kiwi-assignment time), the model's otherwise. An uninitialized widget span
// falls back to the model value — matching snapCenterLockForSlice (#4116).
constexpr double recenterBandwidthMhz(bool kiwiDisplayActive,
                                      double widgetBandwidthMhz,
                                      double modelBandwidthMhz)
{
    if (kiwiDisplayActive && widgetBandwidthMhz > 0.0) {
        return widgetBandwidthMhz;
    }
    return modelBandwidthMhz;
}

} // namespace AetherSDR::PanRecenterPolicy
