#!/usr/bin/env python3
"""Timestamped line capture from the GCU479 CDC-ACM port.

Port must already be configured via stty (115200 raw). Logs every line
(and raw non-UTF8 bytes if any) with monotonic-ish wall timestamps.
"""
import sys, os, time, termios

import sys
PORT = sys.argv[1]
LOG = os.path.join(os.path.dirname(os.path.abspath(__file__)), f"capture_{PORT.rsplit('/',1)[-1]}.log")

fd = os.open(PORT, os.O_RDWR | os.O_NOCTTY)

# 115200 8N1 raw, VMIN=0 VTIME=2 (200ms poll)
attrs = termios.tcgetattr(fd)
attrs[0] = 0                      # iflag
attrs[1] = 0                      # oflag
attrs[2] = termios.CS8 | termios.CREAD | termios.CLOCAL  # cflag
attrs[3] = 0                      # lflag
attrs[4] = termios.B115200        # ispeed
attrs[5] = termios.B115200        # ospeed
attrs[6][termios.VMIN] = 0
attrs[6][termios.VTIME] = 2
termios.tcsetattr(fd, termios.TCSANOW, attrs)

# Assert DTR+RTS explicitly (some firmware gates TX on DTR)
import fcntl, struct
TIOCMBIS = 0x5416
TIOCM_DTR = 0x002
TIOCM_RTS = 0x004
fcntl.ioctl(fd, TIOCMBIS, struct.pack('I', TIOCM_DTR | TIOCM_RTS))

log = open(LOG, "a", buffering=1)
log.write(f"\n=== capture start {time.strftime('%H:%M:%S')} ===\n")
print(f"listening on {PORT}, logging to {LOG}", flush=True)

buf = b""
while True:
    chunk = os.read(fd, 4096)
    if not chunk:
        continue
    buf += chunk
    while b"\n" in buf:
        line, buf = buf.split(b"\n", 1)
        line = line.rstrip(b"\r")
        ts = time.strftime("%H:%M:%S") + f".{int(time.time()*1000)%1000:03d}"
        try:
            text = line.decode("ascii")
            printable = text if text else "<empty heartbeat>"
        except UnicodeDecodeError:
            printable = "RAW " + line.hex(" ")
        log.write(f"{ts}  {printable}\n")
        print(f"{ts}  {printable}", flush=True)
