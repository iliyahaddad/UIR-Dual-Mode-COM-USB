/*
 * IRman-Dual  -  one ATtiny85, one IR receiver, two personalities
 *
 *   * RS-232 (COM port, powered from DTR/RTS)  -> classic IR-Man protocol
 *   * USB (V-USB, bus powered)                 -> HID device, 8-byte reports
 *
 * The personality is chosen ONCE after power-up by measuring the voltage on
 * the serial-only supply node (ADC3/PB3): RS-232 power present => serial.
 *
 * Shared pins (same MCU pins, different meaning):
 *   PB0  USB D-   | RS-232 TX  (MCU -> PC RXD, inverted TTL, idle low)
 *   PB2  USB D+   | RS-232 RX  (PC TXD -> MCU, via 100k, inverted)
 * Other pins:
 *   PB1  IR receiver output (active low, TSOP17xx / SFH506 type)
 *   PB3  ADC3  supply sense (divider from the DTR/RTS diode-OR node)
 *   PB4  switchable 1.5k USB pull-up (output high only in USB mode)
 *   PB5  /RESET (ISP)
 *
 * Fuses: lfuse=0xE1 (PLL 16 MHz) hfuse=0xD5 (BOD 2.7V, EEPROM preserved)
 *
 * License: GPL-3.0-or-later (V-USB is GPLv2/GPLv3, see usbdrv/License.txt)
 */
#include <avr/io.h>
#include <avr/wdt.h>
#include <avr/interrupt.h>
#include <avr/eeprom.h>
#include <avr/pgmspace.h>
#include <util/delay.h>
#include <stdint.h>
#include <stdlib.h>

#include "usbdrv.h"
#include "osccal.h"
#include "ir_hash.h"

/* ------------------------------ pins ------------------------------------ */
#define PIN_TX      PB0     /* shared with USB D- */
#define PIN_IR      PB1
#define PIN_RX      PB2     /* shared with USB D+ */
#define PIN_SENSE   PB3

/* ---------------------- supply sense / mode selection -------------------- */
#define SENSE_R_TOP_K   47UL    /* VSER -> SENSE */
#define SENSE_R_BOT_K   22UL    /* SENSE -> GND  */
#define SER_ON_MV       4000U   /* >= this on the DTR/RTS node  => RS-232 mode */
#define SER_OFF_MV      1500U   /* <  this                       => USB mode   */
                                /* in between: wait up to 0.5 s and re-check   */

/* ------------------------- oscillator calibration ------------------------ */
#define EE_OSCCAL       0       /* EEPROM byte 0 : OSCCAL tuned by USB      */
#define EE_MAGIC        1       /* EEPROM byte 1 : 0xA5 when byte 0 is valid */
#define EE_MAGIC_VAL    0xA5

/* ------------------------------ IR capture ------------------------------- */
#define IR_MAX          96      /* intervals stored per frame                */
#define IR_MIN          4       /* ignore shorter frames (3 = NEC repeat)    */
#define IR_GAP_TICKS    520     /* ~8 ms of silence ends a frame             */
#define IR_STUCK_TICKS  1800    /* ~28 ms continuous "mark": abort           */

static uint16_t ir_buf[IR_MAX];
static uint8_t  ir_n, ir_state, ir_armed, ir_lvl, tick_last;
static uint16_t ir_last, tick_hi;
static uint8_t  ir_code[6];

/* 16-bit monotonic time from 8-bit Timer1. Must be called at least every
 * ~4 ms (main loop does).                                                   */
static uint16_t tick16(void)
{
    uint8_t t = TCNT1;
    if (t < tick_last) tick_hi += 256;
    tick_last = t;
    return tick_hi + t;
}

static void ir_reset(void)
{
    tick_last = TCNT1;
    ir_state = 0;
    ir_armed = 0;
}

/* returns 1 when a complete frame has been converted into ir_code[] */
static uint8_t ir_poll(void)
{
    uint8_t  lvl = (PINB >> PIN_IR) & 1;      /* 1 = idle, 0 = IR burst */
    uint16_t now = tick16();

    if (!ir_state) {
        if (lvl) ir_armed = 1;
        else if (ir_armed) {                  /* falling edge: frame start */
            ir_state = 1; ir_n = 0; ir_last = now; ir_lvl = 0;
        }
        return 0;
    }
    if (lvl != ir_lvl) {                      /* edge inside frame */
        uint16_t d = now - ir_last;
        ir_last = now; ir_lvl = lvl;
        if (d > 2000) d = 2000;
        if (ir_n < IR_MAX) ir_buf[ir_n++] = d;
        return 0;
    }
    if (lvl) {
        if ((uint16_t)(now - ir_last) > IR_GAP_TICKS) {   /* end of frame */
            ir_state = 0;
            if (ir_n >= IR_MIN) {
                ir_make_code(ir_buf, ir_n, ir_code);
                return 1;
            }
        }
    } else if ((uint16_t)(now - ir_last) > IR_STUCK_TICKS) {
        ir_state = 0; ir_armed = 0;           /* jammed low: wait for idle */
    }
    return 0;
}

/* ------------------------------- detection ------------------------------- */
static uint16_t read_vser_mv(void)
{
    uint32_t acc = 0;
    uint8_t  i;

    DIDR0  |= (1 << ADC3D);
    ADMUX   = (1 << REFS2) | (1 << REFS1) | 3;            /* 2.56 V ref, ADC3 */
    ADCSRA  = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    _delay_ms(2);
    for (i = 0; i < 17; i++) {
        ADCSRA |= (1 << ADSC);
        while (ADCSRA & (1 << ADSC)) ;
        if (i) acc += ADC;                                /* drop 1st sample */
    }
    ADCSRA = 0;
    acc = acc * 2560UL / (16UL * 1023UL);                 /* mV at SENSE pin */
    return (uint16_t)(acc * (SENSE_R_TOP_K + SENSE_R_BOT_K) / SENSE_R_BOT_K);
}

static uint8_t detect_serial(void)
{
    uint8_t t;
    for (t = 0; t < 10; t++) {
        uint16_t mv = read_vser_mv();
        if (mv >= SER_ON_MV)  return 1;
        if (mv <  SER_OFF_MV) return 0;
        _delay_ms(50);
    }
    return 0;
}

/* =========================================================================
 *                       RS-232 / IR-Man personality
 * =========================================================================
 * Line polarity is INVERTED TTL exactly like the original PIC16F84 clone
 * (verified by disassembling ir16f84b.hex): idle = pin low, start bit = pin
 * high, data bit 1 = pin low.  The PC sees 0 V as "mark" (idle) and +4.7 V
 * as "space", which all PC UARTs accept; RX uses the real -12/+12 V swing
 * through 100k, read by the MCU as 0 / high.
 * 9600 baud 8N1, timing from Timer0 CTC polling (no interrupts).
 * ========================================================================= */
static uint8_t bit_ticks;              /* Timer0 ticks per bit (26 or 27)    */

#define T0_START(top)  do { TCNT0 = 0; OCR0A = (top); TIFR = (1 << OCF0A); } while (0)
#define T0_WAIT()      do { while (!(TIFR & (1 << OCF0A))) ; TIFR = (1 << OCF0A); } while (0)

static void ser_tx_byte(uint8_t b)
{
    uint8_t i;
    T0_START(bit_ticks - 1);
    PORTB |= (1 << PIN_TX);                    /* start bit */
    T0_WAIT();
    for (i = 0; i < 8; i++) {
        if (b & 1) PORTB &= ~(1 << PIN_TX);    /* logic 1 -> pin low  */
        else       PORTB |=  (1 << PIN_TX);    /* logic 0 -> pin high */
        b >>= 1;
        T0_WAIT();
    }
    PORTB &= ~(1 << PIN_TX);                   /* stop bit / idle */
    T0_WAIT();
}

/* returns byte or -1 if no start bit present */
static int16_t ser_rx_try(void)
{
    uint8_t i, v = 0;

    /* In the inverted IR-Man wiring, idle is LOW and a start bit is HIGH.
     * Wait approximately half a bit, verify the start bit, then wait one
     * complete bit before sampling the centre of data bit 0.  The old code
     * subtracted four Timer0 ticks here, which moved every data sample about
     * 20 us early at 9600 baud. */
    if (!(PINB & (1 << PIN_RX))) return -1;    /* idle = low */

    T0_START((uint8_t)(bit_ticks / 2u - 1u));
    T0_WAIT();
    if (!(PINB & (1 << PIN_RX))) return -1;    /* glitch / false start */

    for (i = 0; i < 8; i++) {
        T0_START((uint8_t)(bit_ticks - 1u));
        T0_WAIT();
        v >>= 1;
        if (!(PINB & (1 << PIN_RX))) v |= 0x80; /* pin low = logic 1 */
    }

    /* Move to the centre of the stop bit.  Waiting here also leaves enough
     * margin before a possible immediately-following start bit. */
    T0_START((uint8_t)(bit_ticks - 1u));
    T0_WAIT();
    return v;
}

static void serial_mode(void)
{
    uint8_t i, hs = 0, active = 0;
    int16_t c;

    if (eeprom_read_byte((uint8_t *)EE_MAGIC) == EE_MAGIC_VAL) {
        OSCCAL = eeprom_read_byte((uint8_t *)EE_OSCCAL);   /* tuned by USB */
        bit_ticks = 27;      /* 16.5 MHz / 8 / 8 / 9600 = 26.86 */
    } else {
        bit_ticks = 26;      /* factory ~16.0 MHz: 16e6/64/9600 = 26.04 */
    }

    cli();
    CLKPR = (1 << CLKPCE);
    CLKPR = (1 << CLKPS1) | (1 << CLKPS0);    /* clk / 8 : saves RTS/DTR current */
    PRR |= (1 << PRADC) | (1 << PRUSI);

    DDRB  |= (1 << PIN_TX);                   /* TX idle = low */
    PORTB &= ~(1 << PIN_TX);

    TCCR0A = (1 << WGM01);                    /* Timer0 CTC                     */
    TCCR0B = (1 << CS01);                     /* clk/8  (~258 kHz)              */
    TCCR1  = (1 << CS12) | (1 << CS11);       /* Timer1 clk/32 -> ~15.5 us tick */

    wdt_enable(WDTO_2S);
    ir_reset();

    for (;;) {
        wdt_reset();

        c = ser_rx_try();
        if (c >= 0) {                         /* handshake: "IR" -> "OK"        */
            if (c == 'I')              hs = 1;
            else if (c == 'R' && hs) { hs = 0; ser_tx_byte('O'); ser_tx_byte('K');
                                       active = 1; ir_reset(); }
            else                       hs = 0;
        }
        if (ir_poll() && active) {
            for (i = 0; i < 6; i++) ser_tx_byte(ir_code[i]);
            ir_reset();
        }
    }
}

/* =========================================================================
 *                            USB / HID personality
 * ========================================================================= */
PROGMEM const char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = {
    0x06, 0x00, 0xff,       /* USAGE_PAGE (Vendor Defined 0xFF00)        */
    0x09, 0x01,             /* USAGE (1)                                 */
    0xa1, 0x01,             /* COLLECTION (Application)                  */
    0x15, 0x00,             /*   LOGICAL_MINIMUM (0)                     */
    0x26, 0xff, 0x00,       /*   LOGICAL_MAXIMUM (255)                   */
    0x75, 0x08,             /*   REPORT_SIZE (8)                         */
    0x95, 0x08,             /*   REPORT_COUNT (8)                        */
    0x09, 0x01,             /*   USAGE (1)                               */
    0x81, 0x02,             /*   INPUT (Data,Var,Abs)  = 8 byte report   */
    0xc0                    /* END_COLLECTION                            */
};
typedef char hid_len_check[(sizeof(usbHidReportDescriptor) == USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH) ? 1 : -1];

/* report = code[6], sequence counter, flags (bit0 = codes were dropped) */
static uint8_t  last_report[8];
static uint8_t  rep_seq, idle_rate, lost_flag;

#define QN 4
static uint8_t  q[QN][6];
static uint8_t  q_head, q_count;

static volatile uint8_t osc_dirty;

void hadUsbReset(void)                  /* called by V-USB after bus reset */
{
    cli();
    calibrateOscillator();
    sei();
    osc_dirty = 1;
}

static void store_osccal(void)
{
    uint8_t cur = OSCCAL;
    uint8_t valid = (eeprom_read_byte((uint8_t *)EE_MAGIC) == EE_MAGIC_VAL);
    int16_t d = (int16_t)cur - (int16_t)eeprom_read_byte((uint8_t *)EE_OSCCAL);

    if (!valid || d > 1 || d < -1) {       /* avoid EEPROM wear for +/-1 jitter */
        eeprom_update_byte((uint8_t *)EE_OSCCAL, cur);
        eeprom_update_byte((uint8_t *)EE_MAGIC, EE_MAGIC_VAL);
    }
}

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
    usbRequest_t *rq = (void *)data;

    if ((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS) {
        switch (rq->bRequest) {
        case USBRQ_HID_GET_REPORT:
            usbMsgPtr = (usbMsgPtr_t)last_report;
            return 8;
        case USBRQ_HID_GET_IDLE:
            usbMsgPtr = (usbMsgPtr_t)&idle_rate;
            return 1;
        case USBRQ_HID_SET_IDLE:
            idle_rate = rq->wValue.bytes[1];
            return 0;
        }
    }
    return 0;
}

static void usb_mode(void)
{
    uint8_t i;

    TCCR1 = (1 << CS13) | (1 << CS10);        /* Timer1 clk/256 -> ~15.5 us tick */
    wdt_enable(WDTO_2S);

    usbInit();
    usbDeviceDisconnect();                    /* force re-enumeration */
    for (i = 0; i < 250; i++) { wdt_reset(); _delay_ms(2); }
    usbDeviceConnect();
    sei();
    ir_reset();

    for (;;) {
        wdt_reset();
        usbPoll();

        if (ir_poll()) {
            if (q_count < QN) {
                uint8_t k, *dst = q[(uint8_t)((q_head + q_count) % QN)];
                for (k = 0; k < 6; k++) dst[k] = ir_code[k];
                q_count++;
            } else {
                lost_flag = 1;
            }
        }
        if (q_count && usbInterruptIsReady()) {
            uint8_t k;
            for (k = 0; k < 6; k++) last_report[k] = q[q_head][k];
            last_report[6] = ++rep_seq;
            last_report[7] = lost_flag;
            lost_flag = 0;
            q_head = (q_head + 1) % QN;
            q_count--;
            usbSetInterrupt(last_report, 8);
        }
        if (osc_dirty) { osc_dirty = 0; store_osccal(); }
    }
}

/* =========================================================================
 *                                  main
 * ========================================================================= */
int main(void)
{
    MCUSR = 0;
    wdt_disable();
    DDRB  = 0;                      /* everything input, no pull-ups: safe on  */
    PORTB = 0;                      /* RS-232 levels and on USB lines          */

    _delay_ms(100);                 /* let the supply settle before measuring  */

    if (detect_serial()) serial_mode();
    else                 usb_mode();
    for (;;) ;
}
