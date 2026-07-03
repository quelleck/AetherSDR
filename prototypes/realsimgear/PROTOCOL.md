# RealSimGear Panels — Serial Protocol Notes

Reverse-engineered 2026-07-02 on Linux (CachyOS); all units on firmware
3.2.4. Verified hardware: GCU479v2, 2× G1000 XFD BL (PFD/MFD), G1000 AUD
(GMA audio panel). Raw GCU479 capture: `capture-2026-07-02.log`.
Per-device message vocabularies (from firmware string dumps):
`vocab-*.txt`.

## Transport

All devices are USB serial under VID `0x32b6` (RealSimGear Inc), but two
hardware generations exist:

| Device | PID | USB serial chip | Linux driver | Node |
|---|---|---|---|---|
| GCU479v2 | `0x02f3` | native CDC-ACM | `cdc_acm` (binds automatically) | `ttyACM*` |
| G1000 XFD BL | `0x0016` | WCH CH340, custom VID | `ch341` (needs `new_id`) | `ttyUSB*` |
| G1000 AUD (GMA) | `0x000d` | WCH CH340, custom VID | `ch341` (needs `new_id`) | `ttyUSB*` |

The CH340 identification is by descriptor fingerprint (single
vendor-specific interface FF/01/02; bulk EP2 IN/OUT 32 B + interrupt EP1
IN 8 B; bcdUSB 1.10). The stock `ch341` driver runs them fine once taught
the IDs:

```
modprobe ch341
echo "32b6 0016" > /sys/bus/usb-serial/drivers/ch341-uart/new_id
echo "32b6 000d" > /sys/bus/usb-serial/drivers/ch341-uart/new_id
```

- 115200 baud 8N1 (mandatory on the CH340 units; cosmetic over CDC-ACM).
- **DTR must be asserted** — devices are silent until the host raises DTR.
- Messages are ASCII lines, `\r\n`-terminated.
- All units can emit a garbage burst on port open (`0xff` filler, stale
  bytes, partial banner) — observed on both the CH340 panels and the
  CDC-ACM GCU479 on reopen. Discard input until the first clean newline.
- The GCU479v2 has an extra vendor-specific interface (class 0xFF/0x05) —
  presumed firmware update; untouched.
- Only the GCU479v2 shows up in `/dev/serial/by-id/` (CH340 units have no
  iSerial and identical iProduct strings) — identify CH340 units by
  banner, and tell twin XFDs apart by the banner's unit-id field.

## Device → host

### Identification banner (every ~3 s)

```
\####RealSimGear#RealSimGear-GCU479#5#3.2.4#756E6B776F04E/     (GCU479v2)
\####RealSimGear#RealSimGear-G1000XFD#1#3.2.4#756E6B776F0124/  (XFD #1)
\####RealSimGear#RealSimGear-G1000XFD#1#3.2.4#756E6B776F0189/  (XFD #2)
\####RealSimGear#RealSimGear-GMA-Addon#5#3.2.4#756E6B776F00A/  (audio panel)
```

Framed `\` … `/`, `#`-delimited fields:
`(pad)#manufacturer#device-name#hw-rev?#fw-version#unit-id`.
Use this for device discovery/identification, not the USB strings alone.
Note the audio panel banners as `GMA-Addon` while RSG's public firmware
build for it is named `GMA350` — treat both names as the same device
class. Unit-id is the only discriminator between identical panels.

### Heartbeat

Empty lines at ~10 Hz whenever awake. Treat absence (> ~1 s) as disconnect.

### Buttons

`BTN_<NAME>=1` on press, `BTN_<NAME>=0` on release.

### Encoders

`ENC_<NAME>_UP` / `ENC_<NAME>_DN`, one line per detent, no value field.

## Control maps

Per-device vocabularies extracted from the official firmware images
(`strings` over Intel-HEX-decoded binaries; see `vocab-*.txt`):

- **GCU479** — `vocab-gcu479-3.2.4.txt`, live-verified below.
- **G1000 XFD** — `vocab-g1000xfd-3.2.4.txt`: 12 softkeys
  (`BTN_SOFT_1..12`), dual NAV/COM/FMS/ALT concentric encoders with
  swap/flip keys (`BTN_NAV_FF`, `BTN_COM_FF`, `BTN_NAV_TOG`,
  `BTN_COM_TOG`), volume knobs (`BTN/ENC_NAV_VOL`, `BTN/ENC_COM_VOL`),
  AP keys (`BTN_AP`, `BTN_FD`, `BTN_HDG`, `BTN_ALT`, `BTN_VS`, `BTN_FLC`,
  `BTN_APR`, `BTN_BC`, `BTN_VNAV`, `BTN_NOSE_UP/DN`), `ENC_HDG`,
  `ENC_CRS`, `ENC_BARO`, `ENC_RANGE`, pan joystick, FMS keys (`BTN_FPL`,
  `BTN_PROC`, `BTN_MENU`, `BTN_DIRECT`, `BTN_CLR`, `BTN_ENT`),
  `BTN_REVERSION`, radio select (`BTN_NAV1/2`, `BTN_COM1/2`, `BTN_ADF`,
  `BTN_DME`, `BTN_MIC1/2`, `BTN_MKR`).
- **GMA audio panel** — `vocab-gma350-3.2.4.txt`: source/monitor keys
  (`BTN_COM1/2`, `BTN_NAV1/2`, `BTN_MIC1/2`, `BTN_TEL`, `BTN_AUX`,
  `BTN_MUS1/2`, `BTN_SPKR`, `BTN_MKR`, `BTN_MANSQ`, `BTN_PLAY`,
  `BTN_PILOT`, `BTN_COPLT`, `BTN_PASS`), `BTN_VOL`, and encoders
  `ENC_PILOT`, `ENC_PASS`, `ENC_ALT`, `ENC_HDG`.

Firmware vocabularies are supersets: a given physical panel emits only
the subset its hardware has — and possibly names *not* in the public
build. Live spot-check 2026-07-02: both XFDs matched their vocabulary
(`BTN_AP`, `ENC_NAV_VOL_*`), but the `GMA-Addon` unit is a different
build from the public GMA350 image (no public firmware found for it; S3
returns 403 for `GMA`/`GMA-Addon` names), so its map was captured live
instead — see below.

## GMA-Addon control map (live-verified, complete)

Captured 2026-07-02 (`capture-gma-addon-2026-07-02.log`,
`vocab-gma-addon-live-2026-07-02.txt`). This is the G1000-suite audio
panel (GMA1347 replica), not a GMA350:

- **Radio select**: `BTN_MIC1/2/3`, `BTN_COM1/2/3`, `BTN_COM_TOG`
  (COM 1/2 swap), `BTN_NAV1`, `BTN_NAV2`, `BTN_ADF`, `BTN_DME`,
  `BTN_AUX`, `BTN_TEL`.
- **Audio**: `BTN_SPKR`, `BTN_PA`, `BTN_MKR`, `BTN_SENS` (HI SENS),
  `BTN_MANSQ`, `BTN_PLAY`.
- **Intercom**: `BTN_PILOT`, `BTN_COPLT`; volume encoders
  `ENC_PILOT_UP/DN` (inner) and `ENC_PASS_UP/DN` (outer), knob push
  `BTN_VOL`.
- **Display backup** (red button): `BTN_REVERSION`.

Differences vs the public GMA350 vocabulary: adds `MIC3/COM3`,
`COM_TOG`, `SENS`, `DME`, `ADF`, `PA`, `REVERSION`; lacks `MUS1/2`,
`BTN_PASS`, `ENC_ALT/HDG`. Clean press/release on every key — none of
the GCU479's alias/ghosting quirks observed on this panel.

## GCU479 control map (live-verified, complete)

### Encoders (4 physical, 2 dual-concentric)

| Encoder | Messages |
|---|---|
| FMS inner | `ENC_FMS_INNER_UP/DN` |
| FMS outer | `ENC_FMS_OUTER_UP/DN` |
| CRS/BARO inner | `ENC_CRS_INNER_UP/DN` |
| CRS/BARO outer | `ENC_CRS_OUTER_UP/DN` |
| COM volume | `ENC_COM_VOL_UP/DN` |
| Range (joystick ring) | `ENC_RANGE_UP/DN` |

Knob pushes: `BTN_FMS` (FMS knob push — see quirks), `BTN_CRS_SYNC`,
`BTN_COM_VOL`, `BTN_PAN_SYNC` (joystick push).

### Joystick (pan)

`BTN_PAN_UP` / `BTN_PAN_DN` / `BTN_PAN_LEFT` / `BTN_PAN_RIGHT` + push
`BTN_PAN_SYNC`. Digital 4-way, press/release semantics.

### Function keys

`BTN_DIRECT` (Direct-To; also emits duplicate alias `BTN_DIRECT2`),
`BTN_MENU`, `BTN_FMS_SYNC`, `BTN_COM`, `BTN_CRS`, `BTN_FPL`, `BTN_PROC`,
`BTN_NAV`, `BTN_XPDR`, `BTN_CLR2`, `BTN_ENT2`, `BTN_HOME`, `BTN_COM_FLIP`,
`BTN_IDENT`.

### Alphanumeric keypad

`BTN_0` … `BTN_9`, `BTN_A` … `BTN_Z`, `BTN_PLUS_MINUS`, `BTN_PERIOD`,
`BTN_SPC`, `BTN_BKSP`, `BTN_CLR`, `BTN_ENT`.

## Quirks observed

- `BTN_DIRECT` and `BTN_DIRECT2` fire together for the single Direct-To key
  (both edges) — dedupe one of them.
- A zero-width `BTN_FMS=1`/`BTN_FMS=0` pulse accompanied presses of
  `BTN_ENT2` and `BTN_COM_FLIP` in the capture — looks like key-matrix
  ghosting in firmware. Debounce/ignore sub-millisecond BTN pulses.
- Encoders occasionally emit a single opposite-direction event when
  reversing direction (mechanical bounce). A short same-direction
  hysteresis window fixes it.

## Host → device

Two distinct cases: most panels are receive-only, but the GMA-Addon has
a real (undocumented) command channel for its annunciator LEDs.

### GCU479, G1000XFD, GMA350: receive-only

No host→device command protocol in firmware 3.2.4. Established two ways:

1. Probed 20 candidate commands live on the GCU479 (`BACKLIGHT=`,
   `SET_BACKLIGHT=`, `BKLT=`, `BL=`, `LIGHT=`, `BRIGHTNESS=`, `DIM=`,
   `LED=`, values 0/100, plus `\KEYWORD#value/` framed variants) — every
   one ignored, no ACK/error/visible change.
2. Dumped each firmware image (Intel HEX → binary, `strings`): the only
   protocol strings are the outbound `BTN_*`/`ENC_*` names and the ID
   banner. No command parser at all. Backlight is hardwired on.

### GMA-Addon: positional LED bitmap protocol (reverse-engineered)

The GMA1347 audio panel has a per-key annunciator LED above most buttons,
and the sim drives them — so this firmware **does** parse incoming bytes.
The public image is
[RealSimGear-Arduino-3.2.4-GMAADDON.ino.mega.hex](https://rsgpublicfiles.s3.us-east-2.amazonaws.com/firmware/V3/RealSimGear-Arduino-3.2.4-GMAADDON.ino.mega.hex)
(note the S3 name is `GMAADDON`, not `GMA-Addon`/`GMA350`). Decoded by
disassembly (`gma2.disasm`, via llvm-mc AVR) and confirmed live 2026-07-02.

The RX handler is a **character-level state machine**, not a line/keyword
parser. For each incoming byte:

| Byte | Action |
|---|---|
| `\|` (0x7C) | Emit an ID banner immediately (verified: forces an off-cadence banner) |
| `<` (0x3C) | Reset write cursor to position 0 |
| `\n` (0x0A) | **Latch**: for each LED position *i*, light it iff `state[i]=='1'`; then reset cursor to 0 |
| any other | `state[cursor++] = byte` (cursor capped at 100) |

So an LED frame is a **positional bitmap**: a run of `'1'`/`'0'` chars,
one per LED, in the fixed order below, terminated by newline. Only the
literal char `'1'` lights a position; `'0'` (or anything else) clears it.
`\r` before `\n` lands in an unused trailing slot and is harmless.

**Canonical usage** — always send a full-width frame so stale positions
can't linger (the state array is not auto-cleared between latches):

```
<0000000000000000000000\n     all 21 off  (leading '<' optional but safe)
<010000000000000000000\n      COM1 only
```

There are **21 controllable LEDs**, indices 0–20 (VOL and REVERSION have
no LED). The order is the firmware's button-table order; the pin column
is the Arduino-Mega pin from the LED table at RAM 0x21a:

| Pos | Label | Pin | Pos | Label | Pin |
|----|-------|-----|----|-------|-----|
| 0 | MIC1 | 44 | 11 | SENS | 36 |
| 1 | COM1 | 43 | 12 | DME | 2 |
| 2 | MIC2 | 9 | 13 | NAV1 | 39 |
| 3 | COM2 | 21 | 14 | ADF | 4 |
| 4 | MIC3 | 12 | 15 | NAV2 | 24 |
| 5 | COM3 | 19 | 16 | AUX | 66 |
| 6 | COM_TOG | 46 | 17 | MANSQ | 64 |
| 7 | TEL | 38 | 18 | PLAY | 22 |
| 8 | PA | 48 | 19 | PILOT | 60 ⚠ |
| 9 | SPKR | 40 | 20 | COPLT | 68 ⚠ |
| 10 | MKR | 6 | | | |

⚠ Positions 19–20 (PILOT/COPLT, pins 60/68 — the ATmega2560 analog-bank
pins) **flicker instead of latching solid**. They respond at the correct
index and time, but appear time-multiplexed with the button/encoder scan,
which steals the pin between LED refreshes. Usable as indicators but
expect visible flicker; a driver can't fix it (firmware-side).

Verified live: `<` + `1` + twenty `0` lit MIC1 alone; a walking-bit sweep
matched every position 0–18 solid and 19–20 flickering, all in order. The
original confusion (`BTN_MIC1=1` lighting TEL+SPKR) is fully explained —
`"BTN_MIC1=1"` has `'1'` chars at string positions 7 and 9, lighting LED
7 (TEL) and LED 9 (SPKR).

Reproduce with `gma_send.py` (one-shot line sender) and `gma_sweep.py`
(walking-bit sweep) in this directory.

Firmware update path (all panels) uses the AVR bootloader (ATmega2560),
not the application protocol.

## Firmware notes (from the 3.2.4 string dump)

- Target is an **ATmega2560** (Arduino Mega core, `.ino.mega.hex`).
- One shared firmware codebase covers the whole RSG Garmin line — device
  name strings for `G1000PFD1/MFD1/XFD/XFD1/XFD2`, `GFC500`, `GCU`, and
  `GCU479` are all present, as is the full message vocabulary of those
  panels (`ENC_ALT_*`, `ENC_HDG_*`, `ENC_NAV_*`, `ENC_COM_INNER/OUTER_*`,
  `ENC_XPDR_*`, `BTN_NAV_VOL`, `BTN_XPDR_SYNC`, …). Don't hardcode the
  GCU479 subset when writing a driver — key off the banner's device-name
  field and accept unknown `BTN_*`/`ENC_*` gracefully.
- `BTN_DIRECT` and `BTN_DIRECT2=0/1` both appear as literal strings,
  confirming the double-emit quirk is baked into firmware, as are the
  standalone `BTN_FMS=1`/`BTN_FMS=0` literals behind the ghost pulses.
