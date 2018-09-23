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

#ifndef WD177X_H
#define WD177X_H

#include "IOPort.h"
#include "DiskImage.h"
#include "m6502.h"

//32x	4e
// For 10 sectors
//		12x	00	// SYNC
//		3x	a1
//		1x	fe	// ID
//		1x	TrackNo (0 indexed)
//		1x	Head
//		1x	sector (1 indexed)
//		1x	02	(sector size)
//		1x	crc high byte
//		1x	crc low byte
//		22x	4e	(gap2)
//		For 2 loops (logical sectors to physical sectors)
//			if first loop
//				12x	00	// SYNC
//				3x	a1
//				1x	fb	// Data mark
//			endif
//			256x	data (indexed sequentailly through D81)
//		end for
//		1x	crc high byte
//		1x	crc low byte
//		35x	4e	(gap3)
// end for

//PHYSICAL:
//	Cylinders 0 thru  79
//	Sectors  1 thru  10 on  Side  1
//	Sectors  1 thru  10  on  Side  2
//	Sector Size  512
//LOGICAL :
//	Tracks  1 thru  80
//	Sectors 0 thru  39 (Using  physical  Sectors  1 ... 10 - Side  1 and 2)
//	Sector Size  256  Bytes


// This emulator emulates at a command level and does not go into emulating actual micro controller instructions.

#define CYCLES_8Mhz_PER_INDEX_HOLE 64

class WD177x
{

	enum Registers
	{
		STATUS = 0,
		COMMAND = 0,
		TRACK,
		SECTOR,
		DATA
	};

	enum StatusBits
	{
		// This bit is 1 when the 177x is busy. This bit is 0 when the 177x is free for CPU commands.
		BUSY = 0x01,					// 0

		// For Type I commands, this bit is high during the index pulse that occurs once per disk rotation low otherwise.
		// For Type II and III commands, high signals the CPU to handle the data register in order to maintain a continuous flow of data.
		// High when the data register is full during a read or when the data register is empty during a write.
		INDEX_DATAREQUEST = 0x02,		// 1

		// For Type I commands, this bit is 0 if the mechanism is at track zero and 1 if the head is not.
		// For Type II or III commands, this bit is 1 if the CPU did not respond to Data Request (INDEX_DATAREQUEST) in time for the 177x to maintain a continuous data flow.
		TRACKZERO_LOSTDATA = 0x04,		// 2

		CRC_ERROR = 0x08,				// 3

		// This bit is set if the 177x cannot find the track, sector, or side.
		SEEK_ERROR = 0x10,		// 4

		// For Type I commands, this bit is low during the 6 - revolution motor spin - up time.
		// For Type II and Type III commands, low indicates a normal data mark, high indicates a deleted data mark.
		SPINUP_DATAMARK = 0x20,		// 5

		WRITE_PROTECT = 0x40,			// 6
		MOTOR_ON = 0x80				// 7
	};

	// Command      Bit 7     B6     B5     B4     B3     B2     B1     Bit 0
	// --------     -----     --     --     --     --     --     --     -----
	// Restore      0         0      0      0      h      V      r1     r0
	// Seek         0         0      0      1      h      V      r1     r0
	// Step         0         0      1      u      h      V      r1     r0
	// Step in      0         1      0      u      h      V      r1     r0
	// Step out     0         1      1      u      h      V      r1     r0

	enum Commands
	{
		RESTORE = 0,
		SEEK = 1,
		STEP = 2,
		STEP_IN = 4,
		STEP_OUT = 6,

		READ_SECTOR = 8,
		WRITE_SECTOR = 0xa,

		READ_ADDRESS = 0xc,

		FORCE_INTERRUPT = 0xd,

		READ_TRACK = 0xe,
		WRITE_TRACK = 0xf,
	};

	enum CommandBit
	{
		COMMANDBIT_A = 0x01,
		COMMANDBIT_H = 0x08,
		COMMANDBIT_U = 0x10,
		COMMANDBIT_E = 0x04,
		COMMANDBIT_M = 0x10
	};

	enum CommandSubState
	{
		SEARCHING_FOR_NEXT_ID,
		WAIT_FOR_TRACK,
		WAIT_FOR_SIDE,
		WAIT_FOR_SECTOR,
		WAIT_FOR_SECTOR_LENGTH,
		SEARCHING_FOR_NEXT_DATAID = WAIT_FOR_SECTOR_LENGTH,
		WAIT_FOR_CRC1,
		WAIT_FOR_CRC2,
		READING_SECTOR,
		WRITING_SECTOR
	};

public:

	WD177x();

	inline void ConnectIRQ(Interrupt* irq) { this->irq = irq; }

	void Insert(DiskImage* diskImage);

	void Reset();

	void Execute();

	unsigned char Read(unsigned int address);
	unsigned char Peek(unsigned int address);
	void Write(unsigned int address, unsigned char value);

	inline bool GetDIRECTIONPin() const { return stepDirection > 0; }
	//bool GetSTEPPin() const { return STEPPulse; }
	inline bool GetTR00Pin() const { return !(currentTrack == 0); }	// active low
	// IP is the index pulse
	inline bool GetIPPin() const { return !(rotationCycle < CYCLES_8Mhz_PER_INDEX_HOLE); }	// active low
	//bool GetDRQPin() const { return ; } // This active high output indicates that the Data Register is full(on a Read) or empty(on a Write operation).
	bool GetWPRTPin() const; // active low
	void SetWPRTPin(bool value); // active low

	inline unsigned int GetCurrentTrack() const { return currentTrack; }

//	void SetSide(int side) { currentSide = (side ^ 1); }
	void SetSide(int side) { currentSide = side; }
	void AssertExternalMotor(bool assert);
	inline bool IsExternalMotorAsserted() const { return externalMotorAsserted; }

private:
	bool SpinUp();
	void SpinDown();

	void UpdateCommandType1();
	void UpdateCommandType2();
	void UpdateCommandType3();
	void UpdateCommandType4();
	void CommandComplete();

	bool CheckForSeekError()
	{
		// SEEK_ERROR after 5 disk revolutions without finding track/sector
		return rotationCountForSeekError > 5;
	}

	//void UpdateReadWriteByte();
	bool ReadByte(unsigned char& byte, bool& syncByte);

	void AssertIRQ();
	void ReleaseIRQ();

	void SetDataRegister(unsigned char byteRead);
	//inline void SetDataRegister(unsigned char byteRead)
	//{
	//	if (statusRegister & INDEX_DATAREQUEST)		// If the CPU has not read the last value yet
	//		statusRegister |= TRACKZERO_LOSTDATA;	// then it missed the bus.
	//	statusRegister |= INDEX_DATAREQUEST;
	//	dataRegister = byteRead;
	//}
	unsigned char GetDataRegister();

	unsigned char trackRegister;
	unsigned char sectorRegister;
	unsigned char statusRegister;
	unsigned char dataRegister;
	unsigned char currentByteRead;
	unsigned char sectorFromHeader;

	unsigned char commandRegister;
	unsigned char commandRegisterPrevious;
	unsigned char commandType;

	unsigned int motorSpinCount;

	int currentTrack;
	int currentSide;
	bool externalMotorAsserted;
	bool writeProtectAsserted;

	int stepDirection;

	unsigned char command;
	unsigned char commandValue;
	unsigned int commandStage;

	unsigned int headDataOffset;
	bool lastByteWasASync;

	DiskImage* diskImage;

	unsigned int rotationCountForSeekError;
	unsigned int rotationCycle;
	unsigned int byteRotationCycle;
	unsigned int inactiveRotationCount;
	unsigned int settleCycleDelay;

	unsigned int readAddressState;
	unsigned int sectorByteIndex;

	Interrupt* irq;

	int delayTimer;

	unsigned short crc;
	unsigned char dataAddressMark;
};

#endif