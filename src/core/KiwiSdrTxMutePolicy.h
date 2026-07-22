#pragma once

#include <algorithm>

// KiwiSDR transmit-mute latch (fork feature: warm Kiwi audio through TX).
//
// The Kiwi transmit gate should release on this client's optimistic local
// unkey (TransmitModel::setMox(false) fires immediately) instead of waiting
// out the radio's interlock round trip and hang timers — but transmissions
// this client never keyed (VOX, CAT, hardware PTT, other clients) must still
// gate on the radio-reported interlock state, which is the only signal that
// exists for them.
//
// The latch distinguishes the two: while a radio-reported TRANSMITTING state
// is only the tail of a transmission this client already ended locally,
// radioTermMasked() is true and the caller ignores the radio term in its
// mute predicate. The latch clears on the interlock's falling edge.

namespace AetherSDR {

struct KiwiSdrTxMuteLatch {
    bool localTxSeen{false};
    bool localUnkeyPending{false};

    // Feed from every TX-state signal edge, before evaluating the mute
    // predicate. localTxActive is this client's optimistic view
    // (isTransmitting() || isTuning()); radioTransmitting is the raw
    // interlock state.
    void update(bool localTxActive, bool radioTransmitting)
    {
        if (localTxActive) {
            localTxSeen = true;
            localUnkeyPending = false;
        } else if (!radioTransmitting) {
            localTxSeen = false;
            localUnkeyPending = false;
        } else if (localTxSeen) {
            localUnkeyPending = true;
        }
    }

    bool radioTermMasked() const { return localUnkeyPending; }

    // The mask rides this client's own unkey tail, but nothing in the
    // interlock stream distinguishes that tail from a foreign transmission
    // that begins before the interlock falls — so the caller must bound the
    // mask with a timeout. expire() ends the mask AND hands the rest of the
    // episode to the radio term (a still-TRANSMITTING interlock this long
    // after our unkey is someone else's transmission, which must mute).
    void expire()
    {
        localTxSeen = false;
        localUnkeyPending = false;
    }
};

// Hold time for "Resume audio after TX delay": how long the transmit gate
// stays closed past unkey so the resume lands on audio the Kiwi received
// AFTER the transmission ended, instead of replaying the operator's own
// delayed TX tail. Two additive components: the ingest lag (the
// receive-sync estimate measures when Kiwi audio ARRIVES relative to the
// Flex stream; without one, the sync base latency stands in as a proxy)
// plus the engine-applied presentation holdback for this source, which
// delays playback beyond arrival. The guard covers what neither can see
// (the Flex chain's own absolute latency plus websocket bunching).
constexpr int kKiwiSdrResumeHoldGuardMs = 250;
constexpr int kKiwiSdrResumeHoldMinMs = 250;
constexpr int kKiwiSdrResumeHoldMaxMs = 6000;

inline int kiwiSdrResumeHoldMs(bool estimateValid, int estimateOffsetMs,
                               int fallbackLatencyMs,
                               int appliedPresentationDelayMs)
{
    const int base = estimateValid ? std::max(estimateOffsetMs, 0)
                                   : std::max(fallbackLatencyMs, 0);
    return std::clamp(base + std::max(appliedPresentationDelayMs, 0)
                          + kKiwiSdrResumeHoldGuardMs,
                      kKiwiSdrResumeHoldMinMs, kKiwiSdrResumeHoldMaxMs);
}

} // namespace AetherSDR
