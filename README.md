# Pi1541 - Circle ported

This is an optional port of Pi1541 to the current Circle bare metal library (as of Jan. 2024, Version 45.3.1).

Target is to remove all Pi bindings which have a counterpart in Circle and to pimp with more functionalities:
- Webserver to download images
- Pi4/400 and 5 (later)
- ...

Status
------
Currently only tested for
- Raspberry 3B+
- LCD Display SSD1306
- Option A (not support split IECLines) of Pi1541, Option *cannot work* as of now!
- WiFi stats and seeks for a DHCP server, Webserver runs, but one can only control the led so far

GPIO handling is still not yet replaced by its circle counterpart, so most likely P4 (and younger) still won't work.

Not yet working & Todos:
- USB Massstorage
- Option B, split IEC lines
- Rotary Input

- Make checkout and build easier

Credits to Stephen [Pi1541](https://cbm-pi1541.firebaseapp.com/), Rene [circle](https://github.com/rsta2/circle), Stephan [circle-stdlib](https://github.com/smuehlst/circle-stdlib) for the brilliant base packages!

Build
-----
One can build the Version 1.24 (+some minor fixes: LED & Buzzer work, build/works with gcc > 10.x).
The circle-version is built by:


`
mkdir build-pottendo-Pi1541
cd build-pottendo-Pi1541
git clone https://github.com/pottendo/pottendo-Pi1541.git

# Checkout (circle-stdlib)[https://github.com/smuehlst/circle-stdlib]:
git clone --recursive https://github.com/smuehlst/circle-stdlib.git
cd circle-stdlib
./configure -r 3
make

# Set/edit some options in libs/circle/include/circle/sysconfig.h and libs/circle/addon/fatfs/ffconf.h, see src/Circle/patch-circle.diff

cd ../pottendo-Pi1541/src
make -f Makefile.circle
`

# to build the standard Pi1541 you have clean the builds by make clean (toplevel), and make -f Makefile.circle in src

WiFi needs the drivers on the flash card under *firmware/...* and a file wpa_supplicant.conf on the toplevel to configure your SSID.
TODO: add where to get the drivers...

the *config.txt* on the SDCard must not set kernel_address (therefore commented below) as it's needed for the original Pi1541.

`
#kernel_address=0x1f00000
arm_64bit=0
#armstub=no-prefetch.bin 

enable_uart=1
gpu_mem=16

hdmi_group=2
hdmi_mode=16

#kernel=kernel8.img
kernel=kernel.img
`

This config.txt enables the uart console on pins 14/15 - this gives useful log information.
*options.txt* and all the other content on a Pi1541 sdcard are similar to the original

# Pi1541

Commodore 1541/1581 emulator for the Raspberry Pi

Pi1541 is a real-time, cycle exact, Commodore 1541 disk drive emulator that can run on a Raspberry Pi 3A, 3B or 3B+. The software is free and I have endeavored to make the hardware as simple and inexpensive as possible.

Pi1541 provides you with an SD card solution for using D64, G64, NIB and NBZ Commodore disk images on real Commodore 8 bit computers such as;-
Commodore 64
Commodore 128
Commodore Vic20
Commodore 16
Commodore Plus4

See https://cbm-pi1541.firebaseapp.com/ for SD card and hardware configurations.

Toolchain Installation
----------------------

On Windows use GNU Tools ARM Embedded tool chain 5.4:
https://launchpad.net/gcc-arm-embedded/5.0/5-2016-q2-update
and Make:
http://gnuwin32.sourceforge.net/packages/make.htm


On dpkg based linux systems install:
(Tested on osmc/rpi3)
```
apt-get install binutils-arm-none-eabi gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
```

On RHEL/Centos/Fedora systems follow the guide at:
https://web1.foxhollow.ca/?menu=centos7arm
(Tested on Centos7/x64 with GCC7)
https://developer.arm.com/open-source/gnu-toolchain/gnu-rm/downloads/7-2017-q4-major

Building
--------
```
make
```
This will build kernel.img


In order to build the Commodore programs from the `CBM-FileBrowser_v1.6/sources/` directory, you'll need to install the ACME cross assembler, which is available at https://github.com/meonwax/acme/
