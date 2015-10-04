CC=avr-gcc
AS=$(CC)
LD=$(CC)

CPU=atmega168
BOOTSTART=0x3800 # note: byte address
PAGE_SIZE=128 # bytes

PROGNAME=siboot
VERSION=1.0

CFLAGS=-Wall -mmcu=$(CPU) -DF_CPU=16000000L -Os -DVERSION_STR=\"$(VERSION)\" -DPAGE_SIZE=$(PAGE_SIZE) -DBOOTSTART=$(BOOTSTART) -DWITH_LED
LDFLAGS=-mmcu=$(CPU) -Wl,-Map=siboot.map -Wl,--section-start=.text=$(BOOTSTART)
HEXFILE=siboot.hex
AVRDUDE=avrdude
AVRDUDE_CPU=m168

OBJS=main.o n64_isr.o

all: $(HEXFILE)

clean:
	rm -f siboot.elf siboot.hex siboot.map $(OBJS)

siboot.elf: $(OBJS)
	$(LD) $(OBJS) $(LDFLAGS) -o siboot.elf

siboot.hex: siboot.elf
	avr-objcopy -j .data -j .text -O ihex siboot.elf siboot.hex
	avr-size siboot.elf

#
# Extended byte: 0xF9
#
# - - - - - BOOTSZ1 BOOTSZ0 BOOTRST
# 0 0 0 0 0    0       0       0
#
# Boot size: 1024 words
# Reset into bootloader
#
EFUSE=0x00

#
# Low fuse byte
#
# CKDIV8	CKOUT	SUT1	SUT0	CKSEL[4]
#   1         1      1       0
#
LFUSE=0xD7

# High fuse byte
#
# RSTDISBL	DWEN	SPIEN	WDTON	EESAVE	BODLEVEL[3]
#   1        1         0      1       1         100
#
# BOD level = 4.1 to 4.5 volt
HFUSE=0xDC



fuse:
	$(AVRDUDE) -p $(AVRDUDE_CPU) -e -P usb -c avrispmkII -Uefuse:w:$(EFUSE):m -Uhfuse:w:$(HFUSE):m -Ulfuse:w:$(LFUSE):m -B 20.0 -F

flash: $(HEXFILE)
	$(AVRDUDE) -p $(AVRDUDE_CPU) -P usb -c avrispmkII -Uflash:w:$(HEXFILE) -B 1.0 -F

reset:
	$(AVRDUDE) -p $(AVRDUDE_CPU) -P usb -c avrispmkII -B 1.0 -F

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<
