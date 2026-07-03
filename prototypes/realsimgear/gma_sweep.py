#!/usr/bin/env python3
"""Walk a single lit bit across all 21 GMA LED positions, ~1.8s each,
printing the firmware-predicted label so the user can flag mismatches."""
import os, time

PORT = "/dev/ttyUSB2"
N = 21
PRED = ["MIC1","COM1","MIC2","COM2","MIC3","COM3","COM_TOG","TEL","PA",
        "SPKR","MKR","SENS","DME","NAV1","ADF","NAV2","AUX","MANSQ",
        "PLAY","PILOT","COPLT"]

fd = os.open(PORT, os.O_WRONLY | os.O_NOCTTY)
def latch(bits):
    os.write(fd, ("<" + bits + "\n").encode())

latch("0"*N); time.sleep(1.0)
for i in range(N):
    bits = "0"*i + "1" + "0"*(N-1-i)
    latch(bits)
    print(f"position {i:2d}  ->  predicted:  {PRED[i]}", flush=True)
    time.sleep(1.8)
latch("0"*N)
print("sweep done, all off", flush=True)
