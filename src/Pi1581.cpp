// Pi1541 - A Commodore 1541 disk drive emulator
// Copyright(C) 2018 Stephen White
//
// This file is part of Pi1541.
// 
// Pi1541 is free software : you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// Pi1541 is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with Pi1541. If not, see <http://www.gnu.org/licenses/>.

#include "defs.h"
#include "Pi1581.h"
#include "iec_bus.h"
#include "options.h"
#include "ROMs.h"
#include "debug.h"

extern Pi1581 pi1581;
extern u8 s_u8Memory[0xc000];
extern ROMs roms;

// PA0 SIDE0
// PA1 !RDY
// PA2 !MOTOR
// PA3 ID 1
// PA4 ID 2
// PA5 POWER LED
// PA6 ACT LED
// PA7 !DISK_CHNG
// PB0 DATA IN
// PB1 DATA OUT
// PB2 CLK IN
// PB3 CLK OUT
// PB4 ATNA
// PB5 FAST SER DIR
// PB6 /WPAT
// PB7 ATN IN

// FAST SER DIR sets the direction of U13 (74ls241)
// When 1
//	- SP is sent to DATA
//	- Fast Clock (SRQ) is sent to CNT
// When 0
//	- DATA is sent to SP
//	- CNT is sent to Fast Clock (SRQ)

enum PortPins
{
	// PORT A
	PORTA_PINS_SIDE0 = 0x01,	//pa0
	PORTA_PINS_RDY = 0x20,		//pa1
	PORTA_PINS_MOTOR = 0x04,	//pa2

	PORTA_PINS_DEVSEL0 = 0x08,	//pa3
	PORTA_PINS_DEVSEL1 = 0x10,	//pa4

	PORTA_PINS_ACT_LED = 0x40,	//pa6
	PORTA_PINS_DISKCHNG = 0x80,	//pa7

	// PORT B
	PORTB_PINS_FAST_SER_DIR = 0x20, //pb5
	PORTB_PINS_WPAT = 0x40, //pb6
};

enum
{
	FAST_SERIAL_DIR_IN,
	FAST_SERIAL_DIR_OUT
};

extern u16 pc;

// CS
// 8520
//	$4000
//
// 1770
//	$6000
//
// ROM
//	$8000
//
// RAM
// 0-$1fff

u8 read6502_1581(u16 address)
{
	u8 value = 0;
#if defined(PI1581SUPPORT)
	if (address & 0x8000)
	{
		value = roms.Read1581(address);
	}
	else if (address >= 0x6000)
	{
		value = pi1581.wd177x.Read(address);
		//DEBUG_LOG("177x r %04x %02x %04x\r\n", address, value, pc);
	}
	else if (address >= 0x4000)
	{
		value = pi1581.CIA.Read(address);
		//DEBUG_LOG("CIA r %04x %02x %04x\r\n", address, value, pc);
	}
	else if (address < 0x2000)
	{
		value = s_u8Memory[address & 0x1fff];
	}
	else
	{
		value = address >> 8;	// Empty address bus
	}
#endif
	return value;
}

// Use for debugging (Reads VIA registers without the regular VIA read side effects)
u8 peek6502_1581(u16 address)
{
	u8 value = 0;
	return value;
}

void write6502_1581(u16 address, const u8 value)
{
#if defined(PI1581SUPPORT)
	if (address & 0x8000)
	{
		return;
	}
	else if (address >= 0x6000)
	{
		//DEBUG_LOG("177x w %04x %02x %04x\r\n", address, value, pc);
		pi1581.wd177x.Write(address, value);
	}
	else if (address >= 0x4000)
	{
		//DEBUG_LOG("CIA w %04x %02x %04x\r\n", address, value, pc);
		pi1581.CIA.Write(address, value);
	}
	else if (address < 0x2000)
	{
		s_u8Memory[address & 0x1fff] = value;
	}
#endif
}

static void CIAPortA_OnPortOut(void* pUserData, unsigned char status)
{
	Pi1581* pi1581 = (Pi1581*)pUserData;

	pi1581->wd177x.SetSide(status & PORTA_PINS_SIDE0);

	bool motorAsserted = (status & PORTA_PINS_MOTOR) == 0;

	if (motorAsserted)
	{
		if (!pi1581->wd177x.IsExternalMotorAsserted())
		{
			pi1581->CIA.GetPortA()->SetInput(PORTA_PINS_RDY, true);

			pi1581->RDYDelayCount = 250000;
			pi1581->wd177x.AssertExternalMotor(motorAsserted); // !MOTOR
		}
	}
	else
	{
		pi1581->RDYDelayCount = 0;
		pi1581->CIA.GetPortA()->SetInput(PORTA_PINS_RDY, true);
		if (pi1581->wd177x.IsExternalMotorAsserted())
		{
			//DEBUG_LOG("pc=%04x\r\n", pc);
			pi1581->wd177x.AssertExternalMotor(motorAsserted); // !MOTOR
		}
	}

	pi1581->SetLED((status & PORTA_PINS_ACT_LED) != 0);
}

static void CIAPortB_OnPortOut(void* pUserData, unsigned char status)
{
	Pi1581* pi1581 = (Pi1581*)pUserData;

	pi1581->wd177x.SetWPRTPin(status & PORTB_PINS_WPAT); // !WPAT

	if (status & PORTB_PINS_FAST_SER_DIR)
		pi1581->fastSerialDirection = FAST_SERIAL_DIR_OUT;
	else
		pi1581->fastSerialDirection = FAST_SERIAL_DIR_IN;


	IEC_Bus::PortB_OnPortOut(0, status);
}

Pi1581::Pi1581()
{
	Initialise();
}

void Pi1581::Initialise()
{
	LED = false;

	CIA.ConnectIRQ(&m6502.IRQ);
	// IRQ is not connected on a 1581
	//wd177x.ConnectIRQ(&m6502.IRQ);

	CIA.GetPortA()->SetPortOut(this, CIAPortA_OnPortOut);
	CIA.GetPortB()->SetPortOut(this, CIAPortB_OnPortOut);

	// For now disk is writable
	CIA.GetPortB()->SetInput(PORTB_PINS_WPAT, true);

	CIA.GetPortA()->SetInput(PORTA_PINS_DISKCHNG, false);
	CIA.GetPortA()->SetInput(PORTA_PINS_RDY, true);

	RDYDelayCount = 0;
}

void Pi1581::Update()
{
	//CIA.GetPortA()->SetInput(PORTA_PINS_DISKCHNG, 1);
	//CIA.GetPortA()->SetInput(PORTA_PINS_RDY, false);

	if (RDYDelayCount)
	{
		RDYDelayCount--;
		if (RDYDelayCount == 0)
		{
			CIA.GetPortA()->SetInput(PORTA_PINS_RDY, false);
		}
	}

	CIA.Execute();

	// SRQ is pulled high by the c128

	// When U13 (74LS241) is set to fast serial IN
	//	- R20 pulls SP high
	//	- R19 pulls CNT high
	//	- R26 pulls Fast Clock (SRQ) in high
	// When U13 (74LS241) is set to fast serial OUT 
	//	- R25 pulls DATA high
	//	- R28 pulls Fast Clock (SRQ) out high

	if (fastSerialDirection == FAST_SERIAL_DIR_OUT)
	{
		// When 1
		//	- SP is sent to DATA
		//	- Fast Clock (SRQ) is sent to CNT
		IEC_Bus::SetFastSerialData(!CIA.GetPinSP());	// Communication on fast serial is done after the inverter.
		IEC_Bus::SetFastSerialSRQ(CIA.GetPinCNT());
		//trace lines and see it this needs to set other lines
	}
	else
	{
		// When 0
		//	- DATA is sent to SP
		//	- CNT is sent to Fast Clock (SRQ)
		CIA.SetPinSP(!IEC_Bus::GetPI_Data());	// Communication on fast serial is done before the inverter.
		CIA.SetPinCNT(IEC_Bus::GetPI_SRQ());
	}

	for (int i = 0; i < 4; ++i)
	{
		wd177x.Execute();
	}
}

void Pi1581::Reset()
{
	IOPort* CIABPortB;

	fastSerialDirection = FAST_SERIAL_DIR_IN;
	CIA.Reset();
	wd177x.Reset();
	IEC_Bus::Reset();
	// On a real drive the outputs look like they are being pulled high (when set to inputs) (Taking an input from the front end of an inverter)
	CIABPortB = CIA.GetPortB();
	CIABPortB->SetInput(VIAPORTPINS_DATAOUT, true);
	CIABPortB->SetInput(VIAPORTPINS_CLOCKOUT, true);
	CIABPortB->SetInput(VIAPORTPINS_ATNAOUT, true);
}

void Pi1581::SetDeviceID(u8 id)
{
	CIA.GetPortA()->SetInput(PORTA_PINS_DEVSEL0, id & 1);
	CIA.GetPortA()->SetInput(PORTA_PINS_DEVSEL1, id & 2);
}

void Pi1581::Insert(DiskImage* diskImage)
{
//	CIA.GetPortB()->SetInput(PORTB_PINS_WPAT, !diskImage->GetReadOnly());
	CIA.GetPortA()->SetInput(PORTA_PINS_DISKCHNG, true);
	wd177x.Insert(diskImage);
	this->diskImage = diskImage;
}

