# Reverse-engineering notes: `ir16f84b.hex` (PIC16F84 IR-Man clone, "Chaveiro '99")

The HEX was disassembled (PIC16 mid-range opcodes) and compared with the schematic
`ir_16f84_interface.jpg`. Findings that the new firmware copies:

| Item | Finding |
|---|---|
| Clock | 4 MHz crystal → 1 µs per instruction |
| Port | `TRISB = 0x09` → **RB0 = RX in**, **RB2 = TX out**, **RB3 = IR receiver in** (active low) |
| Polarity | RX: start bit = RB0 **high**, bit value = **inverted** pin level. TX: idle **low**, start = high, bit 1 = pin low. I.e. inverted TTL, which is what the 100 k + 5V1 zener input and direct output of the schematic imply |
| Baud | 9600 8N1. TX uses `DECFSZ` delay loops with count `0x1F` (≈ 104 cycles/bit); RX start→first sample uses `0x2E` (≈ 1.5 bit) |
| Handshake | waits for `'I'` (0x49) then `'R'` (0x52); answers `'O'` (0x4F) `'K'` (0x4B); anything else restarts |
| Capture | `OPTION = 0xC2` (Timer0 prescaler 1:8 = 8 µs tick) for measuring pulses; `0xC7` (1:256) while waiting ~32 ms for the line to become idle |
| Decoder | appears to be adaptive: measures the header / first pulse, then samples 48 (0x30) bits into six registers (0x16…0x1B) using the learned unit |
| Output | six bytes, register 0x1B first … 0x16 last, each sent at 9600 baud; `CLRWDT` is used throughout, and the `TO` flag is checked at start-up |

What was **not** ported: the exact bit-for-bit decoder. IR-Man software (LIRC `irman`,
WinLIRC, Girder plugins, …) treats the six bytes as an opaque per-key identifier, so
IRman-Dual generates its own stable 6-byte fingerprint (see `firmware/ir_hash.h`).
If you need byte-identical codes with a real IR-Man, re-learn the remote once.

## About the PIC skeleton UART in the request

The pasted `Software UART PIC16F84A` skeleton cannot be used unchanged:

* it is **non-inverted**, the IR-Man wiring needs **inverted** levels;
* `BIT_TIME`/`HALF_BIT` are empty. At 4 MHz one bit is 104 instruction cycles (1 MHz / 9600 = 104.17);
* one shared variable for RX/TX is fine only for half-duplex use (as here);
* a PIC16F84 cannot run a software USB stack (V-USB is AVR-only and needs an AVR running at 12 MHz or more, i.e. ≥ 12 MIPS, while a PIC16F84 tops out at 5 MIPS),
  which is why this project uses an ATtiny85 for the dual interface.
