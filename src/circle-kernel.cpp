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

CKernel::CKernel(void) :
	mScreen (mOptions.GetWidth (), mOptions.GetHeight ()),
	mTimer (&mInterrupt),
	mLogger (mOptions.GetLogLevel (), &mTimer), mScheduler(),
	m_USBHCI (&mInterrupt, &mTimer, true),
	m_pKeyboard (0),
	m_EMMC (&mInterrupt, &mTimer, &m_ActLED),
	m_I2c (0, true),
	m_WLAN (_FIRMWARE_PATH),
#ifndef USE_DHCP
	m_Net (IPAddress, NetMask, DefaultGateway, DNSServer, DEFAULT_HOSTNAME, NetDeviceTypeWLAN),
#else
	m_Net (0, 0, 0, 0, DEFAULT_HOSTNAME, NetDeviceTypeWLAN),
#endif
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

extern CKernel Kernel;

TShutdownMode CKernel::Run (void)
{
	mLogger.Write ("pottendo-kern", LogNotice, "pottendo-Pi1541 (%dx%d)", mScreen.GetWidth(), mScreen.GetHeight());
	kernel_main(0, 0, 0);
	//	DisplayMessage(0, 0, true, "Connect WiFi...", 0xffffffff, 0x0);
	//run_wifi();
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

bool CKernel::run_wifi(void) 
{
	bool bOK = true;
	CString IPString;
	if (bOK) bOK = m_WLAN.Initialize();
	if (bOK) bOK = m_Net.Initialize(FALSE);
	if (bOK) bOK = m_WPASupplicant.Initialize();
	if (!bOK) {
		log("couldn't start network...waiting 5s"); 
		mScheduler.MsSleep (5 * 1000);	
		return bOK;
	}
	while (!m_Net.IsRunning ())
	{
		mScheduler.MsSleep (100);
	}
	m_Net.GetConfig ()->GetIPAddress ()->Format (&IPString);
	mLogger.Write ("pottendo-kern", LogNotice, "Open \"http://%s/\" in your web browser!",
			(const char *) IPString);
	strncpy(ip_address, (const char *) IPString, 31); ip_address[31] = '\0';
	new_ip = true;
	DisplayMessage(0, 16, true, (const char*) IPString, 0xffffffff, 0x0);
	return bOK;
}

void CKernel::run_webserver(void) 
{
	while (!m_Net.IsRunning ())
	{
		mScheduler.MsSleep (1000);
		log("webserver waits for network...");
	}
	new CWebServer (&m_Net, &m_ActLED);
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
	int i = 0;
	switch (core) {
	case 1:
		Kernel.log("launching emulator on core %d", core);
		emulator();
		break;
	case 2:
		do {
			if (i >= 10) {
				Kernel.log("WiFi setup failed, giving up");
				goto out;
			}
			Kernel.log("attempt %d to launch WiFi on core %d", ++i, core);
		} while (!Kernel.run_wifi());
		Kernel.log("launching webserver on core %d", core);
		Kernel.run_webserver();
	out:		
		break;
	case 3:	/* health monitoring */
		Kernel.log("launching system monitoring on core %d", core);
		Kernel.run_tempmonitor();
		break;
	default:
		break;
	}
}

/* wrappers */
void RPiConsole_put_pixel(uint32_t x, uint32_t y, uint16_t c) {	Kernel.set_pixel(x, y, c); }
void SetACTLed(int v) { Kernel.SetACTLed(v); }
void reboot_now(void) { reboot(); }
void i2c_init(int BSCMaster, int fast) { Kernel.i2c_init(BSCMaster, fast); }
void i2c_setclock(int BSCMaster, int clock_freq) { Kernel.i2c_setclock(BSCMaster, clock_freq); }
int i2c_read(int BSCMaster, unsigned char slaveAddress, void* buffer, unsigned count) { return Kernel.i2c_read(BSCMaster, slaveAddress, buffer, count); }
int i2c_write(int BSCMaster, unsigned char slaveAddress, void* buffer, unsigned count) { return Kernel.i2c_write(BSCMaster, slaveAddress, buffer, count); }
int i2c_scan(int BSCMaster, unsigned char slaveAddress) { return Kernel.i2c_scan(BSCMaster, slaveAddress); }
void USPiInitialize(void) 
{
	if (Kernel.usb_updatepnp() == false) 
	{
		Kernel.log("usb update failed");
	}
}
int USPiKeyboardAvailable(void) { return Kernel.usb_keyboard_available(); }
void USPiKeyboardRegisterKeyStatusHandlerRaw(TKeyStatusHandlerRaw *handler) { Kernel.usb_reghandler(handler); }
TKernelTimerHandle TimerStartKernelTimer(unsigned nDelay, TKernelTimerHandler *pHandler, void* pParam, void* pContext)
{
	return Kernel.timer_start(nDelay, pHandler, pParam, pContext);
}
void TimerCancelKernelTimer(TKernelTimerHandle hTimer) { Kernel.timer_cancel(hTimer); }
int GetTemperature(unsigned &value) { unsigned ret = CPUThrottle.GetTemperature(); if (ret) value = ret * 1000; return ret; }
int USPiMassStorageDeviceAvailable(void) { return Kernel.usb_massstorage_available(); }

