/* usbconfig.h - V-USB configuration for IRman-Dual (ATtiny85 @ 16.5 MHz)
 * See usbdrv/usbconfig-prototype.h for the meaning of every option.        */
#ifndef __usbconfig_h_included__
#define __usbconfig_h_included__

/* ---------------------------- Hardware Config ---------------------------- */
#define USB_CFG_IOPORTNAME      B
#define USB_CFG_DMINUS_BIT      0      /* PB0 = D-  (shared with serial TX -> PC RXD) */
#define USB_CFG_DPLUS_BIT       2      /* PB2 = D+ / INT0 (shared with serial RX <- PC TXD) */
#define USB_CFG_CLOCK_KHZ       16500  /* ATtiny85 PLL clock, tuned by osccal */
#define USB_CFG_CHECK_CRC       0

/* 1.5k pull-up on D- is switched by PB4: it must NOT be connected in serial
 * mode, otherwise it would load the RS-232 TX line.                         */
#define USB_CFG_PULLUP_IOPORTNAME   B
#define USB_CFG_PULLUP_BIT          4

/* --------------------------- Functional Range ---------------------------- */
#define USB_CFG_HAVE_INTRIN_ENDPOINT    1
#define USB_CFG_HAVE_INTRIN_ENDPOINT3   0
#define USB_CFG_EP3_NUMBER              3
#define USB_CFG_SUPPRESS_INTR_CODE      0
#define USB_CFG_INTR_POLL_INTERVAL      10
#define USB_CFG_IS_SELF_POWERED         0
#define USB_CFG_MAX_BUS_POWER           50
#define USB_CFG_IMPLEMENT_FN_WRITE      0
#define USB_CFG_IMPLEMENT_FN_READ       0
#define USB_CFG_IMPLEMENT_FN_WRITEOUT   0
#define USB_CFG_HAVE_FLOWCONTROL        0
#define USB_CFG_DRIVER_FLASH_PAGE       0
#define USB_CFG_LONG_TRANSFERS          0
#define USB_COUNT_SOF                   0
#define USB_CFG_CHECK_DATA_TOGGLING     0
#define USB_CFG_HAVE_MEASURE_FRAME_LENGTH   1   /* needed for osccal */
#define USB_USE_FAST_CRC                0

/* Re-tune the RC oscillator after every USB bus reset (see osccal.c).       */
#ifndef __ASSEMBLER__
#include <avr/interrupt.h>
extern void hadUsbReset(void);
#endif
#define USB_RESET_HOOK(resetStarts)     if(!(resetStarts)){hadUsbReset();}

/* -------------------------- Device Description --------------------------- */
/* 0x16c0/0x05df = obdev shared ID for generic HID devices (not keyboard/mouse).
 * Rules (see usbdrv/USB-IDs-for-free.txt): the vendor string MUST contain an
 * e-mail address or domain name that YOU own.  >>> CHANGE THE VENDOR NAME <<<  */
#define USB_CFG_VENDOR_ID       0xc0, 0x16
#define USB_CFG_DEVICE_ID       0xdf, 0x05
#define USB_CFG_DEVICE_VERSION  0x00, 0x01

#define USB_CFG_VENDOR_NAME     'c','h','a','n','g','e','-','m','e','@','e','x','a','m','p','l','e','.','c','o','m'
#define USB_CFG_VENDOR_NAME_LEN 21
#define USB_CFG_DEVICE_NAME     'I','R','m','a','n','-','D','u','a','l'
#define USB_CFG_DEVICE_NAME_LEN 10

#define USB_CFG_DEVICE_CLASS        0      /* class is defined at interface level */
#define USB_CFG_DEVICE_SUBCLASS     0
#define USB_CFG_INTERFACE_CLASS     3      /* HID */
#define USB_CFG_INTERFACE_SUBCLASS  0
#define USB_CFG_INTERFACE_PROTOCOL  0
#define USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH    21   /* keep in sync with main.c */

/* ------------------- Fine Control over USB Descriptors ------------------- */
#define USB_CFG_DESCR_PROPS_DEVICE                  0
#define USB_CFG_DESCR_PROPS_CONFIGURATION           0
#define USB_CFG_DESCR_PROPS_STRINGS                 0
#define USB_CFG_DESCR_PROPS_STRING_0                0
#define USB_CFG_DESCR_PROPS_STRING_VENDOR           0
#define USB_CFG_DESCR_PROPS_STRING_PRODUCT          0
#define USB_CFG_DESCR_PROPS_STRING_SERIAL_NUMBER    0
#define USB_CFG_DESCR_PROPS_HID                     0
#define USB_CFG_DESCR_PROPS_HID_REPORT              0
#define USB_CFG_DESCR_PROPS_UNKNOWN                 0

#endif /* __usbconfig_h_included__ */
