# History and background

## 1. The serial IR-Man (late 1990s)

Evation's **IR-Man** was a small box with a 9-pin connector and an IR receiver. It took its
power from the serial handshake lines (RTS/DTR) and, for every key press on an ordinary remote,
sent a sequence of **six bytes** to the COM port. A Dr. Dobb's Journal article from 2000 describes
exactly this: the six bytes are fairly arbitrary, but one-to-one with the key, so software only has to
compare them with stored values.
Later analysis by the home-theater community describes the bytes as a timing-based pattern of the
IR burst (long gap ≈ 1, short gap ≈ 0, LSB first) rather than a protocol-level address/command.

The IR-Man protocol used by LIRC, WinLIRC, Girder plug-ins etc. is tiny: the host powers the device (DTR and RTS high),
sends `IR`, the device answers `OK`, and from then on every key press yields 6 bytes at 9600 8N1.
LIRC kept an `irman` driver for it (`libirman`); the real hardware is discontinued but some modern
devices (for example the IR Toy) emulate the protocol.

## 2. Homebrew clones

Because the device is just "IR demodulator + UART", clones appeared quickly. The schematic in this
repository (`IR-Man clone – Chaveiro '99`) uses a PIC16F84 at 4 MHz, an SFH506-36 receiver, two 1N4148 diodes
to draw power from DTR/RTS, a 5.1 V zener and a 100 k resistor on TXD. Disassembling the shipped HEX
(see `PIC_REVERSE.md`) confirms the original handshake (`IR` → `OK`), inverted-TTL serial levels and the
six-byte output.

## 3. Igor Češko's software USB (2002–2004) - IgorPlug-USB

When serial ports began to disappear, Slovak engineer **Ing. Igor Češko** implemented a full-speed-less,
**low-speed USB device in pure AVR firmware** (no USB hardware in the chip):

* `USB90S2313.asm`, version 1.6 (April 2003): *"USB stack + infrared remote control to non-USB MCU"* for AT90S2313 at 12 MHz,
  with a TSOP17xx/SFH511x sensor, an 8-bit I/O port, an RS-232 line and EEPROM access.
* `USBtoRS232.asm`, version 2.8 (Feb 2004): *AVR309 - USB to UART protocol converter* for ATmega8, with FIFO.
* The Windows side is `IgorUSB.dll` (`DoGetInfraCode`, `DoRS232Send`, …).

The IR part of this design became known as **IgorPlug-USB**. On Linux, `lirc_igorplugusb` (Jan M. Hochstein, 2004) and,
later, the in-kernel `igorplugusb` driver (Sean Young, 2014) support it (USB ID 03eb:0002, Atmel's vendor ID as used by the
original firmware). The kernel driver notes the limitation of the original design: the device only stores
**36 pulse/space values**, too few for NEC, RC-6, Sony-20 and similar. IRman-Dual stores 96.

## 4. V-USB (2005 - today)

Objective Development published the same idea as a reusable library: **AVR-USB**, first changelog entry 2005-04-01,
later **renamed V-USB** "due to a trademark issue with Atmel". It supports 12 - 20 MHz clocks, optional
interrupt endpoint, and an RC-oscillator tuning helper (`osccal`) that makes crystal-less ATtiny45/85 designs
possible. It is dual-licensed (GPL v2/v3 or commercial) and comes with shared USB IDs for hobby projects
(`usbdrv/USB-IDs-for-free.txt`).

## 5. This project

IRman-Dual merges the three lines:

* **IR-Man behaviour** on the serial side (same pins' roles, same handshake, same inverted levels, same six-byte output),
* **software USB** on the same MCU pins (V-USB instead of Igor's assembler, so it runs on an ATtiny85 and is maintainable in C),
* **automatic personality switching**, which neither the IR-Man, its clones nor IgorPlug-USB had: the firmware looks at the serial-only supply node
  and chooses RS-232 or USB at power-up.

### Sources

* Dr. Dobb's Journal, "Infrared Control of Your PC" (2000): https://www.drdobbs.com/mobile/infrared-control-of-your-pc/184404100
* libirman / `lirc-drv-irman` package description: https://community.linuxmint.com/software/view/lirc-drv-irman
* Linux kernel `drivers/media/rc/igorplugusb.c`: https://kernelsources.org/source/raw/linux/drivers/media/rc/igorplugusb.c
* Igor Češko's firmware and DLL documentation (files supplied with this project: `Firmware.zip`, `IgorPlugUSBdoc.zip`)
* V-USB: https://www.obdev.at/vusb/ and the repository's `usbdrv/Changelog.txt`
