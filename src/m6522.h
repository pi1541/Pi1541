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

#ifndef M6522_H
#define M6522_H

#include "IOPort.h"
#include "m6502.h"

class m6522
{
	// $1800
	// PB 0		data in
	// PB 1		data out
	// PB 2		clock in
	// PB 3		clock out
	// PB 4		ATNA out
	// PB 5,6	device address
	// PB 7,CA1	ATN IN

	// $1C00
	// PB 0,1	step motor
	// PB 2		MTR dirve motor
	// PB 3		ACT drive LED
	// PB 4		WPS	write protect switch
	// PB 5,6	bit rate
	// PB 7		Sync
	// CA 1		Byte ready
	// CA 2		SOE set overflow enable 6502
	// CB 2		read/write

	//  IFR
	//REG 13 -- INTERRUPT FLAG REGISTER
	//+-+-+-+-+-+-+-+-+
	//|7|6|5|4|3|2|1|0|             SET BY                    CLEARED BY
	//+-+-+-+-+-+-+-+-+    +-----------------------+------------------------------+
	// | | | | | | | +--CA2| CA2 ACTIVE EDGE       | READ OR WRITE REG 1 (ORA)*   |
	// | | | | | | |       +-----------------------+------------------------------+
	// | | | | | | +--CA1--| CA1 ACTIVE EDGE       | READ OR WRITE REG 1 (ORA)    |
	// | | | | | |         +-----------------------+------------------------------+
	// | | | | | +SHIFT REG| COMPLETE 8 SHIFTS     | READ OR WRITE SHIFT REG      |
	// | | | | |           +-----------------------+------------------------------+
	// | | | | +-CB2-------| CB2 ACTIVE EDGE       | READ OR WRITE ORB*           |
	// | | | |             +-----------------------+------------------------------+
	// | | | +-CB1---------| CB1 ACTIVE EDGE       | READ OR WRITE ORB            |
	// | | |               +-----------------------+------------------------------+
	// | | +-TIMER 2-------| TIME-OUT OF T2        | READ T2 LOW OR WRITE T2 HIGH |
	// | |                 +-----------------------+------------------------------+
	// | +-TIMER 1---------| TIME-OUT OF T1        | READ T1 LOW OR WRITE T1 HIGH |
	// |                   +-----------------------+------------------------------+
	// +-IRQ---------------| ANY ENABLED INTERRUPT | CLEAR ALL INTERRUPTS         |
	//                     +-----------------------+------------------------------+

	enum Registers
	{
		ORB,  // 0 Port B
		ORA,  // 1 Port A
		DDRB,  // 2 Data direction register for port B
		DDRA,  // 3 Data direction register for port A
	
		T1CL,  // 4 Timer 1 count low
		T1CH,  // 5 Timer 1 count high
		T1LL,  // 6 Timer 1 latch low
		T1LH,  // 7 Timer 1 latch high
		T2CL,  // 8 Timer 2 count low			read-only
		T2LL = T2CL, // 8 Timer 2 latch low	write-only
		T2CH,  // 9 Timer 2 count high		read/write
	
		SR, // 10 Serial port shift register
		
		ACR, // 11 Auxiliary control register
		FCR, // 12 Peripheral control register
	
		IFR, // 13 Interrupt flag register
		IER, // 14 Interrupt Enable Register
		ORA_NH // 15 Port A with no handshake
	};


	enum ACR
	{
		ACR_PA_LATCH_ENABLE = 0x01,	// Port A latch
									//	0 = disabled
									//	1 = enabled on CA1 transition (in)
		ACR_PB_LATCH_ENABLE = 0x02,	// Port B latch
									//	0 = disabled
									//	1 = enabled on CB1 transition (in/out)
		ACR_SHIFTREG_CTRL = 0x1c,	// Shift register control
									//	000 = shift reg disabled
									//	001 = shift in by timer 2
									//	010 = shift in by phi2
									//	011 = shift in by external clock (PB6?)
									//	100 = free run shift out by timer 2 (keep shifting the same byte out over and over)
									//	101 = shift out by timer 2
									//	110 = shift out by phi2
									//	111 = shift out by external clock(PB6 ? )
		ACR_T2_MODE = 0x20,			// Timer 2 control
									//	0 = one shot (timed interrrupt)
									//	1 = count down with pulses on PB6
		ACR_T1_MODE = 0x40,			// Timer 1 control
									//	0 = one shot
									//	1 = continuous, i.e. on underflow timer restarts at latch value.
		ACR_T1_OUT_PB7 = 0x80		// Output on PB7
	};

	enum IR
	{
		IR_CA2 = 0x01,		// CA2 flag
							//	Cleared by a read or write of ORA
		IR_CA1 = 0x02,		// CA1 flag
							//	Cleared by a read or write of ORA
		IR_SR = 0x04,		// Shift Register completion
							//	1 at end of 8 shifts
							//	Cleared by read or write of SR
		IR_CB2 = 0x08,		// CB2 flag
							//	Cleared by a read or write of ORB
		IR_CB1 = 0x10,		// CB1 flag
							//	Cleared by a read or write of ORB
		IR_T2 = 0x20,		// Timer 2
							//	1 when time out
							//	0 after reading T2 low-byte counter or writing T2 high-byte counter
		IR_T1 = 0x40,		// Timer 1
							//	1 when time out
							//	0 after reading T1 low-byte counter or writing T1 high-byte latch
		IR_IRQ = 0x80		// General interrupt status bit 
							//	1 if any interrupt active and enabled
							//	0 when interrupt condition cleared
	};

public:
/*
FCR/PCR
						+---+---+---+---+---+---+---+---+
						| 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
						+---+---+---+---+---+---+---+---+
						 |         |  |  |         |  |
						 +----+----+  |  +----+----+  |
							  |       |       |       |
			 CB2 CONTROL -----+       |       |       +- CA1 INTERRUPT CONTROL
	+-+-+-+------------------------+  |       |   +--------------------------+
	|7|6|5| OPERATION              |  |       |   | 0 = NEGATIVE ACTIVE EDGE |
	+-+-+-+------------------------+  |       |   | 1 = POSITIVE ACTIVE EDGE |
	|0|0|0| INPUT NEG. ACTIVE EDGE |  |       |   +--------------------------+
	+-+-+-+------------------------+  |       +---- CA2 INTERRUPT CONTROL
	|0|0|1| INDEPENDENT INTERRUPT  |  |       +-+-+-+------------------------+
	| | | | INPUT NEGATIVE EDGE    |  |       |3|2|1| OPERATION              |
	+-+-+-+------------------------+  |       +-+-+-+------------------------+
	|0|1|0| INPUT POS. ACTIVE EDGE |  |       |0|0|0| INPUT NEG. ACTIVE EDGE |
	+-+-+-+------------------------+  |       +-+-+-+------------------------+
	|0|1|1| INDEPENDENT INTERRUPT  |  |       |0|0|1| INDEPENDENT INTERRUPT  |
	| | | | INPUT POSITIVE EDGE    |  |       | | | | INPUT NEGATIVE EDGE    |
	+-+-+-+------------------------+  |       +-+-+-+------------------------+
	|1|0|0| HANDSHAKE OUTPUT       |  |       |0|1|0| INPUT POS. ACTIVE EDGE |
	+-+-+-+------------------------+  |       +-+-+-+------------------------+
	|1|0|1| PULSE OUTPUT           |  |       |0|1|1| INDEPENDENT INTERRUPT  |
	+-+-+-+------------------------+  |       | | | | INPUT POSITIVE EDGE    |
	|1|1|0| LOW OUTPUT             |  |       +-+-+-+------------------------+
	+-+-+-+------------------------+  |       |1|0|0| HANDSHAKE OUTPUT       |
	|1|1|1| HIGH OUTPUT            |  |       +-+-+-+------------------------+
	+-+-+-+------------------------+  |       |1|0|1| PULSE OUTPUT           |
		CB1 INTERRUPT CONTROL --------+       +-+-+-+------------------------+
	+--------------------------+              |1|1|0| LOW OUTPUT             |
	| 0 = NEGATIVE ACTIVE EDGE |              +-+-+-+------------------------+
	| 1 = POSITIVE ACTIVE EDGE |              |1|1|1| HIGH OUTPUT            |
	+--------------------------+              +-+-+-+------------------------+
*/
	enum FCR
	{
		FCR_CA1 = 0x01,
		FCR_CA2_OUTPUT_MODE0 = 0x02,		// 1c00 byte ready active 1541 rom $FAC1
		FCR_CA2_OUTPUT_MODE1 = 0x04,
		FCR_CA2_EDGE_TRIGGER_MODE = 0x04,
		FCR_CA2_IO = 0x08,
		FCR_CA2 = 0x0e,

		FCR_CB1 = 0x01,
		FCR_CB2_OUTPUT_MODE0 = 0x20,		// 1c00 writing
		FCR_CB2_OUTPUT_MODE1 = 0x40,
		FCR_CB2_EDGE_TRIGGER_MODE = 0x40,
		FCR_CB2_IO = 0x80,
		FCR_CB2 = 0xe0,
	};

	m6522();

	void Reset();
	void ConnectIRQ(Interrupt* irq) { this->irq = irq; }

	inline IOPort* GetPortA() { return &portA; }
	inline bool GetLatchPortA() const { return latchPortA; }
	inline unsigned char GetLatchedValueA() { return latchedValueA; }
	inline bool GetCA1() { return ca1; }
	void InputCA1(bool value);
	inline bool GetCA2() { return ca2; }
	void InputCA2(bool value);

	inline IOPort* GetPortB() { return &portB; }
	bool GetLatchPortB() const { return latchPortB; }
	unsigned char GetLatchedValueB() { return latchedValueB; }
	inline bool GetCB1() { return cb1; }
	void InputCB1(bool value);
	inline bool GetCB2() { return cb2; }
	void InputCB2(bool value);

	void Execute();

	unsigned char Read(unsigned int address);
	unsigned char Peek(unsigned int address);
	void Write(unsigned int address, unsigned char value);

	inline unsigned char GetFCR()
	{
		return functionControlRegister;
	}
private:
	inline unsigned char ReadPortB()
	{
		unsigned char ddr = portB.GetDirection();
		unsigned char value = (latchPortB && (interruptFlagRegister & (unsigned char)IR_CB1) != 0) ? latchedValueB : (unsigned char)((portB.GetInput() & ~ddr) | (portB.GetOutput() & ddr));
		ClearInterrupt(IR_CB1 | IR_CB2);
		return value;
	}

	inline void WritePortB(unsigned char value)
	{
		ClearInterrupt(IR_CB1 | IR_CB2);
		if ((functionControlRegister & (unsigned char)(FCR_CB2_IO | FCR_CB2_OUTPUT_MODE1)) == (unsigned char)(FCR_CB2_IO | FCR_CB2_OUTPUT_MODE1))
			cb2 = false;
		portB.SetOutput(value);
	}

	inline unsigned char ReadPortA(bool handshake)
	{
		unsigned char ddr = portA.GetDirection();
		unsigned char value = (latchPortA && (interruptFlagRegister & (unsigned char)IR_CA1) != 0) ? latchedValueA : (unsigned char)((portA.GetInput() & ~ddr) | (portA.GetOutput() & ddr));
		if (handshake)
			ClearInterrupt(IR_CA1 | IR_CA2);
		return value;
	}

	inline unsigned char PeekPortA()
	{
		unsigned char ddr = portA.GetDirection();
		unsigned char value = (latchPortA && (interruptFlagRegister & (unsigned char)IR_CA1) != 0) ? latchedValueA : (unsigned char)((portA.GetInput() & ~ddr) | (portA.GetOutput() & ddr));
		return value;
	}

	inline void WritePortA(unsigned char value, bool handshake)
	{
		if (handshake)
		{
			ClearInterrupt(IR_CA1 | IR_CA2);
			if ((functionControlRegister & (unsigned char)(FCR_CA2_IO | FCR_CA2_OUTPUT_MODE1)) == (unsigned char)(FCR_CA2_IO | FCR_CA2_OUTPUT_MODE1))
				ca2 = false;
		}
		portA.SetOutput(value);
	}

	inline unsigned char PeekPortB()
	{
		unsigned char ddr = portB.GetDirection();
		unsigned char value = (latchPortB && (interruptFlagRegister & (unsigned char)IR_CB1) != 0) ? latchedValueB : (unsigned char)((portB.GetInput() & ~ddr) | (portB.GetOutput() & ddr));
		return value;
	}

	inline void SetInterrupt(unsigned char flag)
	{
		if (!(interruptFlagRegister & flag))
		{
			interruptFlagRegister |= flag;
			OutputIRQ();
		}
	}

	inline void ClearInterrupt(unsigned char flag)
	{
		if (interruptFlagRegister & flag)
		{
			interruptFlagRegister &= ~flag;
			OutputIRQ();
		}
	}
	inline void OutputIRQ()
	{
		if (interruptEnabledRegister & interruptFlagRegister & 0x7f)
		{
			if ((interruptFlagRegister & IR_IRQ) == 0)
			{
				interruptFlagRegister |= IR_IRQ;
				if (irq) irq->Assert();
			}
		}
		else
		{
			if (interruptFlagRegister & IR_IRQ)
			{
				interruptFlagRegister &= ~IR_IRQ;
				if (irq) irq->Release();
			}
		}
	}

	struct Counter
	{
		union
		{
			unsigned short value;
			struct
			{
				// if porting to big endian, swap these.
				unsigned char l;
				unsigned char h;
			} bytes;
		};
	};

	Interrupt* irq;

	unsigned char functionControlRegister;
	unsigned char auxiliaryControlRegister;

	IOPort portA;
	bool latchPortA;
	unsigned char latchedValueA;
	bool ca1;
	bool ca2;
	bool pulseCA2;

	IOPort portB;
	bool latchPortB;
	unsigned char latchedValueB;
	bool cb1;
	bool cb1Old;
	bool cb2;
	bool pulseCB2;

	Counter t1c;
	Counter t1l;
	bool t1Ticking;
	bool t1Reload;
	bool t1OutPB7;
	bool t1FreeRun;
	bool t1FreeRunIRQsOn;
	bool t1TimedOut;
	bool t1_pb7;
	bool t1OneShotTriggeredIRQ;

	Counter t2c;
	unsigned char t2Latch;
	bool t2Reload;
	bool t2CountingDown;
	bool t2CountingPB6ModeOld;
	bool t2CountingPB6Mode;
	bool t2TimedOut;
	bool t2LowTimedOut;
	bool t2OneShotTriggeredIRQ;
	unsigned t2TimedOutCount;
	unsigned char pb6Old;

	unsigned char interruptFlagRegister;
	unsigned char interruptEnabledRegister;

	unsigned char shiftRegister;
	unsigned bitsShiftedSoFar;
	unsigned cb1OutputShiftClock;
	unsigned char cb2Shift;  // version of cb2 controlled by the shift register
	bool cb1OutputShiftClockPositiveEdge;
};

#endif