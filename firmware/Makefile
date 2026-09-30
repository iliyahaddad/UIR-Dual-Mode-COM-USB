# IRman-Dual firmware - ATtiny85 @ 16.5 MHz (PLL) + V-USB
#
# 1) Put V-USB next to this Makefile:
#      git submodule add https://github.com/obdev/v-usb vusb   (or copy)
#    then:  ln -s vusb/usbdrv usbdrv ; ln -s vusb/libs-device/osccal.c . ; ln -s vusb/libs-device/osccal.h .
#    (or simply copy the folder usbdrv/ and the two osccal files here)
# 2) make            -> main.hex
# 3) make fuse && make flash

DEVICE   = attiny85
F_CPU    = 16500000
AVRDUDE  = avrdude -c usbasp -p t85
LFUSE    = 0xE1
HFUSE    = 0xD5
EFUSE    = 0xFF

CFLAGS   = -Wall -Os -std=gnu99 -mmcu=$(DEVICE) -DF_CPU=$(F_CPU)UL -I. -Iusbdrv \
           -ffunction-sections -fdata-sections
ASFLAGS  = -mmcu=$(DEVICE) -DF_CPU=$(F_CPU)UL -I. -Iusbdrv -x assembler-with-cpp
LDFLAGS  = -mmcu=$(DEVICE) -Wl,--gc-sections

OBJ = main.o osccal.o usbdrv/usbdrv.o usbdrv/usbdrvasm.o

all: main.hex

%.o: %.c
	avr-gcc $(CFLAGS) -c $< -o $@
%.o: %.S
	avr-gcc $(ASFLAGS) -c $< -o $@

main.elf: $(OBJ)
	avr-gcc $(LDFLAGS) -o $@ $(OBJ)
	avr-size --format=avr --mcu=$(DEVICE) $@

main.hex: main.elf
	avr-objcopy -j .text -j .data -O ihex $< $@

flash: main.hex
	$(AVRDUDE) -U flash:w:main.hex:i
fuse:
	$(AVRDUDE) -U lfuse:w:$(LFUSE):m -U hfuse:w:$(HFUSE):m -U efuse:w:$(EFUSE):m
clean:
	rm -f main.elf main.hex *.o usbdrv/*.o
.PHONY: all flash fuse clean
