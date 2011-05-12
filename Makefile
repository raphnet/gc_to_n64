CC=avr-gcc
AS=$(CC)
LD=$(CC)

CPU=atmega8
UISP=uisp -dprog=stk500 -dpart=atmega8 -dserial=/dev/avr
CFLAGS=-Wall -mmcu=$(CPU) -DF_CPU=16000000L -Os
LDFLAGS=-mmcu=$(CPU) -Wl,-Map=n64_to_wii.map
HEXFILE=n64_to_wii.hex
AVRDUDE=avrdude
AVRDUDE_CPU=m8
#AVRDUDE_CPU=m88

OBJS=main.o gamecube.o support.o n64_isr.o mapper.o gamecube_mapping.o n64_mapping.o buzzer.o timer0.o eeprom.o

all: $(HEXFILE)

clean:
	rm -f n64_to_wii.elf n64_to_wii.hex n64_to_wii.map $(OBJS)

n64_to_wii.elf: $(OBJS)
	$(LD) $(OBJS) $(LDFLAGS) -o n64_to_wii.elf

n64_to_wii.hex: n64_to_wii.elf
	avr-objcopy -j .data -j .text -O ihex n64_to_wii.elf n64_to_wii.hex
	avr-size n64_to_wii.elf

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
