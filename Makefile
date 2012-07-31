CC=avr-gcc
AS=$(CC)
LD=$(CC)

CPU=atmega8
UISP=uisp -dprog=stk500 -dpart=atmega8 -dserial=/dev/avr
CFLAGS=-Wall -mmcu=$(CPU) -DF_CPU=16000000L -Os
LDFLAGS=-mmcu=$(CPU) -Wl,-Map=gc_to_n64.map
HEXFILE=gc_to_n64.hex
AVRDUDE=avrdude
AVRDUDE_CPU=m8
#AVRDUDE_CPU=m88

OBJS=main.o gamecube.o n64_isr.o mapper.o gamecube_mapping.o n64_mapping.o buzzer.o timer0.o eeprom.o sync.o lut.o gcn64_protocol.o

all: $(HEXFILE)

clean:
	rm -f gc_to_n64.elf gc_to_n64.hex gc_to_n64.map $(OBJS)

gc_to_n64.elf: $(OBJS)
	$(LD) $(OBJS) $(LDFLAGS) -o gc_to_n64.elf

gc_to_n64.hex: gc_to_n64.elf
	avr-objcopy -j .data -j .text -O ihex gc_to_n64.elf gc_to_n64.hex
	avr-size gc_to_n64.elf

fuse:
	$(UISP) --wr_fuse_h=0xd9 --wr_fuse_l=0xdf --wr_fuse_e=0xf

flash: $(HEXFILE)
	$(UISP) --erase --upload --verify if=$(HEXFILE)

fuse_usb:
	#sudo $(AVRDUDE) -p $(AVRDUDE_CPU) -P usb -c avrispmkII -Uefuse:w:0x07:m -Uhfuse:w:0xd9:m -Ulfuse:w:0xdf:m -B 20.0 -F
	sudo $(AVRDUDE) -p $(AVRDUDE_CPU) -P usb -c avrispmkII -Uhfuse:w:0xd9:m -Ulfuse:w:0xdf:m -B 20.0 -F

flash_usb: $(HEXFILE)
	sudo $(AVRDUDE) -p $(AVRDUDE_CPU) -P usb -c avrispmkII -Uflash:w:$(HEXFILE) -B 1.0 -F
	
%.o: %.c
	$(CC) $(CFLAGS) -c $<

%.o: %.S
	$(CC) $(CFLAGS) -c $<
