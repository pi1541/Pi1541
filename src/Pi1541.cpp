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

