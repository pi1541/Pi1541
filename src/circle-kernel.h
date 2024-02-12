//
// kernel.h
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
#ifndef _circle_kernel_h
#define _circle_kernel_h

#include <circle/multicore.h>
#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#if RASPPI <= 3
#include <circle/spinlock.h>
#endif
#include <circle/memio.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/types.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/usb/usbkeyboard.h>
#include <circle/sched/scheduler.h>
#include <circle/memio.h>
#include <SDCard/emmc.h>
#include <circle/i2cmaster.h>
#include <circle/sound/pwmsounddevice.h>
#include <fatfs/ff.h>
#include <wlan/bcm4343.h>
#include <wlan/hostap/wpa_supplicant/wpasupplicant.h>
#include <circle/net/netsubsystem.h>
#include <stdio.h>

enum TShutdownMode
{
	ShutdownNone,
	ShutdownHalt,
	ShutdownReboot
};

class Pi1541Cores : public CMultiCoreSupport {
public:
	Pi1541Cores(CMemorySystem *pMemorySystem) : CMultiCoreSupport(pMemorySystem) {}
	virtual ~Pi1541Cores(void) = default;

	virtual void Run(unsigned int); 
};

class CKernel
{
public:
	CKernel (void);
	boolean Initialize (void);
	TShutdownMode Run (void);
	int Cleanup (void) { return 0; }
	inline void set_pixel(unsigned int x, unsigned int y, TScreenColor c) { mScreen.SetPixel(x, y, c); }

	CInterruptSystem *get_isys(void) { return &mInterrupt; }
	CTimer *get_timer(void) { return &mTimer; }
	CLogger *get_logger(void) { return &mLogger; }
	CScreenDevice *get_scrdevice(void) { return &mScreen; }
    CEMMCDevice &get_emmc(void) { return m_EMMC; }

	void blink(int n) { m_ActLED.Blink(n); }
	void tlog(int i) { char x[1024]; sprintf(x, "0x%08x", i); mLogger.Write("tlog:", LogNotice, x); }
	void log(const char *fmt, ...);
	void SetACTLed(int v) { if (v) m_ActLED.On(); else m_ActLED.Off(); }
	boolean init_screen(u32 widthDesired, u32 heightDesired, u32 colourDepth, 
						u32 &width, u32 &height, u32 &bpp, u32 &pitch, u8** framebuffer);
	void launch_cores(void) { m_MCores.Initialize(); }
	void yield(void) { mScheduler.Yield(); }
	bool run_wifi(void);
	bool run_ethernet(void);
	void run_webserver(void);
	void i2c_init(int BSCMaster, int fast);
	void i2c_setclock(int BSCMaster, int clock_freq);
	int i2c_read(int BSCMaster, unsigned char slaveAddress, void* buffer, unsigned count);
	int i2c_write(int BSCMaster, unsigned char slaveAddress, void* buffer, unsigned count);
	int i2c_scan(int BSCMaster, unsigned char slaveAddress);
	bool usb_updatepnp(void) { return m_USBHCI.UpdatePlugAndPlay(); }
	int usb_massstorage_available(void);
	int usb_keyboard_available(void);
	void usb_reghandler(TKeyStatusHandlerRaw *handler) { m_pKeyboard->RegisterKeyStatusHandlerRaw(handler); }
	TKernelTimerHandle timer_start(unsigned delay, TKernelTimerHandler *pHandler, void *pParam = 0, void *pContext = 0);
	void timer_cancel(TKernelTimerHandle handler) { mTimer.CancelKernelTimer(handler); }
	inline bool get_ip(const char **p) { *p = ip_address; return true ; if (new_ip) { new_ip = false; return true; } else return false; }
	void run_tempmonitor(void);
	CUSBKeyboardDevice *get_kbd(void) { return m_pKeyboard; }
	inline void set_kbd(CUSBKeyboardDevice *kbd) { m_pKeyboard = kbd; }
	void playsound(void);
	inline bool screen_available(void) { return screen_failed; }
	
private:
	CActLED				m_ActLED;
	CKernelOptions		mOptions;
	CDeviceNameService	m_DeviceNameService;
	CScreenDevice		mScreen;
	CSerialDevice		mSerial;
	CExceptionHandler	mExceptionHandler;
	CInterruptSystem	mInterrupt;
	CTimer				mTimer;
	CLogger				mLogger;
	CScheduler			mScheduler;
	CUSBHCIDevice		m_USBHCI;
	CUSBKeyboardDevice * volatile m_pKeyboard;
	CDevice 			*pUMSD1;
	CEMMCDevice			m_EMMC;
	CI2CMaster			*m_I2c;
#if RASPPI <= 4	
	CPWMSoundDevice		*m_PWMSoundDevice;
#endif	
	FATFS				m_FileSystem;
	CBcm4343Device		m_WLAN;
	CSynchronizationEvent	mEvent;
	CNetSubSystem		*m_Net;
	CWPASupplicant		m_WPASupplicant;
	Pi1541Cores		 	m_MCores;
	char ip_address[32];
	bool new_ip, screen_failed, no_pwm;
};

extern CKernel Kernel;
extern CCPUThrottle CPUThrottle;

#include "legacy-wrappers.h"

#endif
