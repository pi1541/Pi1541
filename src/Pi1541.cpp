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

#include "Pi1541.h"
#include "debug.h"
#include "options.h"
#include "ROMs.h"

extern Options options;
extern Pi1541 pi1541;
extern u8 s_u8Memory[0xc000];
extern ROMs roms;

///////////////////////////////////////////////////////////////////////////////////////
// 6502 Address bus functions.
// Move here out of Pi1541 to increase performance.
///////////////////////////////////////////////////////////////////////////////////////
// In a 1541 address decoding and chip selects are performed by a 74LS42 ONE-OF-TEN DECODER
// 74LS42 Ouputs a low to the !CS based on the four inputs provided by address bits 10-13
// 1800 !cs2 on pin 9
// 1c00 !cs2 on pin 7
u8 read6502(u16 address)
{
	u8 value = 0;
	if (address & 0x8000)
	{
		switch (address & 0xe000) // keep bits 15,14,13
		{
			case 0x8000: // 0x8000-0x9fff
				if (options.GetRAMBOard()) {
					value = s_u8Memory[address]; // 74LS42 outputs low on pin 1 or pin 2
					break;
				}
			case 0xa000: // 0xa000-0xbfff
			case 0xc000: // 0xc000-0xdfff
			case 0xe000: // 0xe000-0xffff
				value = roms.Read(address);
				break;
		}
	}
	else
	{
		// Address lines 15, 12, 11 and 10 are fed into a 74LS42 for decoding
		u16 addressLines12_11_10 = (address & 0x1c00) >> 10;
		switch (addressLines12_11_10)
		{
			case 0:
			case 1:
				value = s_u8Memory[address & 0x7ff]; // 74LS42 outputs low on pin 1 or pin 2
				break;
			case 6:
				value = pi1541.VIA[0].Read(address);	// 74LS42 outputs low on pin 7
				break;
			case 7:
				value = pi1541.VIA[1].Read(address);	// 74LS42 outputs low on pin 9
				break;
			default:
				value = address >> 8;	// Empty address bus
				break;
		}
	}
	return value;
}

// Allows a mode where we have RAM at all addresses other than the ROM and the VIAs. (Maybe useful to someone?)
u8 read6502ExtraRAM(u16 address)
{
	if (address & 0x8000)
	{
		return roms.Read(address);
	}
	else
	{
		u16 addressLines11And12 = address & 0x1800;
		if (addressLines11And12 == 0x1800) return pi1541.VIA[(address & 0x400) != 0].Read(address);	// address line 10 indicates what VIA to index
		return s_u8Memory[address & 0x7fff];
	}
}

// Use for debugging (Reads VIA registers without the regular VIA read side effects)
u8 peek6502(u16 address)
{
	u8 value;
	if (address & 0x8000)	// address line 15 selects the ROM
	{
		value = roms.Read(address);
	}
	else
	{
		// Address lines 15, 12, 11 and 10 are fed into a 74LS42 for decoding
		u16 addressLines15_12_11_10 = (address & 0x1c00) >> 10;
		addressLines15_12_11_10 |= (address & 0x8000) >> (15 - 3);
		if (addressLines15_12_11_10 == 0 || addressLines15_12_11_10 == 1) value = s_u8Memory[address & 0x7ff]; // 74LS42 outputs low on pin 1 or pin 2
		else if (addressLines15_12_11_10 == 6) value = pi1541.VIA[0].Peek(address);	// 74LS42 outputs low on pin 7
		else if (addressLines15_12_11_10 == 7) value = pi1541.VIA[1].Peek(address);	// 74LS42 outputs low on pin 9
		else value = address >> 8;	// Empty address bus
	}
	return value;
}

void write6502(u16 address, const u8 value)
{
	if (address & 0x8000)
	{
		switch (address & 0xe000) // keep bits 15,14,13
		{
			case 0x8000: // 0x8000-0x9fff
				if (options.GetRAMBOard()) {
					s_u8Memory[address] = value; // 74LS42 outputs low on pin 1 or pin 2
					break;
				}
			case 0xa000: // 0xa000-0xbfff
			case 0xc000: // 0xc000-0xdfff
			case 0xe000: // 0xe000-0xffff
				return;
		}
	}
	else
	{
		// Address lines 15, 12, 11 and 10 are fed into a 74LS42 for decoding
		u16 addressLines12_11_10 = (address & 0x1c00) >> 10;
		switch (addressLines12_11_10)
		{
			case 0:
			case 1:
				s_u8Memory[address & 0x7ff] = value; // 74LS42 outputs low on pin 1 or pin 2
				break;
			case 6:
				pi1541.VIA[0].Write(address, value);	// 74LS42 outputs low on pin 7
				break;
			case 7:
				pi1541.VIA[1].Write(address, value);	// 74LS42 outputs low on pin 9
				break;
			default:
				break;
		}
	}
}

void write6502ExtraRAM(u16 address, const u8 value)
{
	if (address & 0x8000) return; // address line 15 selects the ROM
	u16 addressLines11And12 = address & 0x1800;
	if (addressLines11And12 == 0) s_u8Memory[address & 0x7fff] = value;
	else if (addressLines11And12 == 0x1800) pi1541.VIA[(address & 0x400) != 0].Write(address, value);	// address line 10 indicates what VIA to index
}

Pi1541::Pi1541()
{
	VIA[0].ConnectIRQ(&m6502.IRQ);
	VIA[1].ConnectIRQ(&m6502.IRQ);
}

void Pi1541::Initialise()
{
	VIA[0].ConnectIRQ(&m6502.IRQ);
	VIA[1].ConnectIRQ(&m6502.IRQ);
}

//void Pi1541::ConfigureOfExtraRAM(bool extraRAM)
//{
//	if (extraRAM)
//		m6502.SetBusFunctions(this, Read6502ExtraRAM, Write6502ExtraRAM);
//	else
//		m6502.SetBusFunctions(this, Read6502, Write6502);
//}

void Pi1541::Update()
{
	if (drive.Update())
	{
		//This pin sets the overflow flag on a negative transition from TTL one to TTL zero.
		// SO is sampled at the trailing edge of P1, the cpu V flag is updated at next P1.
		m6502.SO();
	}

	VIA[1].Execute();
	VIA[0].Execute();
}

void Pi1541::Reset()
{
	IOPort* VIABortB;

	// Must reset the VIAs first as the devices will initialise inputs (eg CA1 ports etc)
	// - VIAs will reset the inputs to a default value
	//		- devices will then set the inputs
	//			- could cause a IR_CXX IRQ
	//				- possibilities
	//					- reset while an ATN
	//					- reset while !BYTE SYNC
	//			- should be fine as VIA's functionControlRegister is reset to 0 and IRQs will be turned off
	VIA[0].Reset();
	VIA[1].Reset();
	drive.Reset();
	IEC_Bus::Reset();
	// On a real drive the outputs look like they are being pulled high (when set to inputs) (Taking an input from the front end of an inverter)
	VIABortB = VIA[0].GetPortB();
	VIABortB->SetInput(VIAPORTPINS_DATAOUT, true);
	VIABortB->SetInput(VIAPORTPINS_CLOCKOUT, true);
	VIABortB->SetInput(VIAPORTPINS_ATNAOUT, true);
}

