#include "core/KiwiSdrTxMutePolicy.h"

#include <iostream>

using AetherSDR::KiwiSdrTxMuteLatch;

namespace {

bool expect(bool condition, const char* label)
{
    std::cout << (condition ? "[ OK ] " : "[FAIL] ") << label << '\n';
    return condition;
}

// Mirrors MainWindow::kiwiSdrTransmitMuteRequired()'s use of the latch: the
// radio term is ignored while masked.
bool muteRequired(const KiwiSdrTxMuteLatch& latch, bool localTx,
                  bool radioTx)
{
    bool txActive = localTx;
    if (!txActive && radioTx && !latch.radioTermMasked()) {
        txActive = true;
    }
    return txActive;
}

bool feed(KiwiSdrTxMuteLatch& latch, bool localTx, bool radioTx)
{
    latch.update(localTx, radioTx);
    return muteRequired(latch, localTx, radioTx);
}

} // namespace

int main()
{
    bool ok = true;

    {
        // Local PTT cycle: mute on key, unmute on the optimistic local
        // unkey even though the interlock still reports TRANSMITTING.
        KiwiSdrTxMuteLatch latch;
        ok &= expect(!feed(latch, false, false), "idle: unmuted");
        ok &= expect(feed(latch, true, false),
                     "local key before interlock ack: muted");
        ok &= expect(feed(latch, true, true), "radio confirms: still muted");
        ok &= expect(!feed(latch, false, true),
                     "local unkey, interlock tail: unmuted optimistically");
        ok &= expect(!feed(latch, false, false),
                     "interlock clears: still unmuted");
    }

    {
        // Externally keyed TX (VOX/CAT/other client): no local edge exists,
        // so the radio term must keep muting until the interlock clears.
        KiwiSdrTxMuteLatch latch;
        ok &= expect(feed(latch, false, true), "external TX: muted");
        ok &= expect(feed(latch, false, true), "external TX holds: muted");
        ok &= expect(!feed(latch, false, false), "external TX ends: unmuted");
    }

    {
        // Re-key during the previous over's interlock tail re-engages the
        // mute; the stale mask must not leak into the new over.
        KiwiSdrTxMuteLatch latch;
        feed(latch, true, true);
        ok &= expect(!feed(latch, false, true), "unkey tail: unmuted");
        ok &= expect(feed(latch, true, true), "re-key during tail: muted");
        ok &= expect(!feed(latch, false, true),
                     "second unkey tail: unmuted again");
        ok &= expect(!feed(latch, false, false), "idle again: unmuted");
    }

    {
        // External TX starting immediately after our unkey tail, without an
        // interlock falling edge in between, is indistinguishable from our
        // own tail and stays unmuted until the caller's mask timeout fires
        // (MainWindow bounds the mask at 2500 ms). Once the interlock does
        // drop and rise again, the mute is honored directly.
        KiwiSdrTxMuteLatch latch;
        feed(latch, true, true);
        feed(latch, false, true);
        ok &= expect(!muteRequired(latch, false, true),
                     "external TX inside our tail window: unmuted (bounded)");
        feed(latch, false, false);
        ok &= expect(feed(latch, false, true),
                     "external TX after interlock cycle: muted");
    }

    {
        // Mask timeout: expire() hands a still-transmitting interlock back
        // to the radio term (foreign TX must mute), and a later local key
        // starts a clean episode.
        KiwiSdrTxMuteLatch latch;
        feed(latch, true, true);
        ok &= expect(!feed(latch, false, true), "unkey tail: unmuted");
        latch.expire();
        ok &= expect(muteRequired(latch, false, true),
                     "mask expired, interlock still up: muted as foreign");
        ok &= expect(feed(latch, false, true),
                     "post-expiry update keeps muting");
        ok &= expect(!feed(latch, false, false), "interlock clears: unmuted");
        ok &= expect(feed(latch, true, false), "fresh local key: muted");
        ok &= expect(!feed(latch, false, false), "fresh local unkey: unmuted");
    }

    {
        // Resume-hold computation ("Resume audio after TX delay").
        using AetherSDR::kiwiSdrResumeHoldMs;
        using AetherSDR::kKiwiSdrResumeHoldGuardMs;
        using AetherSDR::kKiwiSdrResumeHoldMinMs;
        using AetherSDR::kKiwiSdrResumeHoldMaxMs;

        ok &= expect(kiwiSdrResumeHoldMs(true, 500, 520, 0)
                         == 500 + kKiwiSdrResumeHoldGuardMs,
                     "valid estimate: offset + guard");
        ok &= expect(kiwiSdrResumeHoldMs(true, 500, 520, 520)
                         == 500 + 520 + kKiwiSdrResumeHoldGuardMs,
                     "applied presentation holdback adds to the hold");
        ok &= expect(kiwiSdrResumeHoldMs(false, 0, 520, 520)
                         == 520 + 520 + kKiwiSdrResumeHoldGuardMs,
                     "no estimate: base latency proxy + holdback + guard");
        ok &= expect(kiwiSdrResumeHoldMs(true, -300, 520, 0)
                         == kKiwiSdrResumeHoldMinMs,
                     "negative estimate clamps to minimum hold");
        ok &= expect(kiwiSdrResumeHoldMs(true, -300, 520, -100)
                         == kKiwiSdrResumeHoldMinMs,
                     "negative holdback is ignored, clamps to minimum");
        ok &= expect(kiwiSdrResumeHoldMs(true, 10000, 520, 0)
                         == kKiwiSdrResumeHoldMaxMs,
                     "huge estimate clamps to maximum hold");
        ok &= expect(kiwiSdrResumeHoldMs(true, 3000, 520, 5000)
                         == kKiwiSdrResumeHoldMaxMs,
                     "estimate + holdback together clamp to maximum");
        ok &= expect(kiwiSdrResumeHoldMs(false, 0, 0, 0)
                         == kKiwiSdrResumeHoldMinMs,
                     "zero fallback clamps to minimum hold");
    }

    if (!ok) {
        std::cout << "kiwi_sdr_tx_mute_policy_test FAILED\n";
        return 1;
    }
    std::cout << "kiwi_sdr_tx_mute_policy_test passed\n";
    return 0;
}
