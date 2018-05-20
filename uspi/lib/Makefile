#
# Makefile
#
# USPi - An USB driver for Raspberry Pi written in C
# Copyright (C) 2014-2018  R. Stange <rsta2@o2online.de>
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

OBJS	= uspilibrary.o \
	  dwhcidevice.o dwhciregister.o dwhcixferstagedata.o \
	  usbconfigparser.o usbdevice.o usbdevicefactory.o usbendpoint.o usbrequest.o usbstandardhub.o \
	  devicenameservice.o macaddress.o smsc951x.o lan7800.o string.o util.o \
	  usbmassdevice.o \
	  dwhciframeschednper.o dwhciframeschedper.o keymap.o usbkeyboard.o \
	  dwhcirootport.o usbmouse.o \
	  dwhciframeschednsplit.o usbgamepad.o synchronize.o usbstring.o usbmidi.o

all: libuspi.a

libuspi.a: $(OBJS)
#	rm -f libuspi.a
	$(AR) cr libuspi.a $(OBJS)

USPIHOME = ..

GCC_BASE = "C:/Program Files (x86)/GNU Tools ARM Embedded/7 2017-q4-major"

RASPPI	?= 3
PREFIX	?= arm-none-eabi-

CC	= $(PREFIX)gcc
CPP	= $(PREFIX)g++
AS	= $(CC)
LD	= $(PREFIX)ld
AR	= $(PREFIX)ar

ifeq ($(strip $(RASPPI)),0)
ARCH	?= -march=armv6zk -mtune=arm1176jzf-s -mfpu=vfp -mfloat-abi=hard -DRPIZERO=1
CFLAGS	+= -DRPIZERO=1
CFLAGS	+= -DRASPPI=1
endif
ifeq ($(strip $(RASPPI)),1)
ARCH	?= -march=armv6zk -mtune=arm1176jzf-s -mfloat-abi=hard -DRPIZERO=1
CFLAGS	+= -DRPIBPLUS=1
CFLAGS	+= -DRASPPI=1
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

INCLUDE	+= -I $(GCC_BASE)/arm-none-eabi/include -I $(GCC_BASE)/lib/gcc/arm-none-eabi/5.4.1/include -I include
INCLUDE +=  -I $(USPIHOME)/include

AFLAGS	+= $(ARCH) $(INCLUDE)
CFLAGS	+= $(ARCH) -Wall -Wno-psabi -fsigned-char -fno-builtin  $(INCLUDE) 
# -Wno-packed-bitfield-compat
#CFLAGS	+= -O3
#CFLAGS	+= -O4
CFLAGS	+= -Ofast

CPPFLAGS+= $(CFLAGS) -fno-exceptions -fno-rtti -std=c++0x -Wno-write-strings -llibusb

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

	
