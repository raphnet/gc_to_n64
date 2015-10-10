CC=avr-gcc
AS=$(CC)
LD=$(CC)

VERSION=2.0
CPU=atmega168
CFLAGS=-Wall -mmcu=$(CPU) -DF_CPU=16000000L -Os -DVISUAL_BUZZER -DVERSION_STR=\"$(VERSION)\"
LDFLAGS=-mmcu=$(CPU) -Wl,-Map=gc_to_n64.map
HEXFILE=gc_to_n64.hex
AVRDUDE=avrdude
AVRDUDE_CPU=m168

OBJS=main.o gamecube.o n64_isr.o mapper.o gamecube_mapping.o n64_mapping.o buzzer.o timer0.o eeprom.o sync.o lut.o gcn64_protocol.o

all: $(HEXFILE)

clean:
	rm -f gc_to_n64.elf gc_to_n64.hex gc_to_n64.map $(OBJS)

gc_to_n64.elf: $(OBJS)
	$(LD) $(OBJS) $(LDFLAGS) -o gc_to_n64.elf

gc_to_n64.hex: gc_to_n64.elf
	avr-objcopy -j .data -j .text -O ihex gc_to_n64.elf gc_to_n64.hex
	avr-size gc_to_n64.elf

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
# 2: BOOTSZ1  -> 0
# 1: BOOTSZ0  -> 0
# 0: BOOTRST  -> 1
#

fuse:
	$(AVRDUDE) -p $(AVRDUDE_CPU) -P usb -c avrispmkII -Uefuse:w:$(EFUSE):m -Uhfuse:w:$(HFUSE):m -Ulfuse:w:$(LFUSE):m -B 20.0 -F

flash: $(HEXFILE)
	$(AVRDUDE) -p $(AVRDUDE_CPU) -P usb -c avrispmkII -Uflash:w:$(HEXFILE) -B 1.0 -F

reset:
	$(AVRDUDE) -p $(AVRDUDE_CPU) -P usb -c avrispmkII -B 1.0 -F

%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<
