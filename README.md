# Pi1541 - Circle ported, ready for new features

This is an optional port of Pi1541 to the current Circle bare metal library (as of Jan. 2024, Version 45.3.1).

As almost all Pi model specific bindings which have a counterparts in Circle have been removed. This allows to use the potential of Circle to extend Pi1541 with new functionalities. Some ideas:
- Webserver to download images
- Pi4/400 and 5 (later, once supported by Circle)
- ...

Credits to Stephen (@pi1541) [Pi1541](https://cbm-pi1541.firebaseapp.com/) and [Pi1541-github](https://github.com/pi1541/Pi1541), Rene (@rsta2) [circle](https://github.com/rsta2/circle), Stephan (@smuehlst) [circle-stdlib](https://github.com/smuehlst/circle-stdlib) for the brilliant base packages!

Status
------
Currently only tested for
- Raspberry models 3B+, PiZero 2W, 4: successful load (JiffyDOS) of some games with fastloaders and GEOS
- LCD Display SSD1306
- Option A HW Support 
- Buzzer sound output 
- PWM/DMA Soundoutput
- USB Keyboard and USB Massstorage, Keyboard supports plug-and-play, Massstorage not fully plug-and-play
- if enabled, Ethernet or WiFi starts and seeks for a DHCP server, Webserver runs, but one can only control the led so far
- Rotary Input

Note that Option B hardware (split IECLines) of Pi1541 is not tested (I don't have the necessay hardware). The code uses `<somePin>.SetMode(GPIOModeInput)` method. This should neither activate _PullUp_ nor _PullDown_ for any of the respective input pins. 
<p>

If enabled (see below), network is activated in the background. For Wifi it may take a few seconds to connect and retreive the IP Address via DHCP.
The IP address is briefly shown on the LCD, once received. One can check the IP address on the screen (HDMI).

<p>

**Attention**: the operating temperature is substantially higher than with the original kernel (legacy build). It is recommended to use _active_ cooling as of now. Raspeberry PIs normally protect themselves through throtteling. This should work at 85C - for some reason I can't lower this threshold via `cmdline.txt` using `socmaxtemp=70`, as this doesn't set the limit as documented [here](https://circle-rpi.readthedocs.io/en/latest/basic-system-services/cpu-clock-rate-management.html#ccputhrottle) - at least not on my RPi3.

TODOs
-----
- Testing of Option B, split IEC lines
- Make the webserver useful
- Allow static IP Adresses for faster startup, to be configured in `options.txt`
- Make execution more efficient wrt. CPU usage to keep temperature lower, use throtteling to protect the Pi.
- Provide a helper script to collect all files to make Pi1541 sdcard build easy
- Test more sophisticated loaders (RT behavior)

What will not come
------------------
- PiZero support for circle, as it doesn't make sense due to lack of network support
- Circle Support for all variants of Pi1 and Pi2, as I don't have those to test

Additional Options in `options.txt`
-----------------------------------
The following options control new functions available:
| Option | Value | Purpose |
|--------|-------|---------|
| netEthernet | 0 or 1 | enable ethernet network|
| netWifi | 0 or 1 | enable network network|

Know Bugs
---------

- Enabling both `netEthernet` and `netWifi` leads to a crash
- Pluging in a USB stick _after_ booting, won't show files on the USB mounted drive and display remains dark. Unplugging/re-plugging works as expected if USB is plugged in at startup

Checkout & Build
----------------
One can build the Version 1.24 (+some minor fixes: LED & Buzzer work, build/works with gcc > 10.x).

The circle-stdlib used by this project is built by:

```
BUILDDIR=build-pottendo-Pi1541
mkdir $BUILDDIR
cd ${BUILDDIR}
git clone https://github.com/pottendo/pottendo-Pi1541.git

# Checkout (circle-stdlib)[https://github.com/smuehlst/circle-stdlib]:
git clone --recursive https://github.com/smuehlst/circle-stdlib.git
cd circle-stdlib
# configure for Pi3 and Pi Zero 2 W:
./configure -r 3
# alternatively configure for Pi4
# ./configure -r 4

# Path Circle sysconfigh on ffconf.h to adapt to Pi1541 needs

cd libs/circle
patch -p1 < ../../../pottendo-Pi1541/src/Circle/patch-circle.diff 
cd ../..

# build circle-lib
make

# build Pi1541 based on circle
cd ${BUILDDIR}/pottendo-Pi1541
```
Depending on the RPi Model and on the chosen build (Circle vs. legacy):
| Model | Version | build cmd | Image Name |
|----------|-----------|----------- |----------------|
| Pi Zero, 1RevXX, 2, 3 | legacy build | `make RASPPI={0,1BRev1,1BRev2,1BPlus,2,3} legacy` | `kernel.img` |
| Pi Zero 2W, 3 | circle build | `make` | `kernel8-32.img` |
| Pi 4 | circle build | `make` | `kernel7l.img` |

*Hint*: in case you want to alternatively build for circle-lib and legacy make sure to `make clean` between the builds!

Now copy the kernel image to your Pi1541 SDCard. Make sure you have set the respective lines `config.txt` on your Pi1541 SDcard:
Model 3 and earlier - `config.txt`
```
arm_freq=1300
over_voltage=4
sdram_freq=500
sdram_over_voltage=1
force_turbo=1
boot_delay=1

# Run in 32-bit mode
arm_64bit=0

enable_uart=1
gpu_mem=16

hdmi_group=2
#hdmi_mode=4
hdmi_mode=16

kernel=kernel8-32.img
#kernel_address=0x1f00000
#kernel=kernel.img

```
in case you use legacy build `kernel.img` you also have to uncomment the line `kernel_address=0x1f00000`!

Model 4 - `config.txt`
```
# some genereic Pi4 configs can remain

# Run in 32-bit mode
arm_64bit=0
force_turbo=1

[all]
enable_uart=1   # in case you have Pin 14/15 connected via TTL cable
kernel=kernel7l.img
```

Uart console on pins *14(TX)/15(RX)* gives useful log information.

Networking
----------
If enabled, WiFi needs the drivers on the flash card. You can download like this:
```
cd ${BUILDDIR}/circle-stdlib/libs/circle/addon/wlan/firmware
make
```
this downloads the necessary files in the current directory.
copy the content to you Pi1541 SDCard in the directlry 
  `firmware/`
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
  `wpa_supplicant.conf`
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

# Disclaimer

**Due to some unlikely, unexpected circumstances (e.g. overheating), you may damage your beloved devices (Raspberry Pi, Retro machines, C64s, VIC20s, SDCards, USBSticks, etc) by using this software. I do not take any responsibility, so use at your own risk!**

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
