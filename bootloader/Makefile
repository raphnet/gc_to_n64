CC=avr-gcc
AS=$(CC)
LD=$(CC)

BOOTSTART=0x1800 # note: byte address
VERSION=1.0
PROGNAME=siboot
CPU=atmega8
UISP=uisp -dprog=stk500 -dpart=atmega8 -dserial=/dev/avr
CFLAGS=-Wall -mmcu=$(CPU) -DF_CPU=16000000L -Os -DVERSION_STR=\"$(VERSION)\" -DPAGE_SIZE=64 -DBOOTSTART=$(BOOTSTART)
LDFLAGS=-mmcu=$(CPU) -Wl,-Map=siboot.map -Wl,--section-start=.text=$(BOOTSTART)
HEXFILE=siboot.hex
AVRDUDE=avrdude
AVRDUDE_CPU=m8
#AVRDUDE_CPU=m88

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
# FUSE low byte
#
# 7: BODLEVEL -> 0  (Reset threshold: 3.7 ~ 4.5)
# 6: BODEN    -> 0  (Enabled)
# 5: SUT1     -> 0
# 4: SUT2     -> 1
# 3: CKSEL3   -> 1
# 2: CKSEL2   -> 1
# 1: CKSEL1   -> 1
# 0: CKSEL0   -> 1
#
# FUSE high byte
#
# 7: RSTDISBL -> 1
# 6: WDTON    -> 1
# 5: SPIEN    -> 0
# 4: CKOPT    -> 1
# 3: EESAVE   -> 1
#
# 2: BOOTSZ1  -> 0
# 1: BOOTSZ0  -> 0
# 0: BOOTRST  -> 0
#
# 1024 words bootloader

fuse:
	$(AVRDUDE) -p $(AVRDUDE_CPU) -e -P usb -c avrispmkII -Uhfuse:w:0xd8:m -Ulfuse:w:0x1f:m -B 20.0 -F

flash: $(HEXFILE)
	$(AVRDUDE) -p $(AVRDUDE_CPU) -P usb -c avrispmkII -Uflash:w:$(HEXFILE) -B 1.0 -F

reset:
	$(AVRDUDE) -p $(AVRDUDE_CPU) -P usb -c avrispmkII -B 1.0 -F

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<
