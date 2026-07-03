# RealSimGear panels on Linux — reverse-engineering notes

Reverse-engineering of RealSimGear flight-sim panels so they can drive
[AetherSDR](../../) on Linux. RealSimGear has ended Linux support, so
this is a from-scratch protocol map built by live capture + firmware
disassembly. All work done 2026-07-02; all units on firmware **3.2.4**.

**[PROTOCOL.md](PROTOCOL.md) is the full technical reference.** This file
is the orientation + reproduction guide.

## Hardware covered

| Panel | Banner name | USB VID:PID | Serial chip | Status |
|---|---|---|---|---|
| GCU479v2 (Perspective+ keypad) | `RealSimGear-GCU479` | `32b6:02f3` | native CDC-ACM | control map complete (live) |
| G1000 XFD BL ×2 (PFD + MFD) | `RealSimGear-G1000XFD` | `32b6:0016` | WCH CH340 | map from firmware, spot-checked |
| GMA audio panel (GMA1347) | `RealSimGear-GMA-Addon` | `32b6:000d` | WCH CH340 | control map + **LED protocol** complete (live) |

The two XFDs share a PID and carry no USB serial number; the **banner
unit-id** is the only way to tell PFD from MFD.

## What we learned (summary)

- **Transport**: USB serial, 115200 8N1, **DTR must be asserted** or the
  device stays silent. The CH340 panels use a custom VID so the `ch341`
  driver must be taught their IDs (see Setup). Discard the garbage burst
  on port open.
- **Device→host**: newline ASCII. Buttons `BTN_<NAME>=1/0`, encoders
  `ENC_<NAME>_UP/DN`, an ID banner (`\####RealSimGear#…/`) every ~3 s,
  and empty-line heartbeats at ~10 Hz.
- **Host→device**: the GCU479 and XFD are **receive-only** (backlight
  hardwired on — proven by probing + firmware dump). The **GMA panel is
  not**: it accepts a **positional LED bitmap** — a string of `'1'`/`'0'`,
  one char per annunciator LED, terminated by newline; `|` requests a
  banner and `<` resets the write cursor. 21 controllable LEDs, mapped.
  This channel is undocumented by RealSimGear; it was recovered by
  disassembling the firmware. Full byte-level grammar and the
  position→label→pin table are in [PROTOCOL.md](PROTOCOL.md).
- **Firmware**: all panels are ATmega2560 (Arduino Mega) running one
  shared codebase; device identity is selected at runtime. A Linux driver
  should key off the banner device-name and tolerate unknown messages.

## Setup (Linux)

```sh
# CDC-ACM GCU479 binds automatically. Teach ch341 the CH340 panels:
sudo modprobe ch341
echo "32b6 0016" | sudo tee /sys/bus/usb-serial/drivers/ch341-uart/new_id
echo "32b6 000d" | sudo tee /sys/bus/usb-serial/drivers/ch341-uart/new_id

# Or install the udev rules to make binding + permissions automatic:
sudo cp 99-realsimgear.rules /etc/udev/rules.d/
sudo udevadm control --reload
```

The udev rules also set `MODE=0666` on every `32b6` tty so no group
membership (`uucp`/`dialout`) is needed. Without them, add your user to
`uucp` (Arch) or `chmod` the node per session.

## Reproduce

```sh
# Watch a panel (asserts DTR, timestamps every line to capture_<port>.log):
python3 rsg_listen.py /dev/ttyACM0     # GCU479
python3 rsg_listen.py /dev/ttyUSB2     # GMA panel

# Drive GMA LEDs — bitmap of 21 chars, position = LED (see PROTOCOL.md
# table). gma_send.py appends \r\n to each argument.
python3 gma_send.py "<010000000000000000000"   # COM1 LED on (position 1)
python3 gma_send.py "<000000000000000000000"   # all off
python3 gma_sweep.py                            # walk one lit LED across all 21
```

Scripts hardcode `/dev/ttyUSB2` for the GMA (`gma_send.py`, `gma_sweep.py`)
and take the port as an argument for the listener — adjust to match your
enumeration order.

## Files

| File | What |
|---|---|
| `PROTOCOL.md` | Full protocol reference (transport, messages, LED bitmap, firmware notes) |
| `99-realsimgear.rules` | udev: ch341 binding + `0666` perms for all `32b6` devices |
| `rsg_listen.py` | Timestamped serial listener (asserts DTR); `python3 rsg_listen.py <port>` |
| `gma_send.py` | One-shot serial line sender (used to drive GMA LEDs) |
| `gma_sweep.py` | Walking-bit LED sweep for mapping GMA annunciators |
| `gma2.disasm` | AVR disassembly of the GMAADDON firmware (llvm-mc), source of the LED grammar |
| `capture-2026-07-02.log` | Raw GCU479 button/encoder capture |
| `capture-gma-addon-2026-07-02.log` | Raw GMA capture incl. LED probe traffic |
| `vocab-gcu479-3.2.4.txt` | GCU479 message vocabulary (firmware strings) |
| `vocab-g1000xfd-3.2.4.txt` | G1000 XFD message vocabulary |
| `vocab-gma350-3.2.4.txt` | Public GMA350 vocabulary (differs from our GMA-Addon) |
| `vocab-gma-addon-live-2026-07-02.txt` | GMA-Addon vocabulary as actually captured |

## Firmware sources

Public, at `https://rsgpublicfiles.s3.us-east-2.amazonaws.com/firmware/V3/RealSimGear-Arduino-3.2.4-<NAME>.ino.mega.hex`.
Names used here: `GCU479`, `G1000XFD`, `GMA350`, **`GMAADDON`** (the audio
panel — note it is *not* `GMA-Addon` or `GMA350`), `GFC500`. Decode Intel
HEX → binary, then `strings` for the vocabulary or disassemble as AVR
(`llvm-mc --disassemble --triple=avr --mcpu=atmega2560`) for logic. Feed
the whole byte stream at once and reconstruct addresses from encoding
lengths — pre-splitting into fixed-size words corrupts 2-vs-4-byte
instruction boundaries.

## Next: AetherSDR integration

A driver/daemon should: discover panels by banner, bind roles by unit-id,
normalize the GCU479 quirks (see PROTOCOL.md), and expose the GMA LED
bitmap as addressable status indicators. Natural mappings: FMS knob →
VFO tune, RANGE → span, keypad → direct frequency entry, GMA MIC/COM
selects → TX/RX slice routing with the annunciator LEDs showing active
transmit and monitored slices.
