CC=avr-gcc
AS=$(CC)
LD=$(CC)

VERSION=2.1-test1
CPU=atmega168
CFLAGS=-Wall -mmcu=$(CPU) -DF_CPU=16000000L -Os -DVISUAL_BUZZER -DVERSION_STR=\"$(VERSION)\"
LDFLAGS=-mmcu=$(CPU) -Wl,-Map=gc_to_n64.map -Wl,--section-start=.endmarker=0x37fc
HEXFILE=gc_to_n64b.hex
AVRDUDE=avrdude
AVRDUDE_CPU=m168

OBJS=main.o gamecube.o n64_isr.o mapper.o gamecube_mapping.o n64_mapping.o buzzer.o timer0.o eeprom.o sync.o lut.o gcn64_protocol.o

all: $(HEXFILE)

clean:
	rm -f gc_to_n64.elf gc_to_n64_tmp.hex $(HEXFILE) gc_to_n64.map $(OBJS)
	$(MAKE) -C bootloader clean

gc_to_n64.elf: $(OBJS)
	$(LD) $(OBJS) $(LDFLAGS) -o gc_to_n64.elf

gc_to_n64_tmp.hex: gc_to_n64.elf
	avr-objcopy -j .data -j .text -j .endmarker -O ihex gc_to_n64.elf gc_to_n64_tmp.hex
	avr-size gc_to_n64.elf

bootloader/siboot.hex:
	$(MAKE) -C bootloader

$(HEXFILE): gc_to_n64_tmp.hex bootloader/siboot.hex
	srec_cat gc_to_n64_tmp.hex -I bootloader/siboot.hex -I -o $@ -I

#
# Extended byte: 0xF9
#
EFUSE=0x01

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


#
# Lock bits
#
# - - BLB12 BLB11 BLB02 BLB01 LB2 LB1
# 0 0   1    0      1     1    1   1
#
#
LOCK=0x2F

fuse:
	$(AVRDUDE) -p $(AVRDUDE_CPU) -P usb -c avrispmkII -Uefuse:w:$(EFUSE):m -Uhfuse:w:$(HFUSE):m -Ulfuse:w:$(LFUSE):m -B 20.0 -F
	$(AVRDUDE) -p $(AVRDUDE_CPU) -P usb -c avrispmkII -Ulock:w:$(LOCK):m

flash: $(HEXFILE)
	$(AVRDUDE) -p $(AVRDUDE_CPU) -P usb -c avrispmkII -Uflash:w:$(HEXFILE) -B 1.0 -F

reset:
	$(AVRDUDE) -p $(AVRDUDE_CPU) -P usb -c avrispmkII -B 1.0 -F

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<
