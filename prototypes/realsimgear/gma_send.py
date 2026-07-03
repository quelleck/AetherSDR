#!/usr/bin/env python3
"""One-shot GMA sender: python3 gma_send.py "MSG" ["MSG2" ...]

Each arg is sent as its own \r\n-terminated line, 0.15 s apart.
"""
import os, sys, time

fd = os.open("/dev/ttyUSB2", os.O_WRONLY | os.O_NOCTTY)
for msg in sys.argv[1:]:
    os.write(fd, (msg + "\r\n").encode())
    print(f"sent {msg!r}", flush=True)
    time.sleep(0.15)
