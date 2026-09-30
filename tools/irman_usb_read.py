#!/usr/bin/env python3
"""Read IR codes from IRman-Dual in USB mode (HID, 8-byte reports).

    pip install hidapi
    python irman_usb_read.py

Report layout: code[0..5], seq, flags (bit0 = codes were dropped).
The 6 code bytes are what the serial personality sends as well.
"""
import sys
import hid

VID, PID = 0x16C0, 0x05DF          # shared obdev generic-HID ID
PRODUCT = "IRman-Dual"

def find():
    for d in hid.enumerate(VID, PID):
        if d.get("product_string") == PRODUCT:      # shared ID => match by name!
            return d["path"]
    return None

def main():
    path = find()
    if not path:
        sys.exit("IRman-Dual not found (is it plugged in by USB?)")
    dev = hid.device()
    dev.open_path(path)
    print("connected - press keys on your remote (Ctrl+C to quit)")
    last = None
    while True:
        r = dev.read(8, 1000)
        if not r:
            continue
        code, seq, flags = bytes(r[:6]), r[6], r[7]
        if seq == last:
            continue
        last = seq
        print(code.hex(" ").upper(), f"seq={seq}", "DROPPED" if flags & 1 else "")

if __name__ == "__main__":
    main()
