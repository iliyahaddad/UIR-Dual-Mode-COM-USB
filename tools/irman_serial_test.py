#!/usr/bin/env python3
"""Test IRman-Dual in RS-232 mode (and any real IR-Man).

    pip install pyserial
    python irman_serial_test.py COM3        # or /dev/ttyS0

DTR and RTS are driven HIGH to power the device, then the classic IR-Man
handshake is performed: host sends "IR", device answers "OK", after that
every key press produces 6 bytes.
"""
import sys, time
import serial

port = sys.argv[1] if len(sys.argv) > 1 else "COM1"
s = serial.Serial(port, 9600, bytesize=8, parity="N", stopbits=1, timeout=0.2)
s.dtr = True
s.rts = True
time.sleep(1.0)                     # power-up + mode detection (~0.3 s)
s.reset_input_buffer()
for ch in b"IR":                    # the original IR-Man wants the bytes spaced out
    s.write(bytes([ch])); time.sleep(0.05)
ans = s.read(2)
print("handshake:", ans)
if ans != b"OK":
    sys.exit("no 'OK' - check wiring, DTR/RTS power, and see hardware/HARDWARE.md")
print("press keys on the remote (Ctrl+C to quit)")
while True:
    d = s.read(6)
    if len(d) == 6:
        print(d.hex(" ").upper())
