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
extern Options options;

CKernel::CKernel(void) :
	mScreen (mOptions.GetWidth (), mOptions.GetHeight ()),
	mTimer (&mInterrupt),
	mLogger (mOptions.GetLogLevel (), &mTimer), mScheduler(),
	m_USBHCI (&mInterrupt, &mTimer, true),
	m_pKeyboard (0),
	m_EMMC (&mInterrupt, &mTimer, &m_ActLED),
	m_I2c (0, true),
#if RASPPI <= 4
	m_PWMSoundDevice (nullptr),
#endif	
	m_WLAN (_FIRMWARE_PATH),
	m_Net(nullptr),
	m_WPASupplicant (_CONFIG_FILE),
	m_MCores(CMemorySystem::Get()),
	screen_failed(false),
	no_pwm(false)
{
	if (mScreen.Initialize() == false) 
	{
		m_ActLED.Blink(2);
		screen_failed = true;
	}
	if (mSerial.Initialize (115200) == true)
		mLogger.Initialize (&mSerial);
	else
		m_ActLED.Blink(3);

	if (screen_failed)
		log("screen initialization failed...  trying headless");
	strcpy(ip_address, "<n/a>");
}

boolean CKernel::Initialize (void) 
{
	boolean bOK;
	if (bOK = mInterrupt.Initialize ())
	mLogger.Write ("pottendo-kern", LogNotice, "Interrupt %s", bOK ? "ok" : "failed");
	if (bOK) bOK = mTimer.Initialize ();
	mLogger.Write ("pottendo-kern", LogNotice, "mTimer %s", bOK ? "ok" : "failed");
	if (bOK) bOK = m_USBHCI.Initialize ();
	mLogger.Write ("pottendo-kern", LogNotice, "USBHCI %s", bOK ? "ok" : "failed");
	bOK = true;	/* USB may not be needed */
	if (bOK) bOK = m_EMMC.Initialize ();
	mLogger.Write ("pottendo-kern", LogNotice, "EMMC %s", bOK ? "ok" : "failed");
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
	mLogger.Write ("pottendo-kern", LogNotice, "I2C %s", bOK ? "ok" : "failed");
	return bOK;
}

TShutdownMode CKernel::Run (void)
{
	mLogger.Write ("pottendo-kern", LogNotice, "pottendo-Pi1541 (%dx%d)", mScreen.GetWidth(), mScreen.GetHeight());

	if (CPUThrottle.SetSpeed(CPUSpeedMaximum, true) != CPUSpeedUnknown)
	{	
		log("maxed freq to %dMHz", CPUThrottle.GetClockRate() / 1000000L);
	}

	CMachineInfo *mi = CMachineInfo::Get();
	if (mi)
	{
		TMachineModel model = mi->GetMachineModel();
		if (model == MachineModelZero2W)
		{
			// disable PWM sound
			log("%s won't support PWM sound, disabling it", mi->GetMachineName());
			no_pwm = true;
		}
		switch (model) {
			case MachineModelZero2W:
				// disable PWM sound

				break;
			case MachineModel3B:
			case MachineModel3BPlus:
			case MachineModel4B:
				m_PWMSoundDevice = new CPWMSoundDevice(&mInterrupt);
				if (!m_PWMSoundDevice)
					no_pwm = true;
				break;
			default:
				log ("model '%s' not tested, use at your onw risk...", mi->GetMachineName());
				MsDelay(2000);
				break;
		}
	}

	kernel_main(0, 0, 0);/* options will be initialized */
	new_ip = true;
	if (screen_failed)
		options.SetHeadLess(1);
	Kernel.launch_cores();
	if (options.GetHeadLess() == false)
	{
		UpdateScreen();
		log("unexpected return of display thread");
	} 
	else 
	{
		log("running headless...");
		while (true) {
			MsDelay(1000 * 3600);
		}
	}
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
	if (screen_failed) return false;
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
	{
		log("%s: cleaning up network stack...", __FUNCTION__);
		delete m_Net;
	}
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
		log("%s: cleaning up network stack...", __FUNCTION__);
		delete m_Net; m_Net = nullptr;
	}
#ifndef USE_DHCP
	m_Net = new CNetSubSystem(IPAddress, NetMask, DefaultGateway, DNSServer, DEFAULT_HOSTNAME, NetDeviceTypeWLAN);
#else
	m_Net = new CNetSubSystem(0, 0, 0, 0, DEFAULT_HOSTNAME, NetDeviceTypeWLAN);
#endif
	if (!m_Net) return false;
	bool bOK = true;
	if (bOK) bOK = m_Net->Initialize(FALSE);
	if (bOK) bOK = m_WLAN.Initialize();
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
	m_Net->GetConfig()->GetIPAddress()->Format(&IPString);
	strcpy(ip_address, (const char *) IPString);
	log ("Open \"http://%s/\" in your web browser!", ip_address);
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
	Kernel.log("%s - state = 0x%04x", __FUNCTION__, CurrentState);
	if (CurrentState & SystemStateUnderVoltageOccurred) {
		Kernel.log("%s: undervoltage occured...", __FUNCTION__);
	}
	if (CurrentState & SystemStateFrequencyCappingOccurred) {
		Kernel.log("%s: frequency capping occured...", __FUNCTION__);
	}
	if (CurrentState & SystemStateThrottlingOccurred) {
		Kernel.log("%s: throttling occured to %dMHz", __FUNCTION__, CPUThrottle.GetClockRate() / 1000000L);
	}
	if (CurrentState & SystemStateSoftTempLimitOccurred) {
		Kernel.log("%s: softtemplimit occured...", __FUNCTION__);
	}
}

void CKernel::run_tempmonitor(void)
{
    unsigned tmask = SystemStateUnderVoltageOccurred | SystemStateFrequencyCappingOccurred |
					 SystemStateThrottlingOccurred | SystemStateSoftTempLimitOccurred;
	CPUThrottle.RegisterSystemThrottledHandler(tmask, monitorhandler, nullptr);
	CPUThrottle.Update();
	// CPUThrottle.DumpStatus(true);
	while (true) {
		if (CPUThrottle.SetOnTemperature() == false)
			log("temperature monitor failed...");
		MsDelay(5 * 1000);
		if (!CPUThrottle.Update())
			log("CPUThrottle Update failed");
		log("Temp %dC (max=%dC), dynamic adaption%spossible, current freq = %dMHz (max=%dMHz)", 
			CPUThrottle.GetTemperature(),
			CPUThrottle.GetMaxTemperature(), 
			CPUThrottle.IsDynamic() ? " " : " not ",
			CPUThrottle.GetClockRate() / 1000000L, 
			CPUThrottle.GetMaxClockRate() / 1000000L);
	}
}

TKernelTimerHandle CKernel::timer_start(unsigned delay, TKernelTimerHandler *pHandler, void *pParam, void *pContext)
{
	return mTimer.StartKernelTimer(delay, pHandler, pParam, pContext);
}

#include "sample.h"
//extern const long int Sample_bin_size;
//extern const unsigned char *Sample_bin;
void CKernel::playsound(void)
{
	if (no_pwm) return;
#if RASPPI <= 4
	if (m_PWMSoundDevice->PlaybackActive())
		return;
	m_PWMSoundDevice->Playback ((void *)Sample_bin, Sample_bin_size, 1, 8);
#endif	
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

