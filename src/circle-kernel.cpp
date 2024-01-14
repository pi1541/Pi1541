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
#include <math.h>
#include <stdlib.h>
#include <time.h>
#include <circle/timer.h>
#include <iostream>
#include "webserver.h"

extern int mandel_driver(void);

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

CKernel::CKernel(void) :
	mScreen (mOptions.GetWidth (), mOptions.GetHeight ()),
	mTimer (&mInterrupt),
	mLogger (mOptions.GetLogLevel (), &mTimer), mScheduler(),
	m_USBHCI (&mInterrupt, &mTimer),
	m_EMMC (&mInterrupt, &mTimer, &m_ActLED),
	m_WLAN (_FIRMWARE_PATH),
#ifndef USE_DHCP
	m_Net (IPAddress, NetMask, DefaultGateway, DNSServer, DEFAULT_HOSTNAME, NetDeviceTypeWLAN),
#else
	m_Net (0, 0, 0, 0, DEFAULT_HOSTNAME, NetDeviceTypeWLAN),
#endif
	m_WPASupplicant (_CONFIG_FILE)
{
	//blink(3);
	//mLogger.Write("pottendo-kern", LogNotice, "CKernel Constructor...");
	boolean bOK = TRUE;
	if (bOK) bOK = mScreen.Initialize ();
	if (bOK) bOK = mSerial.Initialize (115200);
	if (bOK)
	{
		CDevice *pTarget = mDeviceNameService.GetDevice (mOptions.GetLogDevice (), FALSE);
		if (pTarget == 0)
			pTarget = &mScreen;
		bOK = mLogger.Initialize (&mSerial);
	}
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
	if (bOK) bOK = m_WLAN.Initialize ();
	if (bOK) bOK = m_Net.Initialize (FALSE);
	if (bOK) bOK = m_WPASupplicant.Initialize ();
	return bOK;
}

extern CKernel Kernel;
extern int mandel_iterate(int);

TShutdownMode CKernel::Run (void)
{
	mLogger.Write ("pottendo-kern", LogNotice, "Mandelbrot Demo (%dx%d)", mScreen.GetWidth(), mScreen.GetHeight());
	while (!m_Net.IsRunning ())
	{
		mScheduler.MsSleep (100);
	}
	CString IPString;
	m_Net.GetConfig ()->GetIPAddress ()->Format (&IPString);
	mLogger.Write ("pottendo-kern", LogNotice, "Open \"http://%s/\" in your web browser!",
			(const char *) IPString);

	new CWebServer (&m_Net, &m_ActLED);
	(void) mandel_iterate(1000*1000);

	mLogger.Write ("pottendo-kern", LogNotice, "Demo finished");

	for (unsigned nCount = 0; 1; nCount++)
	{
		mScheduler.Yield ();
		mScreen.Rotor (0, nCount);
	}

	return ShutdownHalt;
}

void RPiConsole_put_pixel(uint32_t x, uint32_t y, uint16_t c)
{
	Kernel.set_pixel(x, y, c);
}

void CKernel::log(const char *fmt, ...)
{
    char t[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(t, 256, fmt, args);
	mLogger.Write("pottendo-log:", LogNotice, t);
}
void SetACTLed(int v) 
{ 
	Kernel.SetACTLed(v); 
}