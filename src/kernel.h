//
// kernel.h
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2014  R. Stange <rsta2@o2online.de>
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
#ifndef _kernel_h
#define _kernel_h

//#include <circle/memory.h>
//#include <circle/actled.h>
//#include <circle/devicenameservice.h>
//#include <circle/koptions.h>
//#include <circle/screen.h>
//#include <circle/serial.h>
//#include <circle/exceptionhandler.h>
//#include <circle/interrupt.h>
//#include <circle/logger.h>
//#include <circle/types.h>
//#include <circle/gpiomanager.h>
#include "emmc.h"
//#include <SDCard/emmc.h>
//#include <circle/fs/fat/fatfs.h>
//#include <circle/multicore.h>

//#include <circle/usb/dwhcidevice.h>

#include "iec_bus.h"
#include "iec_commands.h"


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
	~CKernel (void);

	bool Initialize (void);

	TShutdownMode Run (void);


//private:
//	static void KeyPressedHandler(const char *pString);
//	static void ShutdownHandler(void);
//
//	static void KeyStatusHandlerRaw(unsigned char ucModifiers, const unsigned char RawKeys[6]);

private:

	// do not change this order
	CEMMCDevice		m_EMMC;

	IEC_Commands m_IEC_Commands;
};

#endif
