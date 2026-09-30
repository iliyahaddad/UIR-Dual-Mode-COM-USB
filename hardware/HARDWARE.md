# Hardware - IRman-Dual

One ATtiny85 (DIP-8), one IR receiver module, a DB9 (or bare wires) for RS-232
and a USB plug (or cable) for USB. **Only one of the two may be connected at a time.**

```
 RS-232 (DB9)                                                         ATtiny85
 ------------                                                        ┌───┴───┐
 DTR 4 ──►|─┐ 1N4148                                          RESET ─┤1     8├─ VCC
 RTS 7 ──►|─┴──────► VSER ──┬── R1 47k ──┬── 10nF ─ GND   SENSE PB3 ─┤2     7├─ PB2 D+ / RX
                            │            └──────────────►  (pin 2)   │       │
                            │       R2 22k to GND          PULLUP PB4 ┤3     6├─ PB1 IR in
                            └──► U2 LP2950-5.0 ──►|─┐ BAT85      GND ─┤4     5├─ PB0 D- / TX
                                  (2.2uF on OUT)    ├──► VCC          └───────┘
 USB VBUS ─────────────────────────────────►|───────┘ BAT85
```

## Netlist

| Net | Connections |
|---|---|
| **GND** | DB9-5, USB GND, U1-4, U2 GND, IR module GND, all capacitors |
| **VSER** | cathodes of D1 (anode = DB9-4 DTR) and D2 (anode = DB9-7 RTS); U2 IN; R1 |
| **VCC** (~4.7 V) | cathode of D3 (anode = USB VBUS); cathode of D5 (anode = U2 OUT); U1-8; IR module Vs; C 10 µF + 100 nF |
| **SENSE** | R1 (47 k from VSER), R2 (22 k to GND), 10 nF to GND, U1-2 (PB3 / ADC3) |
| **PULLUP** | U1-3 (PB4) → R3 1.5 k → DM_C |
| **DM_C** (USB D- at connector side) | USB D-, Z1 (BZX55C3V6 → GND), R3 1.5 k, R4 68 Ω |
| **DP_C** (USB D+ at connector side) | USB D+, Z2 (BZX55C3V6 → GND), R5 68 Ω |
| **PB0 / TX** | U1-5, R4 68 Ω → DM_C, **DB9-2 (RXD of PC) directly** |
| **PB2 / RX** | U1-7, R5 68 Ω → DP_C, **R6 100 k ← DB9-3 (TXD of PC)** |
| **IR** | IR module OUT → U1-6 (PB1). Recommended: 100 Ω + 4.7 µF supply filter at the module |

Note on the two "shared" pins: the serial connector and the USB connector are
separate physical connectors that land on the *same two MCU pins*. The 68 Ω /
3.6 V zener branch only matters in USB mode, the 100 k resistor only matters in
serial mode; when the other cable is unplugged its branch just hangs on the pin.

## Bill of materials

| Ref | Part | Notes |
|---|---|---|
| U1 | ATtiny85-20PU (or -20SU) | fuses: see README |
| U2 | LP2950-5.0 (TO-92) | 30 V input, 75 µA quiescent; 2.2 µF on output |
| D1, D2 | 1N4148 | DTR / RTS OR-ing |
| D3, D5 | BAT85 or 1N5817 (Schottky) | rail OR-ing (USB / LDO) |
| Z1, Z2 | BZX55C3V6 (low-current 3.6 V) | **must be low leakage** (V-USB "with-zener" circuit) |
| R1, R2 | 47 k, 22 k | supply sense divider |
| R3 | 1.5 k | USB low-speed pull-up (switched by PB4) |
| R4, R5 | 68 Ω | USB series resistors |
| R6 | 100 k | RS-232 input limiter |
| C | 10 µF + 100 nF (VCC), 2.2 µF (LDO out), 10 nF (SENSE), 4.7 µF (IR filter) | |
| IR | SFH506-36 / TSOP1736 / TSOP1738 style 4.5–5.5 V module | output active-low |

## Why it is built this way

**Supply.** RS-232 gives +5…+12 V on DTR/RTS at only a few mA; USB gives 5 V.
Both are brought to one ~4.7 V rail (5 V minus a Schottky). A 5 V rail is used
instead of 3.3 V because the IR module wants 4.5–5.5 V and the ATtiny85 is only
specified for the 16.5 MHz V-USB clock at ≥ 4.5 V. The 3.6 V zeners handle the
USB signal levels (classic V-USB circuit).

**Voltage detection (your 12 V vs 5 V idea).** The firmware measures the
voltage on **VSER**, the node that is fed *only* by the RS-232 handshake lines:

| Voltage on VSER | Decision |
|---|---|
| < 1.5 V | nobody drives DTR/RTS → the board is USB powered → **USB mode** |
| ≥ 4.0 V | RS-232 supply present → **serial mode** |
| 1.5 … 4.0 V | port still ramping → wait ≤ 0.5 s and measure again, else USB |

Measuring the *joined* rail and trying to tell "5 V" from "12 V" would fail on
weak/low-swing ports (some laptops and USB-serial adapters only give ±5 V,
which looks exactly like USB). Looking at the serial-only node avoids that.
The 2.56 V internal ADC reference makes the result independent of VCC.

**Serial levels.** Same inverted-TTL scheme as the original PIC clone: TX idles
at 0 V and swings to ~4.7 V (PC UARTs read 0 V as "mark"); RX uses the real
±12 V signal through 100 k into the MCU pin protection diodes (0.12 mA).

**USB pull-up.** The 1.5 k is driven from PB4, not from VCC, so it is *off* in
serial mode (otherwise it would hold the RS-232 TX line high).

## Power budget (serial mode)

The firmware divides the CPU clock by 8 (≈ 2 MHz) after the mode is chosen.
Expect ≈ 3–4 mA total (MCU ~1.5 mA, IR module ~0.5 mA, LP2950 75 µA, sense
divider up to ~0.8 mA at 12 V). DTR+RTS of a normal PC port supply more than that.
With ±5 V ports the rail drops to ~3.7 V: the MCU still runs, the IR module may lose range.

## Programming (ISP)

PB0/PB1/PB2 are also MOSI/MISO/SCK. Program the chip **before** connecting the USB or
serial cables (or disconnect them), using the 6-pin ISP header wired to pins 1, 4, 5, 6, 7, 8.

## Bring-up checklist

1. Flash, set fuses (`make fuse`). Nothing connected to DB9/USB yet.
2. USB only: VCC must read 4.6–4.8 V; the PC should enumerate "IRman-Dual". Run `tools/irman_usb_read.py`.
3. Unplug USB. Serial only: run `tools/irman_serial_test.py COMx`. VSER should be 7–12 V, VCC ≈ 4.7 V.
4. Scope PB0 while sending "IR": bit time must be 104 µs (±3 %). If the handshake fails, plug the device
   via USB once (it calibrates its oscillator and stores it in EEPROM) and retry serial mode.
5. Never connect both cables at once.
