#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
# 
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
# 
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# for Circle build, the RASPPI variable is fetched from Circle lib located at ../circle-stdlib/libs/circle/Config.mk
# -> make sure this circle-stdlib exists at this path, is configured and built
# to build
# 	make
#
# results in
#   kernel8-32.img for Raspberry 3 and Zero 2W
#   kernel7l.img for Raspberry 4

# for Legacy build:
# use RASPPI = 1BRev1 for Raspberry Pi 1B Rev 1 (26 IOports) (GPIO0/1/21)
# use RASPPI = 1BRev2 for Raspberry Pi 1B Rev 2 (26 IOports) (GPIO2/3/27)
# use RASPPI = 1BPlus for Raspberry Pi 1B+ (40 I/OPorts)
# use RASPPI = 0 for Raspberry Pi Zero
# use RASPPI = 3 for Raspberry Pi Zero 2W or Raspberry Pi 3
# use V = 1 optionally for verbose build
# e.g.
# 	make RASPPI=1BPlus V=1
#
# if you switch from legacy build to circle build 'make clean' is mandatory
#

CIRCLEBASE ?= ../circle-stdlib
CIRCLEHOME ?= $(CIRCLEBASE)/libs/circle

LEGACY_OBJS = 	armc-start.o armc-cstartup.o armc-cstubs.o armc-cppstubs.o emmc.o ff.o cache.o exception.o performance.o \
	      	rpi-interrupts.o Timer.o diskio.o interrupt.o rpi-aux.o  rpi-i2c.o rpi-mailbox-interface.o rpi-mailbox.o rpi-gpio.o dmRotary.o

CIRCLE_OBJS = 	circle-main.o circle-kernel.o webserver.o legacy-wrappers.o

COMMON_OBJS = 	main.o Drive.o Pi1541.o DiskImage.o iec_bus.o iec_commands.o m6502.o m6522.o \
		gcr.o prot.o lz.o options.o Screen.o SSD1306.o ScreenLCD.o \
		FileBrowser.o DiskCaddy.o ROMs.o InputMappings.o xga_font_data.o \
		m8520.o wd177x.o Pi1581.o SpinLock.o Keyboard.o
SRCDIR   = src
OBJS_CIRCLE  := $(addprefix $(SRCDIR)/, $(CIRCLE_OBJS) $(COMMON_OBJS))
OBJS_LEGACY  := $(addprefix $(SRCDIR)/, $(LEGACY_OBJS) $(COMMON_OBJS))

LIBS     = uspi/libuspi.a
INCLUDE  = -Iuspi/include/

ifeq ($(RASPPI),)
include $(CIRCLEHOME)/Config.mk
ifeq ($(strip $(RASPPI)),3)
TARGET_CIRCLE ?= kernel8-32.img
else ifeq ($(strip $(RASPPI)),4)
TARGET_CIRCLE ?= kernel7l.img
else
$(error Circle build only for RASPPI 3, Zero 2W or 4)
endif
else
include Makefile.rules
endif

TARGET ?= kernel
.PHONY: all circlebuild $(LIBS)

all: $(TARGET_CIRCLE)

legacy: $(TARGET)

$(TARGET_CIRCLE): circlebuild
	$(MAKE) -C $(SRCDIR) -f Makefile.circle COMMON_OBJS="$(COMMON_OBJS)" CIRCLE_OBJS="$(CIRCLE_OBJS)"
	@cp $(SRCDIR)/$@ .

$(TARGET): $(OBJS_LEGACY) $(LIBS)
	@echo "  LINK $@"
	$(Q)$(CC) $(CFLAGS) -o $(TARGET).elf -Xlinker -Map=$(TARGET).map -T linker.ld -nostartfiles $(OBJS_LEGACY) $(LIBS)
	$(Q)$(PREFIX)objdump -d $(TARGET).elf | $(PREFIX)c++filt > $(TARGET).lst
	$(Q)$(PREFIX)objcopy $(TARGET).elf -O binary $(TARGET).img

uspi/libuspi.a:
	$(MAKE) -C uspi

clean:
	$(Q)$(RM) $(OBJS_LEGACY) $(OBJS_CIRCLE) $(TARGET).elf $(TARGET).map $(TARGET).lst $(TARGET).img $(TARGET_CIRCLE)
	$(MAKE) -C uspi clean
	$(MAKE) -C $(SRCDIR) -f Makefile.circle clean

