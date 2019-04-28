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

#include "m6522.h"

// There are a number of inherent undocumented edge cases with regards to Timer 2. 
// A lot of empirical measurements, in the form of bus captures of a real Commodore 1541 VIA were taken to discover the exact behavior of the timers (especially timer 2 and all its idiosyncrasies).
// Many comments in this file are taken from statements found in the 6522 data sheets.

m6522::m6522()
{
	Reset();
}

void m6522::Reset()
{
	functionControlRegister = 0;
	auxiliaryControlRegister = 0;

	latchPortA = false;
	latchedValueA = 0;
	ca1 = false;
	ca2 = false;
	pulseCA2 = false;
	
	latchedValueB = 0;
	cb1 = false;
	cb1Old = false;
	cb2 = false;
	pulseCB2 = false;
	
	t1c.bytes.l = 0xff;
	t1c.bytes.h = 0xff;
	t1l.bytes.l = 0xff;
	t1l.bytes.h = 0xff;
	t1Ticking = false;
	t1Reload = false;
	t1OutPB7 = false;
	t1FreeRun = false;
	t1_pb7 = true;
	t1TimedOut = false;
	t1OneShotTriggeredIRQ = false;

	t2c.bytes.l = 0xc9;	// logic analyser detects that these are some what random
	t2c.bytes.h = 0xfb;	// logic analyser detects that these are some what random
	t2Latch = 0;
	t2Reload = false;
	t2CountingDown = false;
	t2TimedOutCount = 0;
	t2LowTimedOut = false;
	t2CountingPB6Mode = false;
	t2CountingPB6ModeOld = false;
	pb6Old = 0;
	t2TimedOut = false;
	t2OneShotTriggeredIRQ = false;

	interruptFlagRegister = 0;
	interruptEnabledRegister = 0;

	shiftRegister = 0;

	// External devices should be doing this
	// - what about CA1 and CB1?
	InputCA2(true);
	InputCB2(true);

	bitsShiftedSoFar = 0;
	cb1OutputShiftClock = 0;
	cb1OutputShiftClockPositiveEdge = false;
	cb2Shift = 0;
	OutputIRQ();
}

void m6522::InputCA1(bool value)
{
	if (ca1 != value && ((functionControlRegister & FCR_CA1) != 0) == value) // CA1 is an input?
	{
		unsigned char ddr = portA.GetDirection();
		latchedValueA = ((portA.GetInput() & ~ddr) | (portA.GetOutput() & ddr));
		// test HANDSHAKE OUTPUT mode and if so auto clear
		if ((functionControlRegister & (FCR_CA2_IO | FCR_CA2_OUTPUT_MODE1 | FCR_CB2_OUTPUT_MODE0)) == FCR_CA2_IO)
			ca2 = false;
		SetInterrupt(IR_CA1);
	}
	ca1 = value;
}

void m6522::InputCA2(bool value)
{
	if ((functionControlRegister & FCR_CA2_IO) == 0) // CA2 is an input?
	{
		if (ca2 != value && ((functionControlRegister & FCR_CA2_EDGE_TRIGGER_MODE) != 0) == value)
			SetInterrupt(IR_CA2);	// interrupt if we are tracking edges
		ca2 = value;
	}
}

void m6522::InputCB1(bool value)
{
	if (cb1 != value && ((functionControlRegister & FCR_CB1) != 0) == value) // CB1 is an input?
	{
		unsigned char ddr = portB.GetDirection();
		latchedValueB = ((portB.GetInput() & ~ddr) | (portB.GetOutput() & ddr));
		// test HANDSHAKE OUTPUT mode and if so auto clear
		if ((functionControlRegister & (FCR_CB2_IO | FCR_CB2_OUTPUT_MODE1 | FCR_CB2_OUTPUT_MODE0)) == FCR_CB2_IO)
			cb2 = false;
		SetInterrupt(IR_CB1);
	}
	cb1 = value;
}

// If CB2 is not set to an output then reads the CB2 line and stores the value in cb2
void m6522::InputCB2(bool value)
{
	if ((functionControlRegister & FCR_CB2_IO) == 0) // CB2 is an input?
	{
		if (cb2 != value && ((functionControlRegister & FCR_CB2_EDGE_TRIGGER_MODE) != 0) == value)
			SetInterrupt(IR_CB2);	// interrupt if we are tracking edges
		cb2 = value;
	}
}

// Update for a single cycle
void m6522::Execute()
{
	if (ca2 && pulseCA2) ca2 = false;
	if (cb2 && pulseCB2) cb2 = false;

	// The t1 counter decrements on each succeeding phi2 from N to 0 and then one half phi2 cycle later IRQ goes active.
	// (where N is the combined count value of T1CL and T1CH)
	if (t1TimedOut)
	{
		t1c.value = t1l.value;
		t1TimedOut = false;
	}
	else if (t1Ticking && !t1Reload && !t1c.value--)
	{
		t1TimedOut = true;

		if (t1FreeRun)
		{
			if (t1FreeRunIRQsOn)
				SetInterrupt(IR_T1);

			if (t1l.value > 1)	// A real VIA will not flip PB7 if the frequency is above a certain (ie 1 cycle) threshold
			{
				t1_pb7 = !t1_pb7;
				if (t1OutPB7)
				{
					unsigned char ddr = portB.GetDirection();
					if (ddr & 0x80)
					{
						// the signal on PB7 is inverted each time the counter reaches zero
						if (!t1_pb7) portB.SetOutput(portB.GetOutput() & (~0x80));
						else portB.SetOutput(portB.GetOutput() | 0x80);
					}
				}
			}
		}
		else
		{
			if (!t1OneShotTriggeredIRQ)
			{
				t1OneShotTriggeredIRQ = true;
				SetInterrupt(IR_T1);

				if (t1OutPB7)
				{
					// PB7 was set low on the write to T1CH now the signal on PB7 will go high
					// The duration of the pulse is equal to N + one and one half (where N equals the count value) to guarantee a valid output level on PB7.
					unsigned char ddr = portB.GetDirection();
					if (ddr & 0x80) portB.SetOutput(portB.GetOutput() | 0x80);
					t1_pb7 = false;
				}
			}
		}
	}
	t1Reload = false;

	// Timer 2 can also be used to count negative pulses on the	PB6 line.
	unsigned char pb6 = portB.GetInput() & ~portB.GetDirection() & 0x40;
	unsigned char shiftMode = (auxiliaryControlRegister & ACR_SHIFTREG_CTRL) >> 2;

	// The data is shifted into the shift register during the phi2 clock cycle following the positive going edge of the CB1 clock pulse.
	// - So we test the edge of the clock last cycle and if positive shift this cycle.
	bool shiftClockPositiveEdge = cb1OutputShiftClockPositiveEdge;
	cb1OutputShiftClockPositiveEdge = false;

	if (t2TimedOut)
	{
		t2TimedOut = false;

		// In both modes the interrupt is only set once


		if ((auxiliaryControlRegister & 0xc) == 4) // shift by timer 2?
		{
			cb1OutputShiftClockPositiveEdge = cb1OutputShiftClock;	// If positive edge we need to shift next phi2 so cache for one cycle
			cb1OutputShiftClock = !cb1OutputShiftClock;
		}

		if (t2Latch == 0xff)
			t2c.value--;

		if ((t2TimedOutCount > 1) && t2c.bytes.h == 0)
		{
			t2c.bytes.h = 0xff;
			t2c.bytes.l = t2Latch + 2;
		}
		else
		{
			t2c.bytes.l = t2Latch;
		}
		t2LowTimedOut = false;
	}

	if (t2CountingDown)
	{
		if (t2CountingPB6Mode ^ t2CountingPB6ModeOld)
		{
			// If T2 has changed modes then the IRQ is back on the table
			t2OneShotTriggeredIRQ = false;
			if (!t2CountingPB6Mode)
			{
				if (t2c.value == 0)			// PB6 mode turned off just as it timed out we still need to interrupt.
					SetInterrupt(IR_T2);
			}
			else
			{
				// When switching PB6Mode back on it will still count down one more time
				t2c.value--;
				t2TimedOut = t2c.value == 0;
			}
		}
		else if (!t2Reload)	// Only do this if it was not just reloaded by writing to T2CH
		{
			// Bit 5 of the ACR determines whether the counter is decremented by the 6502 system clock or input pulses arriving on PB6.
			if (t2CountingPB6Mode && t2CountingPB6ModeOld)
			{
				if (pb6 == 0 && pb6Old == 1)	// Was it the negative edge?
				{
					t2c.value--;
					t2TimedOut = t2c.value == 0;
				}
			}
			else
			{
				t2c.value--;
				t2TimedOut = t2c.value == 0;

				if (t2c.bytes.l == 0xfe)
				{
					t2TimedOutCount++;
					if ((auxiliaryControlRegister & 0xc) == 4) // shift by timer 2?
					{
						if (t2TimedOutCount > 1)
						{
							cb1OutputShiftClockPositiveEdge = cb1OutputShiftClock;	// If positive edge we need to shift next phi2 so cache for one cycle
							cb1OutputShiftClock = !cb1OutputShiftClock;
							t2c.bytes.l = t2Latch;
							t2TimedOut = false;
						}
					}
				}
			}
		}
		else
		{
			t2Reload = false;
		}

		if (t2TimedOut)
		{
			// In both modes the interrupt is only set once
			if (!t2OneShotTriggeredIRQ)
			{
				t2OneShotTriggeredIRQ = true;
				SetInterrupt(IR_T2);
			}
			else
			{
				// At this time the counter will continue to decrement at system clock rate or PB6 negative edge counts (depending upon mode)
				// This allows the system processor to read the contents of the counter to determine the time since interrupt.
			}
		}
	}
	pb6Old = pb6;
	t2CountingPB6ModeOld = t2CountingPB6Mode;

	switch (shiftMode)
	{
		default:	// 000 = shift reg disabled
			// The CPU can read and write the SR but shifting is disabled.
			// Both CB1 and CB2 are controlled by peripheral control register.
		break;
		case 1:		// 001 = shift in by timer 2
			if ((t2TimedOutCount > 2) && shiftClockPositiveEdge && !(bitsShiftedSoFar & 8))
			{
				// should output cb1OutputShiftClock onto cb1
				shiftRegister <<= 1;
				shiftRegister |= cb2;	// Should get from current cb2 (in a 1541 these pins on the VIAs are NC, measure at 5v and read as 1s)
				if (++bitsShiftedSoFar == 8)
					SetInterrupt(IR_SR);
			}
		break;
		case 2:		// 010 = shift in by phi2
			// SHIFT REGISTER BUG not implemented
			// In both the shift in and shift out modes a liming condition may occur when the 6522 does not detect the shift pulse.
			// This no shift condition occurs when CB1 and phi2 are asynchronous and their edges coincide.
			if (!(bitsShiftedSoFar & 8))	// Shift register bug not implmented (would shift 9 bits?)
			{
				// should output cb1OutputShiftClock onto cb1
				cb1OutputShiftClock = !cb1OutputShiftClock;
				shiftRegister <<= 1;
				shiftRegister |= cb2;	// Should get from current cb2 (in a 1541 these pins on the VIAs are NC, measure at 5v and read as 1s)
				if (++bitsShiftedSoFar == 8)
					SetInterrupt(IR_SR);
			}
		break;
		case 3:		// 011 = shift in by external clock
			// SHIFT REGISTER BUG not implemented
			// In both the shift in and shift out modes a liming condition may occur when the 6522 does not detect the shift pulse.
			// This no shift condition occurs when CB1 and phi2 are asynchronous and their edges coincide.
			if (cb1Old && !cb1)	// Negitive edge
			{
				if (!(bitsShiftedSoFar & 8))	// Shift register bug not implmented (would shift 9 bits?)
				{
					shiftRegister <<= 1;
					shiftRegister |= cb2;	// Should get from current cb2 (in a 1541 these pins on the VIAs are NC, measure at 5v and read as 1s)
					if (++bitsShiftedSoFar == 8)
						SetInterrupt(IR_SR);
				}
			}
		break;
		case 4:		// 100 = free run shift out by timer 2 (keep shifting the same byte out over and over)
			if (shiftClockPositiveEdge)	// in this mode	the shift register counter is disabled.
			{
				cb2Shift = (shiftRegister & 0x80) != 0;
				shiftRegister = (shiftRegister << 1) | cb2Shift;
				// should output cb1OutputShiftClock onto cb1
				// cb2Shift should output to cb2
				//	- R/!W (on the 2nd VIA could be dangerous)
			}
		break;
		case 5:		// 101 = shift out by timer 2
			if ((t2TimedOutCount > 2) && shiftClockPositiveEdge && !(bitsShiftedSoFar & 8))
			{
				cb2Shift = (shiftRegister & 0x80) != 0;
				shiftRegister = (shiftRegister << 1) | cb2Shift;
				if (++bitsShiftedSoFar == 8)
					SetInterrupt(IR_SR);
				// should output cb1OutputShiftClock onto cb1
				// cb2Shift should output to cb2
				//	- R/!W (on the 2nd VIA could be dangerous)
			}
		break;
		case 6:		// 110 = shift out by phi2
			if (!(bitsShiftedSoFar & 8))
			{
				// should output cb1OutputShiftClock onto cb1
				cb1OutputShiftClock = !cb1OutputShiftClock;
				cb2Shift = (shiftRegister & 0x80) != 0;
				shiftRegister = (shiftRegister << 1) | cb2Shift;
				if (++bitsShiftedSoFar == 8)
					SetInterrupt(IR_SR);
				// cb2Shift should output to cb2
				//	- R/!W (on the 2nd VIA could be dangerous)
			}
		break;
		case 7:		// 111 = shift out by external clock
			// SHIFT REGISTER BUG not implemented
			// In both the shift in and shift out modes a liming condition may occur when the 6522 does not detect the shift pulse.
			// This no shift condition occurs when CB1 and phi2 are asynchronous and their edges coincide.
			if (cb1Old && !cb1)	// Negitive edge
			{
				if (!(bitsShiftedSoFar & 8))
				{
					// should output cb1OutputShiftClock onto cb1
					cb1OutputShiftClock = !cb1OutputShiftClock;
					cb2Shift = (shiftRegister & 0x80) != 0;
					shiftRegister = (shiftRegister << 1) | cb2Shift;
					if (++bitsShiftedSoFar == 8)
						SetInterrupt(IR_SR);
					// cb2Shift should output to cb2
					//	- R/!W (on the 2nd VIA could be dangerous)
				}
			}
		break;
	}
	cb1Old = cb1;
}

unsigned char m6522::Read(unsigned int address)
{
	unsigned char value = 0;

	switch (address & 0xf)
	{
		case ORB:
			value = ReadPortB();
			if (t1OutPB7)		// We need to see what we are setting eventhough we may not be outputting it (because off DDR)
			{
				if (!t1_pb7) value &= (~0x80);
				else value |= 0x80;
			}
		break;
		case ORA:
			value = ReadPortA(true);
		break;
		case DDRB:
			value = portB.GetDirection();
		break;
		case DDRA:
			value = portA.GetDirection();
		break;
		case T1CL:
			// A read T1CL transters the counter’s contents to the data bus and if a T1 interrupt has occurred the read	operation will clear the IFR flag and reset !IRQ
			ClearInterrupt(IR_T1);
			value = t1c.bytes.l;
		break;
		case T1CH:
			// A read T1CH transfers the counter's contents to the data bus.
			value = t1c.bytes.h;
		break;
		case T1LL:
			// A read of T1LL transfers the latch’s contents to the data bus; it has no	effect on the T1 interrupt flag.
			value = t1l.bytes.l;
		break;
		case T1LH:
			// A read of T1LH transfers the contents of the latch to the data bus.
			value = t1l.bytes.h;
		break;
		case T2CL:
			// A read of T2CL transfers the contents of the low order counter to the data bus, and if a T2 interrupt has occurred,
			// the read operation will clear the T2 interrupt flag and reset !IRQ.
			ClearInterrupt(IR_T2);
			value = t2c.bytes.l;
		break;
		case T2CH:
			// A read of T2CH transfers the contents of the high order counter to the data bus.
			value = t2c.bytes.h;
		break;
		case SR:
			value = shiftRegister;
			if (interruptFlagRegister & IR_SR) bitsShiftedSoFar = 0;
			ClearInterrupt(IR_SR);
		break;
		case ACR:
			value = auxiliaryControlRegister;
		break;
		case FCR:
			value = functionControlRegister;
		break;
		case IFR:
			value = interruptFlagRegister;
		break;
		case IER:
			value = interruptEnabledRegister | IR_IRQ;
		break;
		case ORA_NH:
			value = ReadPortA(false);
		break;
	}
	return value;
}

unsigned char m6522::Peek(unsigned int address)
{
	unsigned char value = 0;

	switch (address & 0xf)
	{
		case ORB:
			value = PeekPortB();
		break;
		case ORA:
			value = PeekPortA();
		break;
		case DDRB:
			value = portB.GetDirection();
		break;
		case DDRA:
			value = portA.GetDirection();
		break;
		case T1CL:
			value = t1c.bytes.l;
		break;
		case T1CH:
			value = t1c.bytes.h;
		break;
		case T1LL:
			value = t1l.bytes.l;
		break;
		case T1LH:
			value = t1l.bytes.h;
		break;
		case T2CL:
			value = t2c.bytes.l;
		break;
		case T2CH:
			value = t2c.bytes.h;
		break;
		case SR:
			value = shiftRegister;
		break;
		case ACR:
			value = auxiliaryControlRegister;
		break;
		case FCR:
			value = functionControlRegister;
		break;
		case IFR:
			value = interruptFlagRegister;
		break;
		case IER:
			value = interruptEnabledRegister | IR_IRQ;
		break;
		case ORA_NH:
			value = PeekPortA();
		break;
	}
	return value;
}

void m6522::Write(unsigned int address, unsigned char value)
{
	unsigned char ddr;

	switch (address & 0xf)
	{
		case ORB:
			WritePortB(value);
		break;
		case ORA:
			WritePortA(value, true);
		break;
		case DDRB:
			portB.SetDirection(value);
		break;
		case DDRA:
			portA.SetDirection(value);
		break;
		case T1CL:
		case T1LL:
			// Writing to the T1CL is effectively a write to the low order latch.
			// Writing T1LL stores an 8 bit count value into the latch. Effectively the same as a write to T1CL.
			// The data is held in the latch until the high order counter is written : at this time the data is transferred to the counter.
			t1l.bytes.l = value;
		break;
		case T1CH:
			// A write to TICH loads both the high order counter and high order latch with the same value.
			// Simultaneously the T1LL contents are transferred to the low order counter and the count begins.
			// If PB7 has been programmed as a TIMER 1 output it will go low on the phi2 following the write operation.
			// Additionally, if the T1 interrupt flag has already been set, the write operation will clear it.
			// The write to TICH initiates the countdown on the next ph2.
			t1l.bytes.h = value;
			t1c.value = t1l.value;
			t1Ticking = true;	// BruceLee needs this else it will not load.
			t1Reload = true;
			ClearInterrupt(IR_T1);
			t1FreeRunIRQsOn = true;
			t1TimedOut = t1c.value == 0;
			// By setting bit 7 in the ACH to a one, PB7 will be enabled as a one shot output. PB7 will go low immediately alter writing T1CH.
			t1_pb7 = true;
			if (t1OutPB7)
			{
				// With the output enabled(ACR7 = 1) a "write T1CH" operation will cause PB7 to go low. PB7 will return high when Timer 1 times out. The result is a single programmable width pulse.
				// To guarantee a valid output level on PB7. Bit 7 of DDRB must also be set to a one. ORB bit 7 will NOT affect the level on PB7.
				// TO CHECK - need to cache the old value of ORB bit 7?
				ddr = portB.GetDirection();
				if (ddr & 0x80)
					portB.SetOutput(portB.GetOutput() & (~0x80));
			}
			if (!t1FreeRun)	// If one shot mode then IRQ is back in play
				t1OneShotTriggeredIRQ = false;
		break;
		case T1LH:
			// A write to T1LH loads an 8 bit count value into the latch.
			t1l.bytes.h = value;
			// To clear or not to clear the IRQ flag?
			// There are a few documents that say a write to T1LH does not clear the IRQ.
			// Even Synertek's official FAQ doc (a document that was supposed to clear up the vagueness of the official datasheet) says that the flag is not cleared.
			// I have now discovered that it is indeed cleared and this clear is very important.
			// My take on it;-
			// Allowing the IRQ to be cleared here allows a programmer to keep T1 running in free run mode but NOT generate IRQs (this is an undocumented feature).
			// It can be useful to have T1 run in free run mode and utilise the benefits (ie timed pulses on PB7) but not incur the overhead of IRQs triggering.
			// The designers of the 6522 allow this mode by clearing the T1 interrupt triggering when T1LH is written to.
			if (!(interruptEnabledRegister & IR_T1))		// It appears that this only occurs if T1 IRQs are already disabled (else EOD refuses to load)
				t1FreeRunIRQsOn = false;
			ClearInterrupt(IR_T1);
		break;
		case T2LL:
			// Writing T2CL/T2LL effectively stores an 8 bit byte in a write only latch where it will be held until the count is initiated.
			t2Latch = value;
		break;
		case T2CH:
			// Writing T2CH loads an 8 bit byte into the high order counter and latch (!!!there is no t2lh!!!) and simultaneously loads the low order latch into the low order counter, and the count down is initiated.
			// If a T2 interrupt has occurred, the write operation will clear the T2 interrupt flag and reset !IRQ.
			t2c.bytes.h = value;
			t2c.bytes.l = t2Latch;
			t2Reload = true;
			t2TimedOutCount = 0;
			t2LowTimedOut = false;
			t2TimedOut = false;
			t2CountingDown = true;
			ClearInterrupt(IR_T2);
			t2OneShotTriggeredIRQ = false;
		break;
		case SR:
			shiftRegister = value;
			if (interruptFlagRegister & IR_SR) bitsShiftedSoFar = 0;
			ClearInterrupt(IR_SR);
			cb1OutputShiftClock = 1;
			cb1OutputShiftClockPositiveEdge = false;
		break;
		case ACR:
			//bool t1OutPB7Prev = (auxiliaryControlRegister & ACR_T1_OUT_PB7) != 0;
			auxiliaryControlRegister = value;
			latchPortA = (value & ACR_PA_LATCH_ENABLE) != 0;
			latchPortB = (value & ACR_PB_LATCH_ENABLE) != 0;
			// T1 will generate continuous interrupts when bit 6 of the ACR is a one.
			// In effect, this bit provides a link between the latches and counter; automatically loading the counters from the latches when time out occurs.
			// Note: when in this mode and !IRQ is enabled. !IRQ will go low after the first (and each succeeding) time out and stay low until either TICL is read or T1CH is written.
			t1FreeRun = (value & ACR_T1_MODE) != 0;
			t1OutPB7 = (value & ACR_T1_OUT_PB7) != 0;
			t2CountingPB6Mode = (value & ACR_T2_MODE) != 0;
			// A precaution to take in the use of PB7 as the timer output concerns the Data Direction Register contents for PB7.
			// Both DDRB bit 7 and ACR bit 7 must be 1 for PB7 to function as the timer output. 
			// If one is 1 and the other is 0, then PB7 functions as a normal output pin, controlled by ORB bit 7.
			// TODO when in this mode cache and track what is occuring in ORB7?
			if (t1OutPB7)
			{
				ddr = portB.GetDirection();
				//if (ddr & 0x80)
				//{
				//	// TO CHECK IN HW
				//	// If t1OutPB7 gets turned on before a time out what happens?
				//	// PB7 could become the cached value of the previously tracked PB7
				//	// PB7 could go low
				//	// PB7 could remain the value of ORB7 until the next time out
				//	if (!t1_pb7)
				//		portB.SetOutput(portB.GetOutput() & (~0x80));
				//	else
				//		portB.SetOutput(portB.GetOutput() | 0x80);
				//}
				if (!t1_pb7)
				{
					if (ddr & 0x80)	portB.SetOutput(portB.GetOutput() & (~0x80));
				}
				else
				{
					if (ddr & 0x80)	portB.SetOutput(portB.GetOutput() | 0x80);
				}
			}
			//else if (t1OutPB7Prev)
			//{
			//	// TODO: what if it was turned off before the timer times out?
			//	// What happens to PB7 in this case? It was low in one shot mode and can vary in free running mode, now what?
			//	// Should go back to the cached version of ORB7?
			//}
		break;
		case FCR:	// Peripheral Control Register
			functionControlRegister = value;
			if ((value & FCR_CA2_IO) == FCR_CA2_IO)
			{
				// ca2 is an output
				pulseCA2 = (value & (FCR_CA2_OUTPUT_MODE1 | FCR_CA2_OUTPUT_MODE0)) == FCR_CA2_OUTPUT_MODE0;
				ca2 = !pulseCA2 && (value & (FCR_CA2_OUTPUT_MODE1 | FCR_CA2_OUTPUT_MODE0)) == (FCR_CA2_OUTPUT_MODE1 | FCR_CA2_OUTPUT_MODE0);
			}
			else
			{
				// ca2 is an input
			}
			if ((value & FCR_CB2_IO) == FCR_CB2_IO)
			{
				// cb2 is an output
				pulseCB2 = (value & (FCR_CB2_OUTPUT_MODE1 | FCR_CB2_OUTPUT_MODE0)) == FCR_CB2_OUTPUT_MODE0;
				cb2 = !pulseCB2 && (value & (FCR_CB2_OUTPUT_MODE1 | FCR_CB2_OUTPUT_MODE0)) == (FCR_CB2_OUTPUT_MODE1 | FCR_CB2_OUTPUT_MODE0);
			}
			else
			{
				// cb2 is an input
			}
		break;
		case IFR:
			ClearInterrupt(value);
		break;
		case IER:
			// If bit 7 is a 0, each 1 in bits 6 through 0 clears the corresponding bit in the IER.
			// For each zero in bits 6 through 0, the corresponding bit is unaffected.
			if (value & IR_IRQ) interruptEnabledRegister |= value;
			else interruptEnabledRegister &= (~value);
			interruptEnabledRegister &= (~IR_IRQ);
			OutputIRQ();
		break;
		case ORA_NH:
			WritePortA(value, false);
		break;
	}
}
