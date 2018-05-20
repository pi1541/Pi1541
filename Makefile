OBJS	= armc-start.o armc-cstartup.o armc-cstubs.o armc-cppstubs.o \
	exception.o main.o rpi-aux.o rpi-mailbox-interface.o rpi-mailbox.o \
	rpi-gpio.o rpi-interrupts.o cache.o ff.o interrupt.o Keyboard.o \
	Pi1541.o DiskImage.o iec_bus.o iec_commands.o m6502.o m6522.o \
	Drive.o gcr.o prot.o lz.o emmc.o diskio.o options.o Screen.o \
	Timer.o FileBrowser.o DiskCaddy.o ROMs.o InputMappings.o xga_font_data.o

LIBS    = uspi/lib/libuspi.a
INCLUDE  = -Iuspi/include/

TARGET  ?= kernel

.PHONY: all $(LIBS)

all: $(TARGET)

$(TARGET): $(OBJS) $(LIBS)
	@echo "  LINK $@"
	$(Q)$(CC) $(CFLAGS) -o $(TARGET).elf -Xlinker -Map=$(TARGET).map -T linker.ld -nostartfiles $(OBJS) $(LIBS)
	$(Q)$(PREFIX)objdump -d $(TARGET).elf | $(PREFIX)c++filt > $(TARGET).lst
	$(Q)$(PREFIX)objcopy $(TARGET).elf -O binary $(TARGET).img

uspi/lib/libuspi.a:
	$(MAKE) -C uspi/lib

clean: clean_libs

clean_libs:
	$(MAKE) -C uspi/lib clean

include Makefile.rules