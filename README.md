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

I use GNU Tools ARM Embedded tool chain 5.4.1 on Windows using make. https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads/5-2016-q2-update
There are two make files.
One in uspi\lib and Pi1541's make file the root folder.
You will need to edit the make files to set GCC_BASE to the location of your GNU tools.
(If anyone knows how to fix this requirement then please fix it. arm-none-eabi-gcc can find the include paths why can't arm-none-eabi-ld find the library paths?)

You need to build uspi\lib first.
Change to uspi\lib and make.
Change back to the root folder of the project and again make.
This will build kernel.img

