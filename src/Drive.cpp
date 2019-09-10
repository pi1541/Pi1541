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

#include "Drive.h"
#include "m6522.h"
#include "debug.h"


//#define PROFILE 1

#if defined(PROFILE)
extern "C"
{
#include "performance.h"
}
#endif

// There is a lot going on even though the emulation code is extremely small.
// A few counters, shift registers and the occasional logic gate takes a surprisingly small amount of code to implement.

// Note: A 1541 is an upgraded (cost reduced) version of the 1540.
//       In a 1541 a lot of the drive logic is implemented in a custom asic chip 235572. Internally, this chip contains essentially the same logic that was used in the 1540.
//       Commodore chose to reduce the chip count to also reduce power consumption, heat, and production costs.
//       Schematics for the 1540 drive logic made this emulator possible. The 1540 logic was emulated hence chip names reference the 1540.
//       I have a VIC-1541 drive with a 1540 type board in it. The chips are laid out in columns A through to H.

// Considering how prolific magnetic storage has been for many decades there is surprisingly little information (retaliative to other computer hardware like CPUs) into how disk drives actually work.
// This is how I understand it (and I apologise for the verbosity but it helped me come to that understanding).
//
// Magnetic media can provide simple, inexpensive and reliable storage medium.
// Reliability being a challenge considering that disks can vary between different disk manufactures and brands.
// Physical properties of drives, like alignment and rotation speed can vary slightly from drive to drive. These properties can even vary overtime or due to temperature fluctuations within the same drive.
// Digital information needs to be stored via an analogue system that can tolerate minor inconsistencies between theses physical properties.
//
// A magnetic field has two important characteristics flux density, and polarity (North and South Poles).
// At first glance, the two magnetic polarities, N and S, look ideal to nicely to represent a 1 or 0.
// Hall effect sensors can detect flux density of a magnetic field (even a constant one) but generally require stronger flux densities than found on the coating of floppy media can provide, so are not used to read data.
// And unfortunately, hall effect sensors have no way of writing data only reading it.
//
// Maxwell–Faraday law/equation states that a time varying magnetic field will induce a time varing electric field and visa versa.
// Disks drive heads use this principal, and can therefore, read and write data.
// Yet they can only detect/read and create/write changes in the disk's analogue magnetic field over time.
// The larger and quicker the change the easier it is to detect and read.
// Writing works in reverse where a changing electric signal creates a changing magnetic field.
//
// So how are digital 1s and 0s represented using this analogue magnetic field?
// A 1 is represented by the changing analogue magnetic field changing from N to S and visa versa.
// But what about the 0s? A 0 is represented by how quickly the magnetic field changes between any two 1 bits.
// So technically only the 1s are written to the disk. 0s are infered from the rate at which the magnetic field changes between two 1 bits.
// The slower the analogue change between N-S/S-N change the more 0s there will be in between the 1s.
// Long groups of consecutive 0s will eventually slow the rate of change down to a point where the magnetic field becomes almost constant.
// A constant magnetic field will stop induction.
// Due to the way the amplification circuits operate (explained below), an almost constant magnetic field can become problematic producing false readings.
//
// To solve the problem of preventing long groups of 0s and to keep the magnetic field varing over time various data encoding schemes were created.
// These data encoding schemes sacrifice disk density for maintaining a rapidly changing, easy to read magnetic field.
// Originally, up to 50% of a disk's density was sacrificed for software encoding schemes. Interleaving clock bits with data bits.
// Then some bright sparks invented other more efficient schemes, almost doubling the density of the of the disk (as space for the clock bits could now be used for data).
// Some marketing genius picked up on the words “double density” and the name stuck, even though the disk was always capable of storing the exact same number of bits!
// A encoding scheme commonly used at the time of the 1541 is called GCR.
// GCR encodes 4 bits into 5 where any combination cannot produce no more than two 0s in a row. This sacrifices only 20% of the disk's density.
// There is no drive hardware that dictates that any specific encoding scheme nees to be used.
// Software is free to do whatever it wants, even change the encoding schemes used over different parts of the disk or even within a single track.
// But any coding scheme that allows long groups of 0s will experience side effects.
// Side effects of long groups of 0s can cause a random 1 to be read incorrectly (explained below).
// Many copy protection schemes rely on these side effects occurring.
// The decoder in the 1541 is also hard coded to output a 1 after every three 0s even if there is no magnetic field change (flux reversal). So a 1541 cannot read more than three 0s in a row.
//
// A read/write head is a coil with a centre tap effectively creating two coils. One coil detects/creates magnetic field changes in one direction (N-S) and the other coil in the other direction (S-N).
// There is also a third coil use to erase.
// The difference between the electric signals in each of the two coils is amplified.
// Remember that we are only interested in the rate in which the magnetic field is changing not its relative amplitude.
// Drives vary slightly and even the same drive can vary with temperature.
// Because of this and to increase alignment tolerance the relative amplitude of the magnetic field can be ignored. The rate of change is important.
// The amplifier therefore amplifies the signal with a varying gain. This is called 'Automatic Gain Control'.
// It is always trying to amplify the signal as much as it needs. For a strong signal not as much as for a weak signal.
//
// The signal is then filtered. A low-pass filter attenuates noise and unwanted harmonics.
//
// In a 1540;-
// This is done by L8-L11, and C16.
//
// To detect the all important change in magnetic field direction (ie a flux reversal) the signal is again amplified.
// But this time the amplifier has been configured as a differentiator as well.
// A differentiator amplifier produces an output signal that is the first derivative of the input signal.
// That is, the output signal is directly proportional to the rate of change of the input signal.
// This means that every place in the input signal where there is a peak or trough (ie the signal changes direction, namely, a flux reversal) the new signal will cross the zero point (in the analogue waveform).
//
// The differentiated signal can now be analysed. A zero crossing represents a 1. The absence of a zero crossing over a certain time threshold represents a 0. 
// The problem then becomes how to detect zero crossings and time them.
// Detecting zero crossings is solved using another operational amplifier configured as a comparator with a reference voltage of zero.
// The comparator output toggles each time its inputs sense a zero crossing. The signal now being analysed is a simple square wave.
//
// In a 1540;-
// Heads read 7mv.
// First amp UH7 amplifies this to 350mv.
// Second amp UH5 amplifies this and differentiates it to 3.5v.
// Comparator UH4 and the pulse generator output 5v TTL.
//
// The comparator output is then applied to the time domain filter.
// The primary purpose of the time domain filter is to generate an output pulse for each transition of the comparator output.
// These pulses correspond to valid 1s being read off the disk and the absence of pulses the infered 0s.
// The secondary purpose is to filter out false zero crossings which can be caused by noise aggravated by media defect and contamination.
// Filtering out false information involves the use of an "ignore window", during which time any zero crossings are ignored.
//
// The time domain filter consists of a two one shots, and a D type flip flop.
// Each transition of the comparator output triggers the first one shot, which provides the ignore window.
// When the output of the first one shot returns to the normal level, the D type flip flop clocks through the new data (from the comparator), causing the Q and !Q outputs to switch.
// This in turn triggers the second one shot, to produce an output pulse.
// However, the second one shot will only be triggered if the comparator output has changed since the last clocking of the D type flip flop, and remained at that new level.
// If the comparator changed level due to noise on a slowly changing magnetic field and returned to its original level within the first one shot's period, the D type flip flop outputs will not change, and the second one shot will not generate a new output pulse.
//
// Thus, the zero crossing detector and the pulse generator result in pulses occurring at the original input signal's peak points (ie flux reversals).
//
// So in a 1540;-
// The output of the comparator is applied to the valid pulse detector(UG2D, UG3A, and UF6A).
// UG2D, along with C27, R24, and R25, forms an edge detector.
// Pin 11 of UG2D will produce a 500 nanosecond active high pulse on rising edges of the comparator's output and will produce a 150 nanosecond high pulse on falling edges of the comparator's output.
// The pulse out of UG2D is applied to UG3A, a single shot multivibrator. 
// UG3A produces an active low pulse which is approximately 2.5 microseconds wide. 
// Flip flop UF6A is clocked on the trailing edge of the output pulse of UG3A.
// The output of the comparator must be maintained for at least 2.5 microseconds in order to be latched into the flip flop.
// If a narrow noise pulse triggers this circuit, the output of the flip flop will not change since the noise pulse will terminate before UG3A triggers UF6A.
// The output of the flip flop (UF6A) will reflect the data delayed by approximately 2.5 microseconds.
// The valid data out of the valid pulse detector is applied to an edge detector consisting of UG2A and UG3B.
// UG2A operates the same as UG2D above.
// The pulses out of UG2A trigger UG3B, a single shot multivibrator.
// UG3B produces a narrow pulse, which represents transitions of the data.
// This output is applied to the timing circuit to synchronise the encoder / decoder clock
//
//
// The problem of zero crossing (hence flux reversals) is solved, now 1s can be detetected. Detecting 0s now becomes a task of timing the gap between the 1s.
//
// After all this explanation, NONE of this needs to be emulated!
//
// Up to this point the hardware explained so far is responsible for converting the analogue siganls into clean digital signals.
// Emulated disk images already consist of digital information and hence the hardware explained so far has to some degree already been emulated.
// What does need to be emulated though (at least simulated) is the side effects the hardware can produce.
//
//
// Side effects
// When software uses a valid encoding scheme long runs of 0 are avoided. 
// Even if the disk is old or the drive is slightly miss alligned or varying in rotation speed (within a certain range) there should not be a problem reading the data.
// But there is still nothing to prevent the magnetic field on a disk changing slowly.
// So what happens when the magnetic field is changing slowly but just fast enough for induction to be detected?
// The first amplifier will detect the weak induced signal and maximise its gain. With a maximised gain noise can become a problem.
// The slope of the slowly changing signal will be differentiated to produce a signal that will be near zero.
// A low amplitude signal varying near zero is fine and valid but a low amplitude signal varying around zero is bad as each zero crossing will be detected incorrectly as a 1.
// So any noise on the differentiated signal could cause it to cross the zero point causing the comparator to switch.
// The time domain filter will normally filter most of the high frequency false crossings out.
// But if these random zero crossing just happen to occur at the normally expected rate of valid flux reversals, random 1s, along with the groups of 0s, will be read.
// On a signal that is near zero these random noise frequencies can and do occur at the normally valid ranges.
// Copy protection schemes rely on this and deliberately place a slow changing magnetic field on the disk.
// The software then reads that section of the disk multiple times. Each time comparing the values, looking for the random 1s.
// If the 1s are consistent then it knows that the field on the disk is no longer the slowly changing one of the original but that of a faster changing one of a copy.
// This side effect is simulated by the code below.
//
// Now all that is left to solve is the problem of timing.
// The data on the disk is divided into cells. If a transition occurs at the beginning of a cell, the cell is interpreted as a logic 1. If no transition occurs, the cell is interpreted as a logic 0.
// We just need to time the cell. The cell handling is the responsibility of the encoder/decoder (for writing/reading).
//
// For the standard CBM format more information (hence more cells) are stored on the outside of a disk than the inside.
// This variation keeps the bit density relatively constant over the surface of the disk.
// The disk spins at a near to constant rate.
// So for a round disk there is more surface area on the outside tracks than the inside.
// The timing of the cells and hence the encoder/decoder's clock can vary upto four levels (set via 2 pins in the VIA($1C00) PB5 and PB6).
// The encoder/decoder is usually clocked at a fester rate when on the outer tracks than when on the inner tracks.
// It is the encoder/decoder clock which determines how many bits will fit on any one track.
//
// The core clock generator inside a 1541 is a 16Mz crystal. All timings including the encoder/decoder clock is derived from this.
// 
// The encoder/decoder clock is simply a counter. UE7 a 74LS193 Synchronous 4-Bit Up/Down Counter.
// If counting at 16Mz it will take 16 cycles to count to 16 and therefore overflow/trigger a carry every 1us (ie 1MHz)
// It always counts at the same rate but can be made to trigger quicker if it is preloaded with an initial count each time it begins a new count.
// This preloading value comes from the CLOCK_SEL_AB pins of the VIA's PB5 and PB6 (set via the CPU) and can therefore be preloaded with a value of 0 to 3.
// The programmable divider is controlled by the CLOCK SEL A and CLOCK SEL B lines and selects the density level.
// Density/Bit Rate Index				3		2		1		0
// CLOCK SEL A							1		0		1		0
// CLOCK SEL B							1		1		0		0
// 16MHz Division Factor				13		14		15		16
// Encoder / Decoder Clock Freq (MHz)	1.2307	1.1428	1.0666	1.00
// Sectors per Track					21		20		18		17
// Track Numbers						1 - 17	18 - 24	25 - 30	31 - 42
// Division Factor/16					0.8125	0.875	0.9375	1
// Bit cell length in us (MAX)			3.25	3.5		3.75	4
// (4xDivision Factor/16)
//
//
// Nothing in the hardware limits what timings are required for any section of the disk. Software is free to change the timing at any time for any section of the disk (even within the same track).
//
// The encoder/decoder itself is made up of a four bit counter (UE4) and a NOR gate (UE5A).
// The counter is configured to always count up, clocked by the encoder/decoder clock.
// Any flux reversal either true or induced by noise will reset the encoder/decoder's counter (UE4) back to zero and encoder/decoder clock's counter (UE7) back to its preload density values.
// The encoder/decoder counter has 4 outputs A, B, C and D. They represent the bit value of the UE4's current count where A is the least significant bit and D the most.
// The B output forms the serial data clock.
// The C and D are NORed together and the output forms the serial data.
// The serial data is fed into the Read Shift Register (UD2) where it is converted from serial to parallel and can be gated onto the VIA's portA via the read latch UC3.
//
// A bit cell is four encoder/decoder clock pulses wide, as the 2nd bit (output B) in UF4's count controls the serial clock. Based on the VIA's density preload values a cell is at a maximum 3.25-4us long (shorter if it contains a flux reversal).
// If a flux reversal occurs at the beginning of a cell, that cell is a 1 else that cell is a 0.
// If a flux reversal occurs, UF4's counter is cleared and the timing circuit is reset to start the encoder/decoder clock at the beginning of the VIA's current density setting.
// To phase lock onto data we must always begin with a 1 hence the need for a soft SYNC marker full of 1s.
//
// When B becomes high the NORed value of C and D (serial data) is sent to the shift register.
// If there is a flux reversal the counter resets. Two counts later a one is always shifted in.
// If after the first flux reversal there is an absence of others, UE4 continues to count up and each time B goes high (counts 6 10 & 14) only 0s will be shifted in (at the rate dictated by the encoder/decoders's clock and its density preload setting).
// If UE4's count is left to cycle (ie no flux reversals for 16 counts) a 1 will be shifted in regardless.
// This limits the hardware to only read three consecutive 0s between any two 1s. (with GCR this will never occur)
//
// The 1541 uses soft sectors. The hardware can only detect the sync sequence of ten one bits in a row and everything must then be phase locked to that.
// The sync sequence phase locks to blocks. Blocks can really be any size, even the entire track.
// UC2 monitors the parallel output of UD2/UE4, when all 10 bits are 1, the output pin 9 goes low indicating a SYNC sequence has been read.
// Whenever the SYNC is asserted UE3 is reset and we are also phase locked to bytes.
// UE3 continues the phase lock to bytes as it is another counter responsible for counting 8 serial clocks.
// When 8 bits have been counted, UF3 pin 12 goes low, UC1 pin 10 goes high, and UF3 pin 8 goes low indicating a byte is ready to be read by the CPU.
//
// And as we have already seen the flux reversals phase lock to bits.
//
// So now the analogue signal can be phase locked onto and the data read/written in a bit stream, despite minor physical inconsistencies between disks and drives.


// The Encoder/Decoder Clock
// UE7 74LS193 Synchronous 4-Bit Up/Down Counter
//		- clocks UF4 
//		- counts up on pin 5 and this is connected to 16Mhz clock
//			- never counts down as pin 4 is tied to VCC
//		- inputs A and B are connected to VIA's PB5/6 CLOCK_SEL_AB input C and D to GND
//		- pin 12 !CO (!carry out) is output to UE5C (NOR gate used as an inverter) and the inverted signal is called "Encoder/Decoder Clock"
//			- carry out is the only output used.
//		- pin 14 CLR is grounded so the count is never cleared this way
//		- pin 11 LOAD is NORed (UE5D) with our inverted !CO (pin 12) and "Bit Sync" signal. "Bit Sync" is data from the read amplifier, differentiator, compatitor, time domain filter and pulse generator.
//			- the counter resets to VIA's current PB5/6 CLOCK_SEL_AB
//
// The Encoder/Decoder
// UF4 74LS193 Synchronous 4-Bit Up/Down Counter (used to provide clocks for the UD2 serial to parallel shifter and the UE3 bit counter)
//		- counts up on pin 5 and this is connected to the inverted UE7's carry out ie "Encoder/Decoder Clock"
//			- never counts down as pin 4 is tied to VCC
//		- pin 11 !LOAD is tied to VCC so the count will never be altered this way
//		- pin 14 CLR receives data from the read amplifiers (ie "Bit Sync")
//			- so a 1 will reset the count
//		- outputs D and C are connected to a NOR gate UE5A
//		- output A
//		- output B ie every 2nd count
//			drives UD2 shifter
//
// Byte Phase Lock
// UE3 74LS191 Presettable 4-Bit Up/Down Counter
//		- pin 14 CLOCK clocked by 
//			- when UF4 counts 2, 6, 10 and 14 (2 is the only count that outputs a 1 into readShiftRegister)
//			- Will count 8 bits everytime UF4 cycles twice
//		- pins 4 (!ENABLE) and 5 (DOWN/UP) connected to GND
//			- puts it in count up mode constantly
//		- pin 7 (OUTPUT D) is NC
//			- eventhough it can count to 16 the high bit is ignored and system treats it as a don't care state
//			- low 3 bits only makes it a 3 bit counter that has 8 counts
//		- pin 11 !LOAD
//			- this is connected to the output of UC2 74ls133 a 13 input NAND gate
//				- 2 inputs are connected to VCC
//				- 8 inputs are connected to 
//				- 2 inputs are connected to 
//				- 1 input is coneeced to read/write signal
//					- UC2 output can only asserted when reading
//				- the output of UC2 is then called !BLOCK SYNC
//		- pins 15, 1 and 10 inputs A,B and C are connected to GND. Pin 0 input D NC and as a don't care state
//			- so, resets back to zero when !LOAD is asserted
//				- ie when in SYNC will always reset until the first 0 bit
//		- pins 3, 2 and 6 outputs A, B and C (pin 7 output D is NC)
//			- NANDed together by UF3A and this is inverted by UC1E 74ls06
//				- this then forms part of UF3B's NAND inputs
//					- along with UF4 output A and B 
//					- this will only activate when (ie UD3 Parallel to serial shift register will only latch when)
//						- UE3 counts 7 AND UF4 counts 3, 7, 11, 15 (outputs A and B)
//					- UF3B output goes to UD3 pin 1 LOAD(LATCH)
//				- Also this then forms part of UF3C's NAND inputs
//					- along with "Byte Sync Enable" (from the VIA) and 
//						- UC5B (NOR used to invert UF4's output B) output high when UF4 counts 0,1,4,5,8,9,12 and 13
//						- (IS THIS AN OVERSIGHT IN VICE? They don't emulate it and trigger BYTE SYNC when the serial clock is high?)
//					- UF3C output is then called !BYTE SYNC
//					
// Read Shift Register
// UD2 74LS164 8-Bit Serial-In Parallel-Out Shift Register
//		- Clocking occurs on the LOW-to-HIGH level transition of the clock input and when so, the value at the inputs is entered
//		- pin 8 CLOCK - shifts the data when UF4 Counter output B (rising edge)
//			- ie when UF4 counts 2, 6, 10 and 14 (2 is the only count that outputs a 1 into readShiftRegister)
//			- so will only shift in a 1 on UF4 counts 2
//				- on counts 6, 10 and 14 the UE5A NOR will output a 0 to pin 2 input B so no transition
//		- pin 2 input B from UF4's NORed outputs C and D
//		- pin 1 input A connected to VCC
//			- so has no impact on the data shifted in via input B
//		- On counts 2, 6, 10 and 14 of UF4 (2 is the only count that outputs a 1 into readShiftRegister)
//			- will shift the data
//				ie but ony a 1 will be shifed in on count 2
//		- outputs Q0-Q7
//
// Sync Phase Lock
// UC2 74LS133 13 input Nand Gate
// UE4A/B 74LS74 Flop flops (These are used as an extention to UD2 shift register to increase the number of bits to 10)
//		- UD2 feeds in UC2 with 8 lines
//		- UE4A/B another 2
//		- R/!W another 1
//		- the other 2 inputs of UC2 are pulled high so SYNC is triggered only when reading and 10 consecutive 1 have been detected.
//
// Read Latch
// UC3 74LS245 Octal Bus Transceiver
//		- Data read and shifted into UD2 is latched here.
//		- Reading PortA of the VIA will read the values in this latch.
//		- Hardware requires this latch to isolate the output of UD2 from VIA PortA when VIA PortA is set to output ie writing.
//		(Note: the emulater does not need to emulate this component as its value will always reflected in the read shift register UD2)
//
// Write Shift Register
// UD3 74LS165 8-BIT Parallel to serial shift register
//		- pin 2 CLOCK from  UF4 Counter output B (rising edge)
//			- ie when UF4 counts 2, 6, 10 and 14 (2 is the only count that outputs a 1 into readShiftRegister)
//		- pin 1 LOAD form UF3B output
//				- this will only activate when 
//					- UE3 counts 7 AND UF4 counts 3, 7, 11, 15 (outputs A and B)
//

// For some insane reason some demo coders use the write protect sensor to detect disk swapping! It is such a lame way to do it. Just use the disk ID in the sector header that's what it is for!
// I don't think I have seen any games that do this only demos. And the most popular ones tend to do it.
// When you eject a disk the disk will start to move out of the drive. Its write protect notch will no longer line up with the write protect sensor and the VIA will signal that the write protection came on.
// When the disk becomes totally ejected there is now nothing blocking the write protect sensor and the VIA will signal that the write protection was turned off.
// Now when the next disk is inserted the top of the disk will obscure the write protect sensor's LED and again the VIA will signal that the write protection came on.
// When the newly inserted disk finally comes to rest its write protect notch will now line up over the write protect sensor and again the VIA will signal that the write protection was turned off.
//
// Because of a few demos using this ridiculous technique we need to emulate the changing write protect status using a timer.
// Using a real drive you can actually break some of these demos (during the disk swap) just by the manual way you eject the disks.
#define DISK_SWAP_CYCLES_DISK_EJECTING 400000
#define DISK_SWAP_CYCLES_NO_DISK 200000
#define DISK_SWAP_CYCLES_DISK_INSERTING 400000

Drive::Drive() : m_pVIA(0)
{
	srand(0x811c9dc5U);
#if defined(EXPERIMENTALZERO)
	localSeed = 0x811c9dc5U;
#endif
	Reset();
}

void Drive::Reset()
{
#if defined(EXPERIMENTALZERO)
	LED = false;
	cyclesForBit = 0;
	UE7Counter = 16;
#endif
	headTrackPos = 18*2;		// Start with the head over track 19 (Very later Vorpal ie Cakifornia Games) need to have had the last head movement -ve
	CLOCK_SEL_AB = 3;		// Track 18 will use speed zone 3 (encoder/decoder (ie UE7Counter) clocked at 1.2307Mhz)
	UpdateHeadSectorPosition();
	lastHeadDirection = 0;
	motor = false;
	SO = false;
	readShiftRegister = 0;
	writeShiftRegister = 0;
	UE3Counter = 0;
#if defined(EXPERIMENTALZERO)
	ResetEncoderDecoder(18 * 16, 4 * 16);
	cyclesLeftForBit = ceil(cyclesPerBit - cyclesForBit);
#else
	ResetEncoderDecoder(18.0f, 22.0f);
#endif
	newDiskImageQueuedCylesRemaining = DISK_SWAP_CYCLES_DISK_EJECTING + DISK_SWAP_CYCLES_NO_DISK + DISK_SWAP_CYCLES_DISK_INSERTING;
	m_pVIA->InputCA1(true);	// Reset in read mode
	m_pVIA->InputCB1(true);
	m_pVIA->InputCA2(true);
	m_pVIA->InputCB2(true);
}

void Drive::Insert(DiskImage* diskImage)
{
	Eject();
	this->diskImage = diskImage;
	newDiskImageQueuedCylesRemaining = DISK_SWAP_CYCLES_DISK_EJECTING + DISK_SWAP_CYCLES_NO_DISK + DISK_SWAP_CYCLES_DISK_INSERTING;
}

void Drive::Eject()
{
	if (diskImage) diskImage = 0;
}

void Drive::DumpTrack(unsigned track)
{
	if (diskImage) diskImage->DumpTrack(track);
}

// Signals from the VIA.
void Drive::OnPortOut(void* pThis, unsigned char status)
{
	Drive* pDrive = (Drive*)pThis;
	if (pDrive->motor)
		pDrive->MoveHead(status & 3);
	pDrive->motor = (status & 4) != 0;
	pDrive->CLOCK_SEL_AB = ((status >> 5) & 3);
	pDrive->LED = (status & 8) != 0;
}

bool Drive::Update()
{
#if defined(PROFILE)
	perf_counters_t pct;
	reset_performance_counters(&pct);

#if defined(RPI2) || defined(RPI3) 
	pct.num_counters = 6;
	pct.type[0] = PERF_TYPE_L1I_CACHE;
	pct.type[1] = PERF_TYPE_L1I_CACHE_REFILL;
	pct.type[2] = PERF_TYPE_L1D_CACHE;
	pct.type[3] = PERF_TYPE_L1D_CACHE_REFILL;
	pct.type[4] = PERF_TYPE_L2D_CACHE_REFILL;
	pct.type[5] = PERF_TYPE_INST_RETIRED;
	pct.counter[0] = 100;
	pct.counter[1] = 101;
	pct.counter[2] = 102;
	pct.counter[3] = 103;
	pct.counter[4] = 104;
	pct.counter[5] = 105;
#else
	pct.num_counters = 2;
	//pct.type[0] = PERF_TYPE_EVERY_CYCLE;
	pct.type[0] = PERF_TYPE_I_CACHE_MISS;
	pct.type[1] = PERF_TYPE_D_CACHE_MISS;
#endif

#endif

	bool dataReady = false;
	
	// When swapping some lame loaders monitor the write protect flag.
	// Bit 4 of PortB (WP - write protect) should be;
	// X Write protect status of D1
	// 0 Write protected (D1 ejecting)
	// 1 Not write protected (no disk)
	// 0 Write protected (D2 inserting)
	// X Write protect status of D2
	if (newDiskImageQueuedCylesRemaining > 0)
	{
		newDiskImageQueuedCylesRemaining--;
		if (newDiskImageQueuedCylesRemaining == 0) m_pVIA->GetPortB()->SetInput(0x10, !diskImage->GetReadOnly()); // X Write protect status of D2
		else if (newDiskImageQueuedCylesRemaining > DISK_SWAP_CYCLES_NO_DISK + DISK_SWAP_CYCLES_DISK_INSERTING) m_pVIA->GetPortB()->SetInput(0x10, false); // 0 Write protected (D1 ejecting)
		else if (newDiskImageQueuedCylesRemaining > DISK_SWAP_CYCLES_DISK_INSERTING) m_pVIA->GetPortB()->SetInput(0x10, true); // 1 Not write protected (no disk)
		else m_pVIA->GetPortB()->SetInput(0x10, false); // 0 Write protected (D2 inserting)
	}
	else if (diskImage && motor)
	{
		unsigned char FCR = m_pVIA->GetFCR();
		bool writing = ((FCR & m6522::FCR_CB2_OUTPUT_MODE0) == 0) && ((FCR & m6522::FCR_CB2_IO) != 0);

		if (SO)
		{
			dataReady = true;
			SO = false;
		}
		// UE6 provides the CPU's clock by dividing the 16Mhz clock by 16.
		// UE7 (a 74ls193 4bit counter) counts up on the falling edge of the 16Mhz clock. UE7 drives the Encoder/Decoder clock.
		// So we need to simulate 16 cycles for every 1 CPU cycle
#if defined(EXPERIMENTALZERO)
		if (writing)
			DriveLoopWrite();
		else
		{
			if (fluxReversalCyclesLeft > 16 && cyclesLeftForBit > 16)
			{
				DriveLoopReadNoFluxNoCycles();
			}
			else if (fluxReversalCyclesLeft > 16)
			{
				DriveLoopReadNoFlux();
			}
			else if (cyclesLeftForBit > 16)
			{
				DriveLoopReadNoCycles();
			}
			else
			{
				DriveLoopRead();
			}
		}
#else
		for (int cycles = 0; cycles < 16; ++cycles)
		{
			if (!writing)
			{
				if (++cyclesForBit >= cyclesPerBit)
				{
					cyclesForBit -= cyclesPerBit;
					// Any 1 bit coming from the disk will come in the form of a flux reversal. (Non return to zero inverted emulation.)
					if (GetNextBit())
					{
						// We have a genuine flux reversal.
						// Pin 12 of UE5D is the BIT SYNC Input. When a positive pulse is applied to pin 12, the output of UE5D(pin 13) is applied to the load line (of UE7),
						// causing the encoder/decoder clock to terminate the current cycle early and begin a new one.
						ResetEncoderDecoder(18.0f, 20.0f); // Start seeing random flux reversals 18us-20us from now (ie since the last real flux reversal).
					}
				}
				// The video amplifiers will often oscillate with no data in, but these oscillations are high enough in frequency that they "seldom" get past the valid pulse detector.
				// Some do and some copy protections rely on this random behaviour so we need to emultate it.
				// For example, 720 will read a byte from the disk multiple times and check that the values read each time were infact different. It does not matter what the values are just that they are different.
				randomFluxReversalTime -= 0.0625f;	// One 16th of a micro second.
				if (randomFluxReversalTime <= 0) ResetEncoderDecoder(2.0f, 25.0f); // Trigger a random noise generated zero crossing and start seeing more anywhere between 2us and 25us after this one.
			}
			if (++UE7Counter == 0x10) // The count carry (bit 4) clocks UF4.
			{
				UE7Counter = CLOCK_SEL_AB;	// A and B inputs of UE7 come from the VIA's CLOCK SEL A/B outputs (ie PB5/6) ie preload the encoder/decoder clock for the current density settings.
				// The decoder consists of UF4 and UE5A. The ecoder has two outputs, Pin 1 of UE5A is the serial data output and pin 2 of UF4 (output B) is the serial clock output.
				++UF4Counter &= 0xf; // Clock and clamp UF4.
				// The UD2 read shift register is clocked by serial clock (the rising edge of encoder/decoder's UF4 B output (serial clock))
				//	- ie on counts 2, 6, 10 and 14 (2 is the only count that outputs a 1 into readShiftRegister as the MSB bits of the count NORed together for other values are 0)
				if ((UF4Counter & 0x3) == 2)
				{
					// A bit cell is four encoder/decoder clock pulses wide, as the 2nd bit of UF4 controls the serial clock (and takes 4 cycles to loop a two bit counter).
					// If a flux reversal (or pulse into the decoder) occurs at the beginning of a cell, that cell is a 1 else that cell is a 0.
					// If a flux reversal occurs, UF4's counter is cleared and the timing circuit is reset to start the encoder/decoder clock at the beginning of the VIA's current density setting.
					// Pins 6 (output C) and 7 (output D) of UF4 are low, causing the output of UE5A, the serial data line, to go high.
					// 2 encoder/decoder clock pulses later, the serial clock(pin 2 of UF4) goes high. When the serial clock line is high, the serial data line is valid and the shift register will shift in the data.
					// The serial clock line remains high for another clock cycle.
					// After four encoder/decoder clocks a bit cell is now complete.
					// At this time, pins 2 (output A) and 3 (output B) of UF4 will again be low but as the count is counting up pin 6 (output C) will now be high.
					// The high on pin 6 (output C) of UF4 causes the serial data line (pin 1 of UE5A) to go low as this is NORed with the low on pin 7 (output D).
					// If a flux reversal occurs at the beginning of the next cell then everything resets and again we see a 1 on the serial data line 2 encoder/decoder cycles into that cell.
					// If no flux reversal occurs at the beginning of the next cell, the serial data line will remain low when the serial clock line goes high again (two encoder/decoder clock cycles into the new cell).
					// If there are no flux reversals for 2 cells then we see 0 on pin 6 (output C) and 1 on pin 7 (output D) of UF4 and this causes the serial data line (pin 1 of UE5A) to remain at 0.
					// If there are no flux reversals for 3 cells then we see 1 on pin 6 (output C) and 1 on pin 7 (output D) of UF4 and this causes the serial data line (pin 1 of UE5A) to also remain at 0, after all, UE5A is a NOR gate.
					// After 4 cells the counter inside UF4 loops back to 0 and we again see 0 on pin 6 (output C) and 0 on pin 7 (output C), causing the output of UE5A, the serial data line, to go to a 1, regardless of a true flux reversal!
					readShiftRegister <<= 1;
					readShiftRegister |= (UF4Counter == 2); // Emulate UE5A and only shift in a 1 when pins 6 (output C) and 7 (output D) (bits 2 and 3 of UF4Counter are 0. ie the first count of the bit cell)
					if (writing) SetNextBit((writeShiftRegister & 0x80));
					writeShiftRegister <<= 1;
					// Note: SYNC can only trigger during reading as R/!W line is one of UC2's inputs.
					if (!writing && ((readShiftRegister & 0x3ff) == 0x3ff))	// if the last 10 bits are 1s then SYNC
					{
						UE3Counter = 0;	// Phase lock on to byte boundary
						m_pVIA->GetPortB()->SetInput(0x80, false);			// PB7 active low SYNC
					}
					else
					{
						if (!writing) m_pVIA->GetPortB()->SetInput(0x80, true); // SYNC not asserted if not following the SYNC bits
						UE3Counter++;
					}
				}
				// UC5B (NOR used to invert UF4's output B serial clock) output high when UF4 counts 0,1,4,5,8,9,12 and 13
				else if (((UF4Counter & 2) == 0) && (UE3Counter == 8))	// Phase locked on to byte boundary
				{
					UE3Counter = 0;
					SO = (m_pVIA->GetFCR() & m6522::FCR_CA2_OUTPUT_MODE0) != 0;	// bit 2 of the FCR indicates "Byte Ready Active" turned on or not.
					if (writing) 
					{
						writeShiftRegister = m_pVIA->GetPortA()->GetOutput();
					}
					else
					{
						writeShiftRegister = (u8)(readShiftRegister & 0xff);
						m_pVIA->GetPortA()->SetInput(writeShiftRegister);
					}
				}
			}
		}
#endif
	}
	m_pVIA->InputCA1(!SO);

#if defined(PROFILE)
	read_performance_counters(&pct);
	print_performance_counters(&pct);
#endif

	return dataReady;
}


#if defined(EXPERIMENTALZERO)
#define min(a,b) (((a) < (b)) ? (a) : (b))
#define max(a,b) (((a) > (b)) ? (a) : (b))

void Drive::DriveLoopReadNoFluxNoCycles()
{
	unsigned int cycles = 16;
	fluxReversalCyclesLeft -= 16;
	cyclesLeftForBit -= 16;
	while (true)
	{
		if (cycles < UE7Counter)
		{
			UE7Counter -= cycles;
			cycles = 0;
			return;
		}
		cycles -= UE7Counter;

		UE7Counter = 16 - CLOCK_SEL_AB;	// A and B inputs of UE7 come from the VIA's CLOCK SEL A/B outputs (ie PB5/6) ie preload the encoder/decoder clock for the current density settings.
									// The decoder consists of UF4 and UE5A. The ecoder has two outputs, Pin 1 of UE5A is the serial data output and pin 2 of UF4 (output B) is the serial clock output.
		++UF4Counter &= 0xf; // Clock and clamp UF4.
								// The UD2 read shift register is clocked by serial clock (the rising edge of encoder/decoder's UF4 B output (serial clock))
								//	- ie on counts 2, 6, 10 and 14 (2 is the only count that outputs a 1 into readShiftRegister as the MSB bits of the count NORed together for other values are 0)
		if ((UF4Counter & 0x3) == 2)
		{
			readShiftRegister <<= 1;
			readShiftRegister |= (UF4Counter == 2); 

			//writeShiftRegister <<= 1;
			
			bool resetTime = ((readShiftRegister & 0x3ff) == 0x3ff);
			m_pVIA->GetPortB()->SetInput(0x80, !resetTime);
			if (resetTime)	// if the last 10 bits are 1s then SYNC
				UE3Counter = 0;	// Phase lock on to byte boundary
			else
				UE3Counter++;
		}
		// UC5B (NOR used to invert UF4's output B serial clock) output high when UF4 counts 0,1,4,5,8,9,12 and 13
		else if (((UF4Counter & 2) == 0) && (UE3Counter == 8))	// Phase locked on to byte boundary
		{
			UE3Counter = 0;
			SO = (m_pVIA->GetFCR() & m6522::FCR_CA2_OUTPUT_MODE0) != 0;	// bit 2 of the FCR indicates "Byte Ready Active" turned on or not.
//			writeShiftRegister = readShiftRegister;
			m_pVIA->GetPortA()->SetInput(readShiftRegister & 0xff);
		}
	};
}

void Drive::DriveLoopReadNoFlux()
{
	unsigned int minCycles;
	unsigned int cycles = 16;

	fluxReversalCyclesLeft -= 16;

	while (true)
	{
		minCycles = min(min(cyclesLeftForBit, cycles), UE7Counter);
		cyclesLeftForBit -= minCycles;
		cycles -= minCycles;
		UE7Counter -= minCycles;

		if (cycles == 0)
			return;

		if (cyclesLeftForBit == 0)
		{
			cyclesForBitErrorCounter -= cyclesPerBitErrorConstant;
			cyclesLeftForBit = cyclesPerBitInt + (cyclesForBitErrorCounter < cyclesPerBitErrorConstant);

			//cyclesForBit -= cyclesPerBit;
			//cyclesLeftForBit = ceil(cyclesPerBit - cyclesForBit);
			//cyclesForBit += cyclesLeftForBit;

			if (GetNextBit())
			{
				ResetEncoderDecoder(18 * 16, /*20 * 16*/ 2 * 16);
			}
			if (cycles < UE7Counter)
			{
				UE7Counter -= cycles;
				cyclesLeftForBit -= cycles;
				return;
			}
			cyclesLeftForBit -= UE7Counter;
			cycles -= UE7Counter;
		}

		//if (UE7Counter == 0x0) // The count carry (bit 4) clocks UF4.
		{
			UE7Counter = 16 - CLOCK_SEL_AB;	// A and B inputs of UE7 come from the VIA's CLOCK SEL A/B outputs (ie PB5/6) ie preload the encoder/decoder clock for the current density settings.
										// The decoder consists of UF4 and UE5A. The ecoder has two outputs, Pin 1 of UE5A is the serial data output and pin 2 of UF4 (output B) is the serial clock output.
			++UF4Counter &= 0xf; // Clock and clamp UF4.
								 // The UD2 read shift register is clocked by serial clock (the rising edge of encoder/decoder's UF4 B output (serial clock))
								 //	- ie on counts 2, 6, 10 and 14 (2 is the only count that outputs a 1 into readShiftRegister as the MSB bits of the count NORed together for other values are 0)
			if ((UF4Counter & 0x3) == 2)
			{
				readShiftRegister <<= 1;
				readShiftRegister |= (UF4Counter == 2);

				//writeShiftRegister <<= 1;

				bool resetTime = ((readShiftRegister & 0x3ff) == 0x3ff);
				m_pVIA->GetPortB()->SetInput(0x80, !resetTime);
				if (resetTime)	// if the last 10 bits are 1s then SYNC
					UE3Counter = 0;	// Phase lock on to byte boundary
				else
					UE3Counter++;
			}
			// UC5B (NOR used to invert UF4's output B serial clock) output high when UF4 counts 0,1,4,5,8,9,12 and 13
			else if (((UF4Counter & 2) == 0) && (UE3Counter == 8))	// Phase locked on to byte boundary
			{
				UE3Counter = 0;
				SO = (m_pVIA->GetFCR() & m6522::FCR_CA2_OUTPUT_MODE0) != 0;	// bit 2 of the FCR indicates "Byte Ready Active" turned on or not.
	//			writeShiftRegister = readShiftRegister;
				m_pVIA->GetPortA()->SetInput(readShiftRegister & 0xff);
			}
		}
	};
}
void Drive::DriveLoopReadNoCycles()
{
	unsigned int minCycles;
	unsigned int cycles = 16;

	cyclesLeftForBit -= 16;
	while (true)
	{
		minCycles = min(cycles, min(UE7Counter, fluxReversalCyclesLeft));
		fluxReversalCyclesLeft -= minCycles;
		cycles -= minCycles;
		UE7Counter -= minCycles;

		if (cycles == 0)
			return;

		if (fluxReversalCyclesLeft == 0)//Not entirely right, a flux reversal will be skipped if a bit read was going to happen
		{
			ResetEncoderDecoder(2 * 16, /*25 * 16*/23 * 16); // Trigger a random noise generated zero crossing and start seeing more anywhere between 2us and 25us after this one.
		}

		if (UE7Counter == 0x0) // The count carry (bit 4) clocks UF4.
		{
			UE7Counter = 16 - CLOCK_SEL_AB;	// A and B inputs of UE7 come from the VIA's CLOCK SEL A/B outputs (ie PB5/6) ie preload the encoder/decoder clock for the current density settings.
										// The decoder consists of UF4 and UE5A. The ecoder has two outputs, Pin 1 of UE5A is the serial data output and pin 2 of UF4 (output B) is the serial clock output.
			++UF4Counter &= 0xf; // Clock and clamp UF4.
								 // The UD2 read shift register is clocked by serial clock (the rising edge of encoder/decoder's UF4 B output (serial clock))
								 //	- ie on counts 2, 6, 10 and 14 (2 is the only count that outputs a 1 into readShiftRegister as the MSB bits of the count NORed together for other values are 0)
			if ((UF4Counter & 0x3) == 2)
			{
				readShiftRegister <<= 1;
				readShiftRegister |= (UF4Counter == 2);

				//writeShiftRegister <<= 1;

				bool resetTime = ((readShiftRegister & 0x3ff) == 0x3ff);
				m_pVIA->GetPortB()->SetInput(0x80, !resetTime);
				if (resetTime)	// if the last 10 bits are 1s then SYNC
					UE3Counter = 0;	// Phase lock on to byte boundary
				else
					UE3Counter++;
			}
			// UC5B (NOR used to invert UF4's output B serial clock) output high when UF4 counts 0,1,4,5,8,9,12 and 13
			else if (((UF4Counter & 2) == 0) && (UE3Counter == 8))	// Phase locked on to byte boundary
			{
				UE3Counter = 0;
				SO = (m_pVIA->GetFCR() & m6522::FCR_CA2_OUTPUT_MODE0) != 0;	// bit 2 of the FCR indicates "Byte Ready Active" turned on or not.
	//			writeShiftRegister = readShiftRegister;
				m_pVIA->GetPortA()->SetInput(readShiftRegister & 0xff);
			}
		}
	};
}

void Drive::DriveLoopRead()
{
	unsigned int minCycles;
	unsigned int cycles = 16;

	while (true)
	{
		minCycles = min(min(cyclesLeftForBit, cycles), min(UE7Counter, fluxReversalCyclesLeft));
		cyclesLeftForBit -= minCycles;
		fluxReversalCyclesLeft -= minCycles;
		cycles -= minCycles;
		UE7Counter -= minCycles;

		if (cycles == 0)
			return;

		if (cyclesLeftForBit == 0)
		{
			cyclesForBitErrorCounter -= cyclesPerBitErrorConstant;
			cyclesLeftForBit = cyclesPerBitInt + (cyclesForBitErrorCounter < cyclesPerBitErrorConstant);

			if (GetNextBit())
			{
				ResetEncoderDecoder(18 * 16, /*20 * 16*/ 2 * 16);
			}
		}

		if (fluxReversalCyclesLeft == 0)//Not entirely right, a flux reversal will be skipped if a bit read was going to happen
		{
			ResetEncoderDecoder(2 * 16, /*25 * 16*/23 * 16); // Trigger a random noise generated zero crossing and start seeing more anywhere between 2us and 25us after this one.
		}

		if (UE7Counter == 0x0) // The count carry (bit 4) clocks UF4.
		{
			UE7Counter = 16-CLOCK_SEL_AB;	// A and B inputs of UE7 come from the VIA's CLOCK SEL A/B outputs (ie PB5/6) ie preload the encoder/decoder clock for the current density settings.
										// The decoder consists of UF4 and UE5A. The ecoder has two outputs, Pin 1 of UE5A is the serial data output and pin 2 of UF4 (output B) is the serial clock output.
			++UF4Counter &= 0xf; // Clock and clamp UF4.
								 // The UD2 read shift register is clocked by serial clock (the rising edge of encoder/decoder's UF4 B output (serial clock))
								 //	- ie on counts 2, 6, 10 and 14 (2 is the only count that outputs a 1 into readShiftRegister as the MSB bits of the count NORed together for other values are 0)
			if ((UF4Counter & 0x3) == 2)
			{
				readShiftRegister <<= 1;
				readShiftRegister |= (UF4Counter == 2);

				//writeShiftRegister <<= 1;

				bool resetTime = ((readShiftRegister & 0x3ff) == 0x3ff);
				m_pVIA->GetPortB()->SetInput(0x80, !resetTime);
				if (resetTime)	// if the last 10 bits are 1s then SYNC
					UE3Counter = 0;	// Phase lock on to byte boundary
				else
					UE3Counter++;
			}
			// UC5B (NOR used to invert UF4's output B serial clock) output high when UF4 counts 0,1,4,5,8,9,12 and 13
			else if (((UF4Counter & 2) == 0) && (UE3Counter == 8))	// Phase locked on to byte boundary
			{
				UE3Counter = 0;
				SO = (m_pVIA->GetFCR() & m6522::FCR_CA2_OUTPUT_MODE0) != 0;	// bit 2 of the FCR indicates "Byte Ready Active" turned on or not.
	//			writeShiftRegister = readShiftRegister;
				m_pVIA->GetPortA()->SetInput(readShiftRegister & 0xff);
			}
		}
	};
}

void Drive::DriveLoopWrite()
{
	unsigned int cycles = 16;
	while(true)
	{
		if (cycles < UE7Counter)
		{
			UE7Counter -= cycles;
			cycles = 0;
			return;
		}
		
		cycles -= UE7Counter;

		UE7Counter = 16 - CLOCK_SEL_AB;	// A and B inputs of UE7 come from the VIA's CLOCK SEL A/B outputs (ie PB5/6) ie preload the encoder/decoder clock for the current density settings.
									// The decoder consists of UF4 and UE5A. The ecoder has two outputs, Pin 1 of UE5A is the serial data output and pin 2 of UF4 (output B) is the serial clock output.
		++UF4Counter &= 0xf; // Clock and clamp UF4.
								// The UD2 read shift register is clocked by serial clock (the rising edge of encoder/decoder's UF4 B output (serial clock))
								//	- ie on counts 2, 6, 10 and 14 (2 is the only count that outputs a 1 into readShiftRegister as the MSB bits of the count NORed together for other values are 0)
		if ((UF4Counter & 0x3) == 2)
		{
			readShiftRegister <<= 1;
			readShiftRegister |= (UF4Counter == 2); // Emulate UE5A and only shift in a 1 when pins 6 (output C) and 7 (output D) (bits 2 and 3 of UF4Counter are 0. ie the first count of the bit cell)

			SetNextBit((writeShiftRegister & 0x80));

			writeShiftRegister <<= 1;
			// Note: SYNC can only trigger during reading as R/!W line is one of UC2's inputs.
			UE3Counter++;
		}
		// UC5B (NOR used to invert UF4's output B serial clock) output high when UF4 counts 0,1,4,5,8,9,12 and 13
		else if (((UF4Counter & 2) == 0) && (UE3Counter == 8))	// Phase locked on to byte boundary
		{
			UE3Counter = 0;
			SO = (m_pVIA->GetFCR() & m6522::FCR_CA2_OUTPUT_MODE0) != 0;	// bit 2 of the FCR indicates "Byte Ready Active" turned on or not.
			writeShiftRegister = m_pVIA->GetPortA()->GetOutput();
		}
	}
}
#endif