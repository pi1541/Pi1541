# Pi1541 - Circle ported, ready for new features

This is an optional port of Pi1541 to the current Circle bare metal library (as of Jan. 2024, Version 45.3.1).

Target is to remove all Pi bindings which have a counterpart in Circle and to pimp with more functionalities:
- Webserver to download images
- Pi4/400 and 5 (later)
- ...

Credits to Stephen (@pi1541) [Pi1541](https://cbm-pi1541.firebaseapp.com/) and [Pi1541-github](https://github.com/pi1541/Pi1541), Rene (@rsta2) [circle](https://github.com/rsta2/circle), Stephan (@smuehlst) [circle-stdlib](https://github.com/smuehlst/circle-stdlib) for the brilliant base packages!


Status
------
Currently only tested for
- Raspberry 3B+, PiZero 2W: successful load (JiffyDOS) of some games with fastloaders and GEOS
- LCD Display SSD1306
- Option A (not support split IECLines) of Pi1541, **Option B cannot work** as of now!
- Buzzer sound output 
- USB Keyboard and USB Massstorage
- WiFi starts and seeks for a DHCP server, Webserver runs, but one can only control the led so far
GPIO handling is still not yet fully replaced by its circle counterpart, so most likely P4 (and younger) still won't work.

TODOs
-----
- Option B, split IEC lines
- PWM/DMA Soundoutput
- Rotary Input
- Some better output on the LCD and Screen to instruct user: IP address, Status WiFi, etc.
- Make the webserver useful
- Make execution more efficient wrt. CPU usage to keep temperature lower, use throtteling to protect the Pi.

- Make checkout and build easier
- find and fix strict RPI model specific sections, which don't fint to RP4+
- Test more sophisticated loaders (RT behavior)

What will not come
------------------
- PiZero support, as it doesn't make sense due to lack of network support
- Support for all variants of Pi1 and Pi2, as I don't have those to test

Build
-----
One can build the Version 1.24 (+some minor fixes: LED & Buzzer work, build/works with gcc > 10.x).
The circle-version is built by:

```
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
```

In order to build the standard Pi1541 you have clean the builds by 
```
make clean (toplevel)
cd src
make -f Makefile.circle clean
```

WiFi needs the drivers on the flash card. You can download like this:
```
cd ....build-pottendo-Pi1541
cd circle-stdlib/libs/circle/addon/wlan/firmware
make
```
this downloads the necessary files in the directory.\
cp the content to you Pi1541 SDCard in the directlry 
  *firmware/*
it should look like this:
```
brcmfmac43430-sdio.bin
brcmfmac43430-sdio.txt
brcmfmac43436-sdio.bin
brcmfmac43436-sdio.clm_blob
brcmfmac43436-sdio.txt
brcmfmac43455-sdio.bin
brcmfmac43455-sdio.clm_blob
brcmfmac43455-sdio.txt
brcmfmac43456-sdio.bin
brcmfmac43456-sdio.clm_blob
brcmfmac43456-sdio.txt
LICENCE.broadcom_bcm43xx

```
Further you need a file 
  *wpa_supplicant.conf* 
on the toplevel to configure your SSID:
```
#
# wpa_supplicant.conf
#
# adjust your country code
country=AT

network={
    # adjust your SSID
    ssid="REPLACE_WITH_MY_NETWORK_SSI"
    # adjust your WiFi password
    psk="REPLACYE_WITH_MY_WIFI_PASSWORD"
    proto=WPA2
    key_mgmt=WPA-PSK
}
```

the *config.txt* on the SDCard must not set kernel_address (therefore commented below) as it's needed for the original Pi1541.

```
#kernel_address=0x1f00000 # needed for original builds
arm_64bit=0               # 64 bit won't work
#armstub=no-prefetch.bin  # not sure if this is needed

enable_uart=1             # Console 14(TX), 15(RX)
gpu_mem=16

hdmi_group=2
hdmi_mode=16              # 1024x768 @ 60Hz (in group 2)

#kernel=kernel.img        # use this for the original build
kernel=kernel8-32.img     # Circle build default name

```

This config.txt enables the uart console on pins 14(TX)/15(RX) - this gives useful log information.
*options.txt* and all the other content on a Pi1541 sdcard are similar to the original

# Disclaimer

**Due to some unlikely, unexpected circumstances, you may damage your beloved devices (Raspberry Pi, Retro machines, C64s, VIC20s, etc) by using this software. I do not take any responsibility, so use at your own risk!**

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
