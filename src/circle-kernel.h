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

#include <circle/actled.h>
#include <circle/koptions.h>
#include <circle/devicenameservice.h>
#include <circle/screen.h>
#include <circle/serial.h>
#include <circle/exceptionhandler.h>
#include <circle/interrupt.h>
#include <circle/timer.h>
#include <circle/logger.h>
#include <circle/types.h>
#include <circle/usb/usbhcidevice.h>
#include <circle/sched/scheduler.h>
#include <SDCard/emmc.h>
#include <fatfs/ff.h>
#include <wlan/bcm4343.h>
#include <wlan/hostap/wpa_supplicant/wpasupplicant.h>
#include <circle/net/netsubsystem.h>

enum TShutdownMode
{
	ShutdownNone,
	ShutdownHalt,
	ShutdownReboot
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
	
private:
	CActLED			m_ActLED;
	CKernelOptions		mOptions;
	CDeviceNameService	mDeviceNameService;
	CScreenDevice		mScreen;
	CSerialDevice		mSerial;
	CExceptionHandler	mExceptionHandler;
	CInterruptSystem	mInterrupt;
	CTimer			mTimer;
	CLogger			mLogger;
	CScheduler		mScheduler;
	CUSBHCIDevice		m_USBHCI;
	CEMMCDevice		m_EMMC;
	FATFS			m_FileSystem;
	CBcm4343Device		m_WLAN;
	CSynchronizationEvent	mEvent;
	CNetSubSystem		m_Net;
	CWPASupplicant		m_WPASupplicant;
};

#endif


