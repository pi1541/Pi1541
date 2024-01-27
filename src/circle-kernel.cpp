//
// kernel.cpp
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
// written by pottendo
//
#include "circle-kernel.h"

#include <stdio.h>
#include <cstring>
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <circle/timer.h>
#include <circle/startup.h>
#include <circle/cputhrottle.h>
#include <iostream>
#include <circle/usb/usbmassdevice.h>
#include "options.h"
#include "webserver.h"

#define _DRIVE		"SD:"
#define _FIRMWARE_PATH	_DRIVE "/firmware/"		// firmware files must be provided here
#define _CONFIG_FILE	_DRIVE "/wpa_supplicant.conf"

#define USE_DHCP
#ifndef USE_DHCP
static const u8 IPAddress[]      = {192, 168, 188, 31};
static const u8 NetMask[]        = {255, 255, 255, 0};
static const u8 DefaultGateway[] = {192, 168, 188, 1};
static const u8 DNSServer[]      = {192, 168, 188, 1};
#endif

CCPUThrottle CPUThrottle;
extern CKernel Kernel;

CKernel::CKernel(void) :
	mScreen (mOptions.GetWidth (), mOptions.GetHeight ()),
	mTimer (&mInterrupt),
	mLogger (mOptions.GetLogLevel (), &mTimer), mScheduler(),
	m_USBHCI (&mInterrupt, &mTimer, true),
	m_pKeyboard (0),
	m_EMMC (&mInterrupt, &mTimer, &m_ActLED),
	m_I2c (0, true),
	m_WLAN (_FIRMWARE_PATH),
	m_Net(nullptr),
	m_WPASupplicant (_CONFIG_FILE),
	m_MCores(CMemorySystem::Get())
{
	//blink(3);
	//mLogger.Write("pottendo-kern", LogNotice, "CKernel Constructor...");
	boolean bOK = TRUE;
	if (bOK) {
		bOK = mScreen.Initialize ();
	} else {
		//m_ActLED.Blink(2);
	}
	if (bOK) {
		bOK = mSerial.Initialize (115200);
	} else {
		//m_ActLED.Blink(5);
	}
	if (bOK)
	{
		CDevice *pTarget = m_DeviceNameService.GetDevice (mOptions.GetLogDevice (), FALSE);
		if (pTarget == 0)
			pTarget = &mScreen;
		bOK = mLogger.Initialize (&mSerial);
	} 
	strcpy(ip_address, "<not assigned>");
}

boolean CKernel::Initialize (void) 
{
	boolean bOK = TRUE;
	if (bOK) bOK = mInterrupt.Initialize ();
	mLogger.Write ("pottendo-kern", LogNotice, "Interrupt done");
	if (bOK) bOK = mTimer.Initialize ();
	mLogger.Write ("pottendo-kern", LogNotice, "mTimer done");
	if (bOK) bOK = m_USBHCI.Initialize ();
	mLogger.Write ("pottendo-kern", LogNotice, "USBHCI done");
	if (bOK) bOK = m_EMMC.Initialize ();
	mLogger.Write ("pottendo-kern", LogNotice, "EMMC done");
	if (bOK) 
	{ 
		if (f_mount (&m_FileSystem, _DRIVE, 1) != FR_OK)
		{
			mLogger.Write ("pottendo-kern", LogError,
					"Cannot mount drive: %s", _DRIVE);
			bOK = FALSE;
		} else {
			mLogger.Write ("pottendo-kern", LogNotice, "mounted drive: " _DRIVE);
		}
	}
	if (bOK) bOK = m_I2c.Initialize();
	mLogger.Write ("pottendo-kern", LogNotice, "I2C done");
	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	mLogger.Write ("pottendo-kern", LogNotice, "pottendo-Pi1541 (%dx%d)", mScreen.GetWidth(), mScreen.GetHeight());
	kernel_main(0, 0, 0);
	new_ip = true;
	Kernel.launch_cores();
	UpdateScreen();
	log("unexpected return of display thread");
	return ShutdownHalt;
}

void CKernel::log(const char *fmt, ...)
{
    char t[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(t, 256, fmt, args);
	mLogger.Write("pottendo-log:", LogNotice, t);
}

boolean CKernel::init_screen(u32 widthDesired, u32 heightDesired, u32 colourDepth, u32 &width, u32 &height, u32 &bpp, u32 &pitch, u8** framebuffer)
{
	Kernel.log("init_screen for %dx%dx%d", widthDesired, heightDesired, colourDepth);
	width = mScreen.GetWidth();
	height = mScreen.GetHeight();
	Kernel.log("screen %dx%d", width, height);
	if (mScreen.GetFrameBuffer() == nullptr)
		return false;
	*framebuffer = (u8 *)(mScreen.GetFrameBuffer());
	bpp = mScreen.GetFrameBuffer()->GetDepth();
	pitch = mScreen.GetFrameBuffer()->GetPitch();
	Kernel.log("bpp=%d, pitch=%d, fb=%p", bpp, pitch, *framebuffer);
	return true;
}

bool CKernel::run_ethernet(void)
{
	bool bOK;
	int retry;
	if (m_Net)
		delete m_Net;
	Kernel.log("Initializing ethernet network");
#ifndef USE_DHCP
	m_Net = new CNetSubSystem(IPAddress, NetMask, DefaultGateway, DNSServer, DEFAULT_HOSTNAME, NetDeviceTypeWLAN);
#else
	m_Net = new CNetSubSystem(0, 0, 0, 0, DEFAULT_HOSTNAME, NetDeviceTypeEthernet);
#endif
	bOK = m_Net->Initialize(FALSE);
	if (!bOK) {
		log("couldn't start ethernet network...waiting 1s"); 
		mScheduler.MsSleep (1 * 1000);
		return bOK;
	}
	retry = 501;	// try for 500*100ms, same as Wifi
	while (--retry && !m_Net->IsRunning()) 
		mScheduler.MsSleep(100);
	return (retry != 0);
}

bool CKernel::run_wifi(void) 
{
	if (m_Net)
	{
		log("%s: cleaning up network stack, may crash here...", __FUNCTION__);
		delete m_Net; m_Net = nullptr;
	}
#ifndef USE_DHCP
	m_Net = new CNetSubSystem(IPAddress, NetMask, DefaultGateway, DNSServer, DEFAULT_HOSTNAME, NetDeviceTypeWLAN);
#else
	m_Net = new CNetSubSystem(0, 0, 0, 0, DEFAULT_HOSTNAME, NetDeviceTypeWLAN);
#endif
	if (!m_Net) return false;
	bool bOK = true;
	if (bOK) bOK = m_WLAN.Initialize();
	if (bOK) bOK = m_Net->Initialize(FALSE);
	if (bOK) bOK = m_WPASupplicant.Initialize();
	if (!bOK) {
		log("couldn't start wifi network...waiting 5s"); 
		mScheduler.MsSleep (5 * 1000);	
	}
	return bOK;
}

void CKernel::run_webserver(void) 
{
	CString IPString;
	while (!m_Net->IsRunning())
	{
		log("webserver waits for network... waiting 5s");
		mScheduler.MsSleep (5000);
	}
	m_Net->GetConfig()->GetIPAddress()->Format (&IPString);
	log ("Open \"http://%s/\" in your web browser!", (const char *) IPString);
	memcpy(ip_address, (const char *) IPString, strlen((const char *) IPString) + 1);
	new_ip = true;
	mScheduler.MsSleep (1000);/* wait a bit, LCD output */
	DisplayMessage(0, 24, true, (const char*) IPString, 0xffffff, 0x0);
	new CWebServer (m_Net, &m_ActLED);
	for (unsigned nCount = 0; 1; nCount++)
	{
		mScheduler.MsSleep (100);
		mScreen.Rotor (0, nCount);
	}
}

void CKernel::i2c_setclock(int BSCMaster, int clock_freq)
{
	m_I2c.SetClock(clock_freq);
}

int CKernel::i2c_read(int BSCMaster, unsigned char slaveAddress, void* buffer, unsigned count)
{
	return m_I2c.Read(slaveAddress, buffer, count);
}

int CKernel::i2c_write(int BSCMaster, unsigned char slaveAddress, void* buffer, unsigned count)
{
	return m_I2c.Write(slaveAddress, buffer, count);
}

int CKernel::i2c_scan(int BSCMaster, unsigned char slaveAddress)
{
	int found = 0;
	u8 t[1];
	if (m_I2c.Read(slaveAddress, t, 1) >= 0) 
	{
		log("identified I2C slave on address 0x%02x", slaveAddress);
		found++;
	}
	return found;
}

void KeyboardRemovedHandler(CDevice *pDevice, void *pContext) 
{
	extern bool USBKeyboardDetected;
	USBKeyboardDetected = false;
	Kernel.get_kbd()->UnregisterKeyStatusHandlerRaw();
	Kernel.set_kbd(nullptr);
	Kernel.log("keyboard removed");
}

/* Ctrl-Alt-Del*/
void KeyboardShutdownHandler(void)
{
	Kernel.log("Ctrl-Alt-Del pressed, rebooting...");
	reboot();
}

int CKernel::usb_keyboard_available(void) 
{
	//if (m_pKeyboard) return 1;
	m_pKeyboard = (CUSBKeyboardDevice *) m_DeviceNameService.GetDevice ("ukbd1", FALSE);
	if (!m_pKeyboard)
		return 0;
	m_pKeyboard->RegisterRemovedHandler(KeyboardRemovedHandler);
	m_pKeyboard->RegisterShutdownHandler(KeyboardShutdownHandler);
	return 1;
}

int CKernel::usb_massstorage_available(void)
{
	pUMSD1 = m_DeviceNameService.GetDevice ("umsd1", TRUE);
	if (pUMSD1 == 0)
	{
		log("USB mass storage device not found");
		return 0;
	}
	return 1;
}

void monitorhandler(TSystemThrottledState CurrentState, void *pParam)
{
	if (CurrentState & SystemStateUnderVoltageOccurred)
	{
		Kernel.log("%s: undervoltage occured...", __FUNCTION__);
	}
	if (CurrentState & SystemStateFrequencyCappingOccurred)
	{
		Kernel.log("%s: frequency capping occured...", __FUNCTION__);
	}
	if (CurrentState & SystemStateThrottlingOccurred)
	{
		Kernel.log("%s: throttling occured to %dMHz", __FUNCTION__, CPUThrottle.GetClockRate() / 1000000L);
	}
	if (CurrentState & SystemStateSoftTempLimitOccurred)
	{
		Kernel.log("%s: softtemplimit occured...", __FUNCTION__);
	}
}

#include <circle/gpiopin.h>

void CKernel::run_tempmonitor(void)
{
    unsigned tmask = SystemStateUnderVoltageOccurred | SystemStateFrequencyCappingOccurred |
					 SystemStateThrottlingOccurred | SystemStateSoftTempLimitOccurred;
	CPUThrottle.RegisterSystemThrottledHandler(tmask, monitorhandler, nullptr);
	// CPUThrottle.DumpStatus(true);
	log("Maximum temp set to %d, dynamic adaption%spossible, curret freq = %dMHz (max=%dMHz)", 
			CPUThrottle.GetMaxTemperature(), 
			CPUThrottle.IsDynamic() ? " " : " not ",
			CPUThrottle.GetClockRate() / 1000000L, 
			CPUThrottle.GetMaxClockRate() / 1000000L);
	//if (CPUThrottle.SetSpeed(CPUSpeedMaximum, true) != CPUSpeedUnknown)
	//	log ("maxed freq to %dMHz", __FUNCTION__, CPUThrottle.GetClockRate() / 1000000L);
	log("ARM_GPIO_GPFSEL1 = 0x%08x", ARM_GPIO_GPFSEL1);
	log("ARM_GPIO_GPSET0 = 0x%08x", ARM_GPIO_GPSET0);
	log("ARM_GPIO_GPCLR0 = 0x%08x", ARM_GPIO_GPCLR0);

	while (true) {
		if (CPUThrottle.SetOnTemperature() == false)
			log("temperature monitor failed...");
		MsDelay(5 * 1000);
		log("Temperature = %dC, IO is 0x%08x", CPUThrottle.GetTemperature(), CGPIOPin::ReadAll()); 
	}
}

TKernelTimerHandle CKernel::timer_start(unsigned delay, TKernelTimerHandler *pHandler, void *pParam, void *pContext)
{
	return mTimer.StartKernelTimer(delay, pHandler, pParam, pContext);
}

void Pi1541Cores::Run(unsigned int core)			/* Virtual method */
{
	extern Options options;
	int i = 0;
	switch (core) {
	case 1:
		Kernel.log("launching emulator on core %d", core);
		emulator();
		break;
	case 2:
		if (!options.GetNetWifi() && !options.GetNetEthernet()) goto out;
		if (options.GetNetEthernet()) // cable network has priority over Wifi
		{
			if (!Kernel.run_ethernet()) {
				Kernel.log("setup ethernet failed");
				i = 0;
			} else i = 1;
		} 
		if ((i == 0) && options.GetNetWifi()) 
		{
			i = 10;
			do {
				Kernel.log("attempt %d to launch WiFi on core %d", 11 - i, core);
			} while (i-- && !Kernel.run_wifi());
		}
		if (i == 0) 
		{
			Kernel.log("network setup failed, giving up");
		} 
		else 
		{
			Kernel.log("launching webserver on core %d", core);
			Kernel.run_webserver();
		}
	out:
		Kernel.log("disabling network support");
		break;
	case 3:	/* health monitoring */
		Kernel.log("launching system monitoring on core %d", core);
		Kernel.run_tempmonitor();
		break;
	default:
		break;
	}
}

