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

#ifndef M8520_H
#define M8520_H

#include "IOPort.h"
#include "m6502.h"
#include "debug.h"

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

// !FLAG = !ATN IN


class m8520
{
	enum Registers
	{
		ORA,  // 0 Port A
		ORB,  // 1 Port B
		DDRA,  // 2 Data direction register for port A
		DDRB,  // 3 Data direction register for port B
	
		TALO,  // 4 Timer A low
		TAHI,  // 5 Timer A high
		TBLO,  // 6 Timer B low
		TBHI,  // 7 Timer B high
		EVENT_LSB,	// 8
		EVENT_8_15,	// 9
		EVENT_MSB,	// 10

		NC,		// 11 No connect
		SDR, // 12 Serial data register
		
		ICR, // 13 Interrupt control register
		CRA, // 14 Control register A
		CRB, // 15 Control register B
	};

	enum IR
	{
		IR_TA = 0x01,
		IR_TB = 0x02,
		IR_TOD = 0x04,
		IR_SDR = 0x08,
		IR_FLG = 0x10,
		IR_SET = 0x80
	};

	enum CRA_BIT
	{
		CRA_START = 0x01,
		CRA_PBON = 0x02,
		CRA_OUTPUTMODE = 0x04,
		CRA_RUNMODE = 0x08,
		CRA_LOAD = 0x10,
		CRA_INMODE = 0x20,
		CRA_SPMODE = 0x40,
		CRA_TODIN = 0x80
	};

	enum CRB_BIT
	{
		CRB_START = 0x01,
		CRB_PBON = 0x02,
		CRB_OUTPUTMODE = 0x04,
		CRB_RUNMODE = 0x08,
		CRB_LOAD = 0x10,
		CRB_INMODE0 = 0x20,
		CRB_INMODE1 = 0x40,
		CRB_ALARM = 0x80
	};

	enum TimerAMode
	{
		TA_MODE_PHI2,
		TA_MODE_CNT_PVE
	};

	enum TimerBMode
	{
		TB_MODE_PHI2,
		TB_MODE_CNT_PVE,
		TB_MODE_TA_UNDEFLOW,
		TB_MODE_TA_UNDEFLOW_CNT_PVE
	};

	enum SerialPortMode
	{
		SP_MODE_OUTPUT,
		SP_MODE_INPUT
	};

public:
	m8520();

	void Reset();
	void ConnectIRQ(Interrupt* irq) { this->irq = irq; }

	inline IOPort* GetPortA() { return &portA; }
	inline IOPort* GetPortB() { return &portB; }

	void Execute();

	unsigned char Read(unsigned int address);
	unsigned char Peek(unsigned int address);
	void Write(unsigned int address, unsigned char value);

	bool IsPCAsserted() const { return PCAsserted; }
	void SetPinFLAG(bool value);	// active low
	void SetPinCNT(bool value);
	bool GetPinCNT() const { return CNTPin; }
	void SetPinSP(bool value);
	bool GetPinSP() const { return SPPin; }
	void SetPinTOD(bool value);

//private:
	inline unsigned char ReadPortB()
	{
		unsigned char ddr = portB.GetDirection();
		unsigned char value = (unsigned char)((portB.GetInput() & ~ddr) | (portB.GetOutput() & ddr));
		return value;
	}

	inline void WritePortB(unsigned char value)
	{
		portB.SetOutput(value);
	}

	inline unsigned char ReadPortA()
	{
		unsigned char ddr = portA.GetDirection();
		unsigned char value = (unsigned char)((portA.GetInput() & ~ddr) | (portA.GetOutput() & ddr));
		return value;
	}

	inline unsigned char PeekPortA()
	{
		unsigned char ddr = portA.GetDirection();
		unsigned char value = (unsigned char)((portA.GetInput() & ~ddr) | (portA.GetOutput() & ddr));
		return value;
	}

	inline void WritePortA(unsigned char value)
	{
		portA.SetOutput(value);
	}

	inline unsigned char PeekPortB()
	{
		unsigned char ddr = portB.GetDirection();
		unsigned char value = (unsigned char)((portB.GetInput() & ~ddr) | (portB.GetOutput() & ddr));
		return value;
	}

	inline void SetInterrupt(unsigned char flag)
	{
		if (!(ICRData & flag))
		{
			ICRData |= flag;
			OutputIRQ();
		}
	}

	inline void ClearInterrupt(unsigned char flag)
	{
		if (ICRData & flag)
		{
			ICRData &= ~flag;
			OutputIRQ();
		}
	}

	inline void OutputIRQ()
	{
		// Any interrupt which is enabled by the MASK register will set the IR bit(MSB) of the DATA register and bring the IRQ pin low.
		if (ICRMask & ICRData & (IR_FLG | IR_SDR | IR_TOD | IR_TB | IR_TA))
		{
			if ((ICRData & IR_SET) == 0)
			{
				ICRData |= IR_SET;
				if (irq) irq->Assert();
			}
		}
		else
		{
			if (ICRData & IR_SET)
			{
				//DEBUG_LOG("Releasing IRQ %02x\r\n", ICRData);
				ICRData &= ~IR_SET;
				if (irq) irq->Release();
			}
		}
	}

	inline void ReloadTimerA()
	{
		timerACounter = timerALatch;
		timerAReloaded = true;
	}

	inline void ReloadTimerB()
	{
		timerBCounter = timerBLatch;
		timerBReloaded = true;
	}

	Interrupt* irq;

	IOPort portA;
	IOPort portB;

	unsigned char ICRMask;
	unsigned char ICRData;

	unsigned char CRARegister;
	unsigned char CRBRegister;

	u32 PCAsserted;
	bool FLAGPin;
	bool CNTPin;
	bool CNTPinOld;
	bool SPPin;
	bool TODPin;

	unsigned short timerACounter;
	unsigned short timerALatch;
	bool timerAActive;
	bool timerAOutputOnPB6;
	bool timerAToggle;
	bool timerAOneShot;
	TimerAMode timerAMode;
	bool timerA50Hz;
	bool ta_pb6;
	bool timerAReloaded;

	unsigned short timerBCounter;
	unsigned short timerBLatch;
	bool timerBActive;
	bool timerBOutputOnPB7;
	bool timerBToggle;
	bool timerBOneShot;
	TimerBMode timerBMode;
	bool timerBAlarm;
	bool tb_pb7;
	bool timerBReloaded;

	bool TODActive;
	unsigned TODAlarm;
	unsigned TODClock;
	unsigned TODLatch;

	SerialPortMode serialPortMode;
	unsigned char serialPortRegister;
	unsigned char serialShiftRegister;
	unsigned serialBitsShiftedSoFar;
	bool serialShiftingEnabled;
	//unsigned timerATimeOutCount;
};

#endif