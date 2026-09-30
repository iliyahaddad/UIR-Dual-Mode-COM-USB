# IRman-Dual

**One IR receiver, one microcontroller, two personalities: a classic serial *IR-Man* on a COM port, and a USB HID device - chosen automatically at power-up.**

```
COM port (DTR/RTS powered) ─┐                     ┌─► "IR" → "OK" → 6 bytes per key @ 9600 8N1  (IR-Man compatible)
                            ├─► ATtiny85 + IR ────┤
USB (bus powered)  ─────────┘   (auto-detect)     └─► HID device, 8-byte input reports, V-USB (software USB)
```

* Same MCU pins are used for **USB D-/D+ and RS-232 TX/RX**, and the supply comes from USB VBUS *or* DTR/RTS.
* No jumper, no switch: the firmware measures the voltage on the serial-only supply node and picks the mode.
* Descended from the PIC16F84 "IR-Man clone" (1999) and Igor Češko's software-USB IgorPlug-USB; uses Objective Development's V-USB.
  See [docs/HISTORY.md](docs/HISTORY.md).

> **Status - please read.** The IR fingerprint algorithm is unit-tested on a PC (`tools/test_ir_hash.c`: 0 unstable codes in
> 50 000 jittered NEC/Sony/RC-5 frames, 0 collisions). The firmware was syntax-checked but **not yet compiled with avr-gcc or run on silicon** by the author of this text,
> and the USB/serial timing needs a scope check on your board (see the bring-up checklist in [hardware/HARDWARE.md](hardware/HARDWARE.md)).
> Treat v0.1 as a reviewed, carefully derived starting point, not a certified product.

## Pin map (ATtiny85, DIP-8)

| Pin | Name | USB mode | RS-232 mode |
|---|---|---|---|
| 1 | PB5 | /RESET (ISP) | /RESET (ISP) |
| 2 | PB3 / ADC3 | - | **mode detect**: divider from the DTR/RTS node (47k / 22k) |
| 3 | PB4 | switched 1.5 k pull-up for D- | unused (hi-Z) |
| 4 | GND | GND | GND |
| 5 | PB0 | **D-** | **TX** → PC RXD (inverted TTL) |
| 6 | PB1 | IR receiver output | IR receiver output |
| 7 | PB2 (INT0) | **D+** | **RX** ← PC TXD (via 100 k, inverted) |
| 8 | VCC | ~4.7 V from VBUS | ~4.7 V from DTR/RTS via LP2950-5.0 |

The four "external" lines of your request map exactly like this: **GND**, **power (VBUS ↔ DTR/RTS)**, **D-/TXD-out**, **D+/RXD-in**.
Full schematic/netlist/BOM: [hardware/HARDWARE.md](hardware/HARDWARE.md).

## How the mode is chosen

After a 100 ms settling delay the firmware reads the voltage of **VSER** (the node fed only by the two DTR/RTS diodes) with the
2.56 V internal reference:

| VSER | Mode |
|---|---|
| ≥ 4.0 V (typ. 7 … 12 V) | RS-232: `clk/8`, software UART 9600 8N1, IR-Man handshake |
| < 1.5 V | USB: 16.5 MHz, V-USB HID |
| in between | re-measure for up to 0.5 s, then USB |

Why not simply "5 V vs 12 V" on the joined rail? Many laptops/adapters deliver only ±5 V on RS-232, which is indistinguishable from USB on the
joined rail. Looking at the serial-only node is unambiguous. (Don't connect both cables at once.)

## Build & flash

```bash
cd firmware
git clone https://github.com/obdev/v-usb vusb          # or copy your v-usb-master
cp -r vusb/usbdrv .  &&  cp vusb/libs-device/osccal.[ch] .
make                # needs avr-gcc, avr-libc
make fuse           # lfuse 0xE1 (PLL 16 MHz), hfuse 0xD5 (BOD 2.7 V, EEPROM kept), efuse 0xFF
make flash          # programmer: edit AVRDUDE in the Makefile (default usbasp)
```

Before publishing/using USB, **edit the vendor string in `usbconfig.h`** (shared USB-ID rules) and run `tools/test_ir_hash.c` if you change the decoder.

**First run:** plug the board into USB once. After the first bus reset the firmware tunes the RC oscillator (V-USB `osccal`) and stores the value in EEPROM;
the serial mode uses that value for accurate 9600 baud. Without it, serial mode still works on the factory calibration but with less margin.

## Using it

### Serial (IR-Man compatible)
Open the port at 9600 8N1, set **DTR and RTS high** (they power the device), send `I`,`R` → the device answers `O`,`K`; after that every key press sends 6 bytes.
Works with software that supports IR-Man (LIRC `irman`/`libirman`, WinLIRC-style tools, Girder plug-ins…). Quick test: `python tools/irman_serial_test.py COM3`.

### USB
Enumerates as a generic HID device `16c0:05df`, product string `IRman-Dual`. One 8-byte input report per received key frame:

| Byte | Meaning |
|---|---|
| 0 … 5 | the same 6-byte code as in serial mode |
| 6 | sequence counter (increments per report) |
| 7 | bit 0 = codes were dropped (4-deep queue overflowed) |

Quick test: `pip install hidapi` and `python tools/irman_usb_read.py`. `GET_REPORT` returns the last report, so polling clients work too.

## The 6-byte code

`[h3 h2 h1 h0 n 01]` - a 32-bit FNV hash of the relative length pattern of the frame (mark/space compared with the one two positions earlier, 20 % window),
`n` = number of intervals, `01` = decoder id. It is protocol-agnostic and gives a stable unique code per key for typical remotes (NEC, Sony SIRC, RC-5, …),
which is all IR-Man software needs. Notes: RC-5/RC-6 toggle bit changes the code on every new press (learn both); NEC repeat frames (3 intervals) are ignored
(`IR_MIN` in `main.c`); up to 96 intervals per frame (Igor's original stored 36).
Codes are **not** bit-identical to a genuine IR-Man - re-learn your remote once.

## Repository layout

```
firmware/   main.c  ir_hash.h  usbconfig.h  Makefile          (+ usbdrv/ and osccal.[ch] from V-USB)
hardware/   HARDWARE.md  netlist, BOM, power budget, bring-up checklist
docs/       HISTORY.md (IR-Man → clones → IgorPlug-USB → V-USB)   PIC_REVERSE.md (disassembly of ir16f84b.hex)
tools/      irman_usb_read.py  irman_serial_test.py  test_ir_hash.c
```

## Roadmap
* protocol-specific decoders (NEC/RC-5/Sony → stable codes without toggle bit), id byte `02…`
* USB keyboard/consumer-control mode so no host software is needed
* raw timing dump over a vendor control request (for LIRC `mode2`-like tools)
* KiCad schematic/PCB, release HEX, CI build

## License
GPL-3.0-or-later for this project; V-USB keeps its own GPLv2/3-or-commercial license. See [LICENSE-NOTE.md](LICENSE-NOTE.md).
