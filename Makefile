#
# Makefile
#

OBJS	= armc-start.o armc-cstartup.o armc-cstubs.o armc-cppstubs.o exception.o main.o rpi-aux.o rpi-mailbox-interface.o rpi-mailbox.o rpi-gpio.o rpi-interrupts.o cache.o ff.o interrupt.o keyboard.o Pi1541.o DiskImage.o iec_bus.o iec_commands.o m6502.o m6522.o drive.o gcr.o prot.o lz.o emmc.o diskio.o options.o Screen.o Timer.o FileBrowser.o DiskCaddy.o ROMs.o InputMappings.o xga_font_data.o

kernel.img: $(OBJS) $(OBJSS)
	$(LD) -o kernel.elf -Map kernel.map -T linker.ld $(OBJS) $(LIBS)
	$(PREFIX)objdump -d kernel.elf | $(PREFIX)c++filt > kernel.lst
	$(PREFIX)objcopy kernel.elf -O binary kernel.img
	wc -c kernel.img

GCC_BASE = "C:/Program Files (x86)/GNU Tools ARM Embedded/5.4 2016q2"

RASPPI	?= 3
PREFIX	?= arm-none-eabi-

CC	= $(PREFIX)gcc
CPP	= $(PREFIX)g++
AS	= $(CC)
LD	= $(PREFIX)ld
AR	= $(PREFIX)ar

ifeq ($(strip $(RASPPI)),0)
#ARCH	?= -march=armv6zk -mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard -DRPIZERO=1 -DDEBUG
ARCH	?= -march=armv6zk -mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard -DRPIZERO=1
CFLAGS	+= -DRPIZERO=1
endif
ifeq ($(strip $(RASPPI)),1)
ARCH	?= -march=armv6zk -mtune=arm1176jzf-s -mfloat-abi=hard -DRPIZERO=1
CFLAGS	+= -DRPIBPLUS=1
endif
ifeq ($(strip $(RASPPI)),2)
ARCH	?= -march=armv7-a -mtune=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -marm -DRPI2=1
CFLAGS	+= -DRPI2=1
endif
ifeq ($(strip $(RASPPI)),3)
ARCH	?= -march=armv8-a -mtune=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard -marm -DRPI3=1 -DDEBUG -DNDEBUG
#ARCH	?= -march=armv8-a -mtune=cortex-a53 -mfpu=neon-fp-armv8 -mfloat-abi=hard -marm -DRPI3=1
CFLAGS	+= -DRPI3=1
endif

LIBS	= $(GCC_BASE)/arm-none-eabi/lib/fpu/libc.a $(GCC_BASE)/lib/gcc/arm-none-eabi/5.4.1/fpu/libgcc.a
LIBS += uspi/lib/libuspi.a -lstdc++

INCLUDE	+= -I $(GCC_BASE)/arm-none-eabi/include -I $(GCC_BASE)/lib/gcc/arm-none-eabi/5.4.1/include 
#INCLUDE	+= -I USB/include
INCLUDE	+= -I uspi/include
#INCLUDE	+= -I uspi/include/uspi

AFLAGS	+= $(ARCH) $(INCLUDE)
CFLAGS	+= $(ARCH) -Wall -Wno-psabi -fsigned-char -fno-builtin  $(INCLUDE) 
#-Wno-packed-bitfield-compat
#CFLAGS	+= -O3
#CFLAGS	+= -O4
CFLAGS	+= -Ofast
CPPFLAGS+= $(CFLAGS) -fno-exceptions -fno-rtti -std=c++0x -Wno-write-strings

CFLAGS	+= -fno-delete-null-pointer-checks -fdata-sections -ffunction-sections -u _printf_float

%.o: %.S
	$(AS) $(AFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

#assm add -S
%.o: %.cpp
	$(CPP) $(CPPFLAGS) -c -o $@ $<

clean:
#	rm -f *.o *.a *.elf *.lst *.img *.cir *.map *~ $(EXTRACLEAN)
	del *.o
	del *.a
	del *.elf
	del *.img


	

#arm-none-eabi-objdump -D kernel.elf > kernel.elf.asm
