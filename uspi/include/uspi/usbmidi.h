//
// usbmidi.h
//
// USPi - An USB driver for Raspberry Pi written in C
// Copyright (C) 2016  R. Stange <rsta2@o2online.de>
// Copyright (C) 2016  J. Otto <joshua.t.otto@gmail.com>
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
#ifndef _usbmidi_h
#define _usbmidi_h

#include <uspi/usbdevice.h>
#include <uspi/usbendpoint.h>
#include <uspi/usbrequest.h>
#include <uspi/types.h>

typedef void TMIDIPacketHandler(unsigned nCable, unsigned nLength, u8 *pPacket);

typedef struct TUSBMIDIDevice
{
	TUSBDevice m_USBDevice;

	TUSBEndpoint *m_pEndpointIn;

	TMIDIPacketHandler *m_pPacketHandler;

	TUSBRequest *m_pURB;
	u16 m_usBufferSize;
	u8 *m_pPacketBuffer;
}
TUSBMIDIDevice;

void USBMIDIDevice (TUSBMIDIDevice *pThis, TUSBDevice *pDevice);
void _CUSBMIDIDevice (TUSBMIDIDevice *pThis);

boolean USBMIDIDeviceConfigure (TUSBDevice *pUSBDevice);

void USBMIDIDeviceRegisterPacketHandler (TUSBMIDIDevice *pThis, TMIDIPacketHandler *pPacketHandler);

#endif
