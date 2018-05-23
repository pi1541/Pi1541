# Pi1541

Commodore 1541 emulator for the Raspberry Pi

Pi1541 is a real-time, cycle exact, Commodore 1541 disk drive emulator that can run on a Raspberry Pi 3B (or 3B+). The software is free and I have endeavored to make the hardware as simple and inexpensive as possible.

Pi1541 provides you with an SD card solution for using D64, G64, NIB and NBZ Commodore disk images on real Commodore 8 bit computers such as;-
Commodore 64
Commodore 128
Commodore Vic20
Commodore 16
Commodore Plus4

See www.pi1541.com for SD card and hardware configurations.

Building
--------

On Windows use GNU Tools ARM Embedded tool chain 5.4.1 using make.
https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads/5-2016-q2-update

On dpkg based linux systems install:
binutils-arm-none-eabi
gcc-arm-none-eabi
libnewlib-arm-none-eabi
libstdc++-arm-none-eabi-newlib

```
make
```
This will build kernel.img
