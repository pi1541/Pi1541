OBJS	= armc-start.o armc-cstartup.o armc-cstubs.o armc-cppstubs.o \
	exception.o main.o rpi-aux.o rpi-i2c.o rpi-mailbox-interface.o rpi-mailbox.o \
	rpi-gpio.o rpi-interrupts.o dmRotary.o cache.o ff.o interrupt.o Keyboard.o performance.o \
	Drive.o Pi1541.o DiskImage.o iec_bus.o iec_commands.o m6502.o m6522.o \
	gcr.o prot.o lz.o emmc.o diskio.o options.o Screen.o SSD1306.o ScreenLCD.o \
	Timer.o FileBrowser.o DiskCaddy.o ROMs.o InputMappings.o xga_font_data.o m8520.o wd177x.o Pi1581.o SpinLock.o

SRCDIR   = src
OBJS    := $(addprefix $(SRCDIR)/, $(OBJS))

LIBS     = uspi/libuspi.a
INCLUDE  = -Iuspi/include/

TARGET  ?= kernel

.PHONY: all $(LIBS)

all: $(TARGET)

$(TARGET): $(OBJS) $(LIBS)
	@echo "  LINK $@"
	$(Q)$(CC) $(CFLAGS) -o $(TARGET).elf -Xlinker -Map=$(TARGET).map -T linker.ld -nostartfiles $(OBJS) $(LIBS)
	$(Q)$(PREFIX)objdump -d $(TARGET).elf | $(PREFIX)c++filt > $(TARGET).lst
	$(Q)$(PREFIX)objcopy $(TARGET).elf -O binary $(TARGET).img

uspi/libuspi.a:
	$(MAKE) -C uspi

clean:
	$(Q)$(RM) $(OBJS) $(TARGET).elf $(TARGET).map $(TARGET).lst $(TARGET).img
	$(MAKE) -C uspi clean

include Makefile.rules