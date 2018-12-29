// burst
//	- graph SRQ
//		- option to display it
// test
//	- read track (how?)
//	- write protect
// clean up
//	- cpu accesses inside the cpps
// snoop
// format
// clear caddy if different image selected




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

#include "wd177x.h"
#include "debug.h"

#include "Pi1581.h"

// Clocks
//	Master 16Mhz
//		into 74ls93
//			QB - 16Mhz / 2 = 8MHz (177x)
//			QD - 16Mhz / 8 = 2MHz (6502)

// Disk spins at 300rpm = 5rps so to calculate how many 8Mhz cycles one rotation takes;-
// 8000000 / 5 = 1600000;

// With 6122 bytes per head track
// 1600000 / 6122 = 261.3525 cycles per byte
// V is 64 so scale is 4.078
// 

static const unsigned int CYCLES_8Mhz_PER_ROTATION = 1600000;
static const unsigned int CYCLES_8Mhz_PER_BYTE = 261;

#define Mhz 8

WD177x::WD177x()
{
	diskImage = 0;
	Reset();
}

void WD177x::Reset()
{
	inactiveRotationCount = 0;

	externalMotorAsserted = false;
	writeProtectAsserted = false;

	//trackRegister;
	//sectorRegister;
	statusRegister = 0;
	//dataRegister;
	commandRegisterPrevious = 0;
	commandRegister = 0;

	motorSpinCount = 0;

	currentTrack = 0;
	stepDirection = 1;

	commandStage = 0;

	headDataOffset = 0;

	byteRotationCycle = 0;

	delayTimer = 0;
}

bool WD177x::GetWPRTPin() const	// active low
{
	// This input is sampled whenever a Write Command is received A logic low on this line will prevent any Write Command from executing(internal pull up)
	bool writeProtect = true;

	if (diskImage)
		writeProtect = diskImage->GetReadOnly();

	return !writeProtect; // active low
}

void WD177x::SetWPRTPin(bool value) // active low
{
//	writeProtectAsserted = (value == false);
}

void WD177x::AssertIRQ()
{
	if (irq) irq->Assert();
}

void WD177x::ReleaseIRQ()
{
	if (irq) irq->Release();
}

void WD177x::UpdateCommandType1()
{
	//DEBUG_LOG("UCT1 %d\r\n", commandStage);

	// For Type I commands, this bit is high during the index pulse that occurs once per disk rotation low otherwise.
	if (!GetIPPin()) // active low
		statusRegister |= INDEX_DATAREQUEST;
	else
		statusRegister &= ~INDEX_DATAREQUEST;

	// For Type I commands, this bit is 0 if the mechanism is at track zero and 1 if the head is not.
	if (!GetTR00Pin()) // active low
		statusRegister &= ~TRACKZERO_LOSTDATA;
	else
		statusRegister |= TRACKZERO_LOSTDATA;

	switch (commandStage)
	{
		case 0:
			// slight usec delay before BUSY
			delayTimer = 5 * Mhz;
			commandStage++;
		break;
		case 1:
			//DEBUG_LOG("%d %02x\r\n", delayTimer, statusRegister);
			delayTimer--;
			if (delayTimer < 0)
				commandStage++;
			break;
		case 2:	// Wait for disk to spin up
			statusRegister = BUSY;
			//DEBUG_LOG("BUSY = %02x\r\n", statusRegister);

			if (commandValue & COMMANDBIT_H)
			{
				// In a 1581 the 8520 drives the motor
				//if (motorSpinCount)
				//	motorSpinCount--;	// Need to wait 6 revoiutions before executing the command.

				//if (motorSpinCount == 0)
				//{
					statusRegister |= SPINUP_DATAMARK;
					commandStage++;
				//}
			}
			else
			{
				commandStage++;
			}

			// r (Step Time) - This bit pair determines the time between track steps according to the following table:
			// r1       r0            1770                 1772
			// --       --            ----                 ----
			// 0        0             6ms                  6ms
			// 0        1             12ms	               12ms
			// 1        0             20ms                 2ms
			// 1        1             30ms                 3ms
			switch (commandValue & 3)
			{
				case 0:
					delayTimer = 6000 * Mhz;
				break;
				case 1:
					delayTimer = 12000 * Mhz;
				break;
				case 2:
					delayTimer = 2000 * Mhz;
				break;
				case 3:
					delayTimer = 3000 * Mhz;
				break;
			}
		break;
		case 3:
			//DEBUG_LOG("%d %02x\r\n", delayTimer, statusRegister);
			delayTimer--;
			if (delayTimer < 0)
				commandStage++;
		break;
		case 4:
			switch (command)
			{
				case RESTORE:
					// For now teleport
					trackRegister = 0;
					currentTrack = 0;

					CommandComplete();
				break;
				case SEEK:
					// For now teleport
					currentTrack = dataRegister;
					trackRegister = dataRegister;

					CommandComplete();
				break;
				case STEP:
				case STEP + 1:
				case STEP_IN:
				case STEP_IN + 1:
				case STEP_OUT:
				case STEP_OUT + 1:
					currentTrack += stepDirection;
					if (currentTrack < 0)
						currentTrack = 0;
					else if (currentTrack > 79)
						currentTrack = 79;

					if (commandValue & COMMANDBIT_U)
					{
						trackRegister = currentTrack;
					}
					CommandComplete();
				break;
				default:
					CommandComplete();
				break;
			}
		break;
		//CommandComplete();
	}
}

unsigned char sb[512 * 50];
int sbo = 0;

void WD177x::UpdateCommandType2()
{
	switch (command)
	{
		case READ_SECTOR:
			switch (commandStage)
			{
				case 0:
					// slight usec delay before BUSY
					// THERE IS NO DOCUMETED FIGURE IN THE DATASHEET - just guessing (as the 1581 ROM is polling the BUSY to go high and needs this delay)
					delayTimer = 5 * Mhz;
					commandStage++;
				break;
				case 1:
					//DEBUG_LOG("%d %02x\r\n", delayTimer, statusRegister);
					delayTimer--;
					if (delayTimer < 0)
						commandStage++;
					break;
				case 2:	// Wait for disk to spin up
					statusRegister = BUSY;
					lastByteWasASync = false;

					if (settleCycleDelay)
					{
						settleCycleDelay--;
					}
					else
					{
						rotationCountForSeekError = 0;
						commandStage++;
					}
				break;
				case 3:
				{
					// When an ID field is encountered that has the correct track number, correct sector number, and correct CRC, the data field is presented to the computer.
					// The Data Address Mark of the data field is found with 30 bytes in single density and 43 bytes in double density of the last ID field CRC byte.
					// If not, the ID field is searched for and verified again followed by the Data Address Mark search. 
					// If, alter five revolutions the DAM is not found, the Record Not Found Status Bit is set and the operation is terminated.
					// When the first character or byte of the data field is shifted through the DSR, it is transferred to the DR, and DRQ is generated.
					// When the next byte is accumulated in the DSR, it is transferred to the DR and another DRO is generated.
					// If the computer has not read the previous contents of the DR before a new character is transferred that character is lost and the Lost Data Status Bit is set.
					//UpdateReadWriteByte();

					unsigned char byteRead;
					bool syncByte;
					if (ReadByte(byteRead, syncByte))
					{
						switch (readAddressState)
						{
							case SEARCHING_FOR_NEXT_ID:

								if (CheckForSeekError())
								{
									statusRegister |= SEEK_ERROR;
									CommandComplete();
								}
								else
								{
									// byte after the last sync needs to be 0xfe
									if (!syncByte)
									{
										if (lastByteWasASync && byteRead == 0xfe)
										{
											readAddressState = WAIT_FOR_TRACK;
										}
										lastByteWasASync = false;
									}
									else
									{
										lastByteWasASync = true;
									}
								}
							break;
							case WAIT_FOR_TRACK:
								if (byteRead == trackRegister)
									readAddressState = WAIT_FOR_SIDE;
								else
									readAddressState = SEARCHING_FOR_NEXT_ID;
							break;
							case WAIT_FOR_SIDE:
								readAddressState = WAIT_FOR_SECTOR;
							break;
							case WAIT_FOR_SECTOR:
								if (byteRead == sectorRegister)
									readAddressState = SEARCHING_FOR_NEXT_DATAID;
								else
									readAddressState = SEARCHING_FOR_NEXT_ID;
							break;
							case SEARCHING_FOR_NEXT_DATAID:
								if (!syncByte)
								{
									if (lastByteWasASync && byteRead == 0xfb)
									{
										sectorByteIndex = 0;
										readAddressState = READING_SECTOR;
									}
									lastByteWasASync = false;
								}
								else
								{
									lastByteWasASync = true;
								}
							break;
							case READING_SECTOR:
								SetDataRegister(byteRead);
								sectorByteIndex++;
								if (sectorByteIndex == D81_SECTOR_LENGTH)
								{
									if (commandValue & COMMANDBIT_M)
									{
										if (sectorRegister == 10)
										{
											CommandComplete();
										}
										else
										{
											sectorRegister++;
											readAddressState = SEARCHING_FOR_NEXT_ID;
										}
									}
									else
									{
										CommandComplete();
									}
								}
							break;
						}
					}
				}
				break;
			}
		break;

		case WRITE_SECTOR:
			switch (commandStage)
			{
				case 0:
					// slight usec delay before BUSY
					// THERE IS NO DOCUMETED FIGURE IN THE DATASHEET - just guessing (as the 1581 ROM is polling the BUSY to go high and needs this delay)
					delayTimer = 5 * Mhz;
					commandStage++;
				break;
				case 1:
					//DEBUG_LOG("%d %02x\r\n", delayTimer, statusRegister);
					delayTimer--;
					if (delayTimer < 0)
						commandStage++;
					break;
				case 2:	// Wait for disk to spin up
					statusRegister = BUSY;
					lastByteWasASync = false;

					if (settleCycleDelay)
					{
						settleCycleDelay--;
					}
					else
					{
						if (writeProtectAsserted)
						{
							DEBUG_LOG("WP\r\n");
							statusRegister |= WRITE_PROTECT;
							CommandComplete();
						}
						else
						{
							rotationCountForSeekError = 0;
							commandStage++;
						}
					}
				break;
				case 3:
				{
					unsigned char byteRead;
					bool syncByte;
					if (ReadByte(byteRead, syncByte))
					{
						switch (readAddressState)
						{
							case SEARCHING_FOR_NEXT_ID:
								if (CheckForSeekError())
								{
									statusRegister |= SEEK_ERROR;
									CommandComplete();
								}
								else
								{
									// byte after the last sync needs to be 0xfe
									if (!syncByte)
									{
										if (lastByteWasASync && byteRead == 0xfe)
										{
											readAddressState = WAIT_FOR_TRACK;
										}
										lastByteWasASync = false;
									}
									else
									{
										lastByteWasASync = true;
									}
								}
							break;
							case WAIT_FOR_TRACK:
								if (byteRead == trackRegister)
									readAddressState = WAIT_FOR_SIDE;
								else
									readAddressState = SEARCHING_FOR_NEXT_ID;
							break;
							case WAIT_FOR_SIDE:
								readAddressState = WAIT_FOR_SECTOR;
							break;
							case WAIT_FOR_SECTOR:
								if (byteRead == sectorRegister)
									readAddressState = SEARCHING_FOR_NEXT_DATAID;
								else
									readAddressState = SEARCHING_FOR_NEXT_ID;
							break;
							case SEARCHING_FOR_NEXT_DATAID:
								if (!syncByte)
								{
									if (lastByteWasASync)
									{
										if (commandValue & COMMANDBIT_A)
											dataAddressMark = 0xf8;
										else
											dataAddressMark = 0xfb;
										
										if (diskImage)
											diskImage->SetD81Byte(currentTrack, currentSide, headDataOffset, dataAddressMark);

										GetDataRegister();

										sectorByteIndex = 0;
										readAddressState = WRITING_SECTOR;
									}
									lastByteWasASync = false;
								}
								else
								{
									lastByteWasASync = true;
								}
							break;
							case WRITING_SECTOR:
							{
								unsigned char data = GetDataRegister();
								if (sectorByteIndex == 0)
								{
									crc = 0xffff;
									DiskImage::CRC(crc, 0xa1);
									DiskImage::CRC(crc, 0xa1);
									DiskImage::CRC(crc, 0xa1);
									DiskImage::CRC(crc, dataAddressMark);
								}
								DiskImage::CRC(crc, data);

								sb[sbo++] = data;

								//DEBUG_LOG("%d %02x\r\n", sectorByteIndex, data);
								if (diskImage)
									diskImage->SetD81Byte(currentTrack, currentSide, headDataOffset, data);

								sectorByteIndex++;
								if (sectorByteIndex == D81_SECTOR_LENGTH)
								{
									readAddressState = WAIT_FOR_CRC1;
								}
							}
							break;
							case WAIT_FOR_CRC1:
								if (diskImage)
									diskImage->SetD81Byte(currentTrack, currentSide, headDataOffset, (unsigned char)(crc >> 8));
								readAddressState = WAIT_FOR_CRC2;
							break;
							case WAIT_FOR_CRC2:
								if (diskImage)
									diskImage->SetD81Byte(currentTrack, currentSide, headDataOffset, (unsigned char)(crc & 0xff));

								if (commandValue & COMMANDBIT_M)
								{
									if (sectorRegister == 10)
									{
										CommandComplete();
									}
									else
									{
										sectorRegister++;
										readAddressState = SEARCHING_FOR_NEXT_ID;
									}
								}
								else
								{
									CommandComplete();

									for (int i = 0; i < sbo; ++i)
									{
										DEBUG_LOG("%d %02x\r\n", i, sb[i]);
									}
									sbo = 0;
								}
							break;
						}
					}
				}
				break;
			}
		break;
		default:
			CommandComplete();
		break;
	}

}

void WD177x::UpdateCommandType3()
{
	switch (command)
	{
		case READ_ADDRESS:
		{
			switch (commandStage)
			{
				case 0:
					// slight usec delay before BUSY
					// THERE IS NO DOCUMETED FIGURE IN THE DATASHEET - just guessing (as the 1581 ROM is polling the BUSY to go high and needs this delay)
					delayTimer = 5 * Mhz;
					commandStage++;
				break;
				case 1:
					//DEBUG_LOG("%d %02x\r\n", delayTimer, statusRegister);
					delayTimer--;
					if (delayTimer < 0)
						commandStage++;
					break;
				case 2:	// Wait for disk to spin up
					statusRegister = BUSY;
					lastByteWasASync = false;

					if (settleCycleDelay)
					{
						settleCycleDelay--;
					}
					else
					{
						readAddressState = SEARCHING_FOR_NEXT_ID;
						commandStage++;
					}
				break;
				case 3:
				{
					unsigned char byteRead;

					bool syncByte;
					if (ReadByte(byteRead, syncByte))
					{
						switch (readAddressState)
						{
							case SEARCHING_FOR_NEXT_ID:
								// byte after the last sync needs to be 0xfe
								if (!syncByte)
								{
									if (lastByteWasASync && byteRead == 0xfe)
									{
										//DEBUG_LOG("RA found ID\r\n");
										readAddressState = WAIT_FOR_TRACK;
									}
									lastByteWasASync = false;
								}
								else
								{
									lastByteWasASync = true;
								}
							break;
							case WAIT_FOR_TRACK:
								SetDataRegister(byteRead);
								readAddressState = WAIT_FOR_SIDE;
								break;
							case WAIT_FOR_SIDE:
								SetDataRegister(byteRead);
								readAddressState = WAIT_FOR_SECTOR;
								break;
							case WAIT_FOR_SECTOR:
								sectorFromHeader = byteRead;
								SetDataRegister(byteRead);
								readAddressState = WAIT_FOR_SECTOR_LENGTH;
								break;
							case WAIT_FOR_SECTOR_LENGTH:
								SetDataRegister(byteRead);
								readAddressState = WAIT_FOR_CRC1;
								break;
							case WAIT_FOR_CRC1:
								SetDataRegister(byteRead);
								readAddressState = WAIT_FOR_CRC2;
							break;
							case WAIT_FOR_CRC2:
								//DEBUG_LOG("%02x\r\n", byteRead);
								SetDataRegister(byteRead);
								sectorRegister = sectorFromHeader;
								CommandComplete();
							break;
						}
					}
				}
			}
		}
		break;

		case READ_TRACK:
			switch (commandStage)
			{
				case 0:
					// slight usec delay before BUSY
					// THERE IS NO DOCUMETED FIGURE IN THE DATASHEET - just guessing (as the 1581 ROM is polling the BUSY to go high and needs this delay)
					delayTimer = 5 * Mhz;
					commandStage++;
				break;
				case 1:
					//DEBUG_LOG("%d %02x\r\n", delayTimer, statusRegister);
					delayTimer--;
					if (delayTimer < 0)
						commandStage++;
					break;
				case 2:	// Wait for disk to spin up
					statusRegister = BUSY;
					lastByteWasASync = false;

					if (settleCycleDelay)
					{
						settleCycleDelay--;
					}
					else
					{
						if (diskImage)
							commandStage++;
						else
							CommandComplete();
					}
				break;
				case 3:	// Wait for index hole
					if (rotationCycle == 0)
					{
						sectorByteIndex = 0;
						headDataOffset = 0;
						byteRotationCycle = CYCLES_8Mhz_PER_BYTE;
						commandStage++;
					}
				break;
				case 4:
				{
					unsigned char byteRead;
					bool syncByte;
					if (ReadByte(byteRead, syncByte))
					{
						SetDataRegister(byteRead);
						sectorByteIndex++;

						if (sectorByteIndex == diskImage->TrackLength(currentTrack))
							CommandComplete();
					}
				}
			}
		break;

		case WRITE_TRACK:
			switch (commandStage)
			{
				case 0:
					// slight usec delay before BUSY
					// THERE IS NO DOCUMETED FIGURE IN THE DATASHEET - just guessing (as the 1581 ROM is polling the BUSY to go high and needs this delay)
					delayTimer = 5 * Mhz;
					commandStage++;
				break;
				case 1:
					//DEBUG_LOG("%d %02x\r\n", delayTimer, statusRegister);
					delayTimer--;
					if (delayTimer < 0)
						commandStage++;
					break;
				case 2:	// Wait for disk to spin up
					statusRegister = BUSY;
					lastByteWasASync = false;

					if (settleCycleDelay)
					{
						settleCycleDelay--;
					}
					else
					{
						if (writeProtectAsserted)
						{
							statusRegister |= WRITE_PROTECT;
							CommandComplete();
						}
						else
						{
							rotationCountForSeekError = 0;
							commandStage++;
						}
					}
				break;
				case 3:
				{
					// Writing starts with the leading edge of the first encountered Index Pulse and continues until the next index Pulse, at which time the interrupt is activated.
					// The Data Request is activated immediately upon receiving the command, but writing does not start until after the first byte is loaded into the Data Register.
					// If the DR is not loaded within three byte times, the operation is terminated making the device Not Busy, the Lost Data Status Bit is set, and the interrupt is activated.

					// If a byte is not present in the DR when needed, a byte of zeroes is substituted.

					// This sequence continues from one index Pulse to the next.
					// Normally whatever data pattern appears in the Data Register is written on the disk with a normal clock pattern.
					// However, if the WD177X-00 detects a data pattern of F5 through FE in the Data Register, this is interpreted as Data Address Marks with missing ciocks or CRC generation.

					// The CRC generator is initialized when any data byte from F8 to FE is transferred from the DR to the DSR in FM or by receipt of F5 in MFM.
					// An F7 pattern generates two CRC characters in FM or MFM.
					// As a consequence, the patterns F5 through FE do not appear in the gaps, data field, or ID ttetds.
					// Also, CRC’s are generated by an F7 pattern.

					// Disks are formatted in IBM 3 ? 40 or System 34 formats with sector lengths of 128. 256, 512, or 1024 bytes.

					CommandComplete();
				}
			}
		break;
		default:
			CommandComplete();
		break;
	}
}

void WD177x::UpdateCommandType4()
{
	CommandComplete();
}

void WD177x::CommandComplete()
{
	// Add a small usec delay before we drop the BUSY flag.
	// This gives the CPU time to query the dataRegister before we signal the command complete.
	// THERE IS NO DOCUMETED FIGURE IN THE DATASHEET - just guessing.
	// When the 1581 ROM issues a FORCE_INTERRUPT it will waste time calling AD2F a few times
	delayTimer = 30 * Mhz;

	commandType = 0;
	//DEBUG_LOG("CC %02x\r\n", statusRegister);
}

bool WD177x::ReadByte(unsigned char& byte, bool& syncByte)
{
	bool byteRead = false;

	byteRotationCycle++;

	if (byteRotationCycle >= CYCLES_8Mhz_PER_BYTE)
	{
		byteRotationCycle = 0;

		if (diskImage)
		{
			unsigned int trackLength = diskImage->TrackLength((unsigned int)currentTrack);

			headDataOffset++;
			if (headDataOffset > trackLength)
			{
				headDataOffset = 0;
				rotationCountForSeekError++;
			}

			currentByteRead = diskImage->GetD81Byte(currentTrack, currentSide, headDataOffset);
			syncByte = diskImage->IsD81ByteASync(currentTrack, currentSide, headDataOffset);

			byte = currentByteRead;
			byteRead = true;
		}
		//else
		//{
		//	DEBUG_LOG("NO disk!\r\n");
		//}
	}

	return byteRead;
}

//void WD177x::UpdateReadWriteByte()
//{
//	byteRotationCycle++;
//
//	if (byteRotationCycle >= CYCLES_8Mhz_PER_BYTE)
//	{
//		byteRotationCycle = 0;
//
//		if (diskImage)
//		{
//			unsigned int trackLength = diskImage->TrackLength((unsigned int)currentTrack);
//
//			headDataOffset++;
//			if (headDataOffset > trackLength)
//				headDataOffset = 0;
//
//			// Watch/trace V fdd.c line 557
//			unsigned headIndex = 0;
//			currentByteRead = diskImage->GetD81Byte(currentTrack, headIndex, headDataOffset);
//
//			// if reading conditions have been met eg correct track number, correct sector number, and correct CRC this can be transfered into the dataRegister
//			//dataRegister = currentByteRead;
//
//			// INDEX_DATAREQUEST
//			// For Type II and III commands, high signals the CPU to handle the data register in order to maintain a continuous flow of data.
//			// High when the data register is full during a read or when the data register is empty during a write.
//
//			// When a byte is ready
//			//sectorRegister |= INDEX_DATAREQUEST;
//		}
//	}
//}

// Update for a single cycle
void WD177x::Execute()
{
	// The Direction signal is active high when stepping in and low when stepping out.
	// A 4 uSec(MFM) or 8 uSec(FM) pulse is provided as	an output to the drive.
	// The Direction signal is valid 24 uSec before the first stepping pulse is	generated.
	// Alter the last directional step an additional 30 msec of head settling time takes place if the Verify flag is set in Type I Commands.
	// There is also a 30 msec head settling time if the E flag is set In any Type II or III Command.

	// When a Seek, Step or Restore Command is executed, an optional verification of Read / Write head position can be performed by setting bit 2 (V = 1) in the command word to a logic 1.
	// The verification operation begins at the end of the 30 msec settling time.
	// The track number from the first encountered ID Field is compared against the contents of the Track Register.
	// If the track numbers compare and the ID Field CRC is correct, the verify operation is complete and an INTRQ is generated with no errors.
	// If there is a match but not a valid CRC, the CRC error status bit is set (Status	Bit 3), and the next encountered ID Field is read from the disk for the verification operation.

	// The WD177x finds an ID Field with correct track number and correct CRC within 5 revolutions of the media, or the seek error is set and an INTRQ is generated. If V = 0, no verification is performed.


	// Rotation
	// 1600000 / 6122 = 261.3525 cycles per byte
	// When the first character or byte of the data field is shifted through the DSR, it is transferred to the DR, and DRQ is generated.When the next byte is accumulated in the DSR, it is transferred to the DR and another DRO is generated.
	// If the computer has not read the previous contents of the DR before a new character is transferred that character is lost and the Lost Data Status Bit is set.
	//
	// At the end of the Read operation, the type of Data Address Mark encountered in the data field is recorded in the Status Register(Bit 5) as shown :
	// 1 Deleted data mark
	// 0 Data mark

	rotationCycle++;
	if (rotationCycle == CYCLES_8Mhz_PER_ROTATION)
	{
		rotationCycle = 0;
		if (commandType == 0)
		{
			// In a 1581 the 8520 drives the motor
			//// If the 177x is idle for 9 consecutive disk revolutions, it turns off the drive motor. In a 1581 the CIA will do this.
			//inactiveRotationCount++;
			//if (inactiveRotationCount > 9)
			//{
			//	SpinDown();
			//}
		}
		if (commandRegisterPrevious & 4) // I2
		{
			AssertIRQ();
		}
	}

	switch (commandType)
	{
		case 0:
			if (delayTimer)
			{
				delayTimer--;
				if (delayTimer == 0)
					statusRegister &= ~BUSY;
			}
		break;
		case 1:
			UpdateCommandType1();
		break;
		case 2:
			UpdateCommandType2();
		break;
		case 3:
			UpdateCommandType3();
		break;
		case 4:
			UpdateCommandType4();
		break;
	}
}

unsigned char WD177x::Read(unsigned int address)
{
	unsigned char value = 0;

	//The address bits A1 and A0, combined with the signals R/W, are interpreted as selecting the following registers:
	//A1 A0   Read             Write
	// 0  0 Status Register   Command Register
	// 0  1 Track Register    Track Register
	// 1  0 Sector Register   Sector Register
	// 1  1 Data Register     Data Register

	// Alter any register is written to, the same register can not be read from until 16 usec in MFM or 32 usec in FM have elapsed.

	switch (address & 3)
	{
		case STATUS:
			// a CPU read of the Status Register clears the 177x's interrupt output.
			value = statusRegister;

			ReleaseIRQ();
		break;
		case TRACK:
			value = trackRegister;
		break;
		case SECTOR:
			value = sectorRegister;
		break;
		case DATA:
			value = dataRegister;
			// When the Data Register is read the DRQ bit in the Status Register and the DRQ line are automatically reset.
			statusRegister &= ~INDEX_DATAREQUEST;
		break;
	}

	return value;
}

unsigned char WD177x::Peek(unsigned int address)
{
	unsigned char value = 0;

	switch (address & 3)
	{
		case COMMAND:
			value = statusRegister;
		break;
		case TRACK:
			value = trackRegister;
		break;
		case SECTOR:
			value = sectorRegister;
		break;
		case DATA:
			value = dataRegister;
		break;
	}

	return value;
}

void WD177x::Write(unsigned int address, unsigned char value)
{
	//The address bits A1 and A0, combined with the signals R/W, are interpreted as selecting the following registers:
	//A1 A0   Read             Write
	// 0  0 Status Register   Command Register
	// 0  1 Track Register    Track Register
	// 1  0 Sector Register   Sector Register
	// 1  1 Data Register     Data Register

	// Alter any register is written to, the same register can not be read from until 16 usec in MFM or 32 usec in FM have elapsed.

	inactiveRotationCount = 0;

	switch (address & 3)
	{
		case COMMAND:
		{
			// INTRQ is reset by either reading the Status Register or by loading the Command Register with a new command.
			ReleaseIRQ();

			command = value >> 4;
			commandValue = value;
			// When the 177x is busy, it ignores CPU writes to this register UNLESS the new command is a force interrupt.

			// If the 177x receives a Force Interrupt command while it is executing another command, the FDDC will clear the
			// Busy bit and leave all other Status bits unchanged.
			// If the 177x receives a Force Interrupt command while it is not executing a command, the 177x will update the
			// entire Status Register as though it had just executed a Type I command.

			if ((statusRegister & BUSY) && (command != FORCE_INTERRUPT))
			{
				DEBUG_LOG("Command rejected BUSY\n");
			}
			else
			{
				// Type I Commands
				// Command      Bit 7     B6     B5     B4     B3     B2     B1     Bit 0
				// --------     -----     --     --     --     --     --     --     -----
				// Restore      0         0      0      0      h      V      r1     r0
				// Seek         0         0      0      1      h      V      r1     r0
				// Step         0         0      1      u      h      V      r1     r0
				// Step in      0         1      0      u      h      V      r1     r0
				// Step out     0         1      1      u      h      V      r1     r0

				// u (Update Track Register) - If this flag is set, the 177x will update the track register after executing the command.
				// h (Motor On) - If the value of this bit is 1, the controller will disable the motor spinup sequence. 
				//				 If the h Flag is not set and the MO signal is low when a command is received, ir forces MO to a logic 1 and waits 6 revoiutions before executing the command.
				//				Otherwise, if the motor is off, the chip will turn the motor on and wait 6 revolutions before executing the command.
				//				At 300 RPM, the 6 - revolution wait guarantees a one - second start time. If the 177x is idle for 9 consecutive disk revolutions, it turns off the drive motor.
				//				If the 177x receives a command while the motor is on, the controller executes the command immediately.

				// r (Step Time) - This bit pair determines the time between track steps according to the following table:
				// r1       r0            1770                 1772
				// --       --            ----                 ----
				// 0        0             6ms                  6ms
				// 0        1             12ms	               12ms
				// 1        0             20ms                 2ms
				// 1        1             30ms                 3ms


				// Type II Commands
				// Command          Bit 7     B6     B5     B4     B3     B2     B1     Bit 0
				// ------------     -----     --     --     --     --     --     --     -----
				// Read Sector      1         0      0      m      h      E      0      0
				// Write Sector     1         0      1      m      h      E      P      a0

				// m (Multiple Sectors) - If this bit = 0, the 177x reads or writes ("accesses") only one sector.  
				//						If this bit = 1, the 177x sequentially accesses sectors up to and including the last sector on the track.
				//						A multiple-sector command will end prematurely when the CPU loads a Force Interrupt command into the Command Register.
				// E(Settling Delay) - If this flag is set, the head settles before command execution.
				//						The settling time is 15ms for the 1772 and 30ms for the 1770.
				// P (Write Precompensation) - On the 1770-02 and 1772-00, a 0 value in this bit enables automatic write precompensation.
				//							The FDDC delays or advances the write bit stream by one-eighth of a cycle according to the following table.
				// Previous          Current bit           Next bit
				// bits sent         sending               to be sent       Precompensation
				// ---------         -----------           ----------       ---------------
				// x       1         1                     0                Early
				// x       0         1                     1                Late
				// 0       0         0                     1                Early
				// 1       0         0                     0                Late
				//
				//							Programmers typically enable precompensation on the innermost tracks, where bit shifts usually occur and bit density is maximal.
				//							A 1 value for this flag disables write precompensation.
				// a0(Data Address Mark) - If this bit is 0, the 177x will write a normal data mark (0xfb).
				//							If this bit is 1, the 177x will write a deleted	data mark (0xf8).

				// Type III commands are Read Address, Read Track, and Write Track.
				// Command          Bit 7     B6     B5     B4     B3     B2     B1     Bit 0
				// ------------     -----     --     --     --     --     --     --     -----
				// Read Address     1         1      0      0      h      E      0      0
				// Read Track       1         1      1      0      h      E      0      0
				// Write Track      1         1      1      1      h      E      P      0

				// Read Address:
				// The 177x reads the next ID field it finds, then sends the CPU the following six bytes via the Data Register:
				// Byte #     Meaning                |     Sector length code     Sector
				// length
				// ------     ------------------     |     -------------------------------
				// 1          Track                  |     0                      128
				// 2          Side                   |     1                      256
				// 3          Sector                 |     2                      512
				// 4          Sector length code     |     3                      1024
				// 5          CRC byte 1             |
				// 6          CRC byte 2             |

				bool spinningUp = false;

				commandStage = 0;
				commandRegisterPrevious = commandRegister;
				commandRegister = value;

				//DEBUG_LOG("Command %02x\r\n", value);

				// In a 1581 the 8520 drives the motor
				//if ((command != FORCE_INTERRUPT) && ((value & 8) == 0))
				//	spinningUp = SpinUp();

				statusRegister = 0;

				switch (command)
				{
					//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					// Type I Commands
					//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

					case RESTORE:	// seek to track 0
						// Upon receipt of this command, the Track 00 input is sampled. If TR00 is active low indicating the Read/write head is positioned over	track 0,
						// the Track Register is Loaded with zeroes and	an interrupt is generated.
						// If TR00 is not active low, stepping pulses at a rate specified by the r1 r0 field are issued until the TR00 input is activated.

						// When at track 0, the Track Register is loaded with zeroes and an interrupt is generated.
						// If the TR00 input does not go active low alter 255 stepping pulses, the WD177x terminates operation, interrupts, and sets the Seek Error status bit, providing the V flag is set.

						//if (GetTR00Pin() == false) 	// active low
						//{
						//}
						//else
						//{
						//	// Now teleport
						//	currentTrack = 0;
						//}

						//DEBUG_LOG("RESTORE\r\n");

						commandType = 1;
					break;
					case SEEK:
						// This command assumes that the Track Register contains the track number of the current position of the Read / Write head and the Data Register contains the desired track number.

						// Issue stepping pulses in the	appropriate direction until the contents of the Track Register are equal to the contents of the Data Register.

						//  An interrupt is generated at the completion of the command.

						// DEBUG_LOG("SEEK\r\n");

						commandType = 1;
					break;

					case STEP:
					case STEP + 1:
						// The WD177x issues one Stepping Pulse to the disk drive.
						// The stepping	motor direction is the same as in the previous step	command.
						// After a delay determined by the r1, r0 field.
						// If the U flag is on, the Track Register is updated.

						//  An interrupt is generated at the completion of the command.

						//DEBUG_LOG("STEP\r\n");

						commandType = 1;
					break;
					case STEP_IN:
					case STEP_IN + 1:
						// The WD177x issues one Stepping Pulse In the direction towards track 80.
						// if the U flag is on, the Track Register is incremented by one.
						// After a delay determined by the r1, r0 field

						//  An interrupt is generated at the completion of the command.
						stepDirection = 1;

						//DEBUG_LOG("STEP IN\r\n");

						commandType = 1;
					break;
					case STEP_OUT:
					case STEP_OUT + 1:
						// The WD177x issues one Stepping Pulse In the direction towards track 0.
						// if the U flag is on, the Track Register is incremented by one.
						// After a delay determined by the r1, r0 field

						//  An interrupt is generated at the completion of the command.
						stepDirection = -1;

						commandType = 1;
					break;

					//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					// Type II Commands
					//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

					case READ_SECTOR:
						// The controller waits for a sector ID field that has the correct track number, sector number, and CRC.
						// The controller then checks for the Data Address Mark, which consists of 43 copies of the second byte of the CRC.
						// If the controller does not find a sector with correct ID	field and address mark within 5 disk revolutions, the command ends.
						// Once the 177x finds the desired sector, it loads the bytes of that sector into the data register.
						// If there is a CRC error at the end of the data field, the 177x sets the CRC Error bit in the Status Register and ends the command regardless of the state of the "m" flag.

						if (value & COMMANDBIT_E)
							settleCycleDelay = 15000 * Mhz;	// The settling time is 15ms for the 1772 and 30ms for the 1770.
						else
							settleCycleDelay = 0;

						//DEBUG_LOG("READ_SECTOR\r\n");
						readAddressState = SEARCHING_FOR_NEXT_ID;

						commandType = 2;
					break;

					case WRITE_SECTOR:
						// The 177x waits for a sector ID field with the correct track number, sector number, and CRC.The 177x then counts off 22 bytes from the CRC field.
						// If the CPU has not loaded a byte into the Data Register before the end of this 22 - byte delay, the 177x ends the command.
						// Assuming that the CPU has heeded the 177x's data request, the controller writes 12 bytes of zeroes.
						// The 177x then writes a normal or deleted Data Address Mark according to the a0 flag of the command.
						// Next, the 177x writes the byte which the CPU placed in the Data Register, and continues to request and write data bytes until the end of the sector.
						// After the 177x writes the last byte, it calculates and writes the 16 - bit CRC.The chip then writes one $ff byte.
						// The 177x interrupts the CPU 24 cycles after it writes the second byte of the CRC.
						statusRegister = INDEX_DATAREQUEST;

						if (value & COMMANDBIT_E)
							settleCycleDelay = 15000 * Mhz;	// The settling time is 15ms for the 1772 and 30ms for the 1770.
						else
							settleCycleDelay = 0;


						//DEBUG_LOG("WRITE_SECTOR\r\n");
						readAddressState = SEARCHING_FOR_NEXT_ID;

						commandType = 2;
					break;

					//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					// Type III Commands
					//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					case READ_ADDRESS:
						settleCycleDelay = 0;

						readAddressState = SEARCHING_FOR_NEXT_ID;

						commandType = 3;
					break;

					case READ_TRACK:
						// This command dumps a raw track, including gaps, ID fields, and data, into the Data Register.

						DEBUG_LOG("READ_TRACK\r\n");

						commandType = 3;
					break;

					case WRITE_TRACK:
						// This command is the means of formatting disks. 

						//During the format command the character loaded into the data register of the WD1772 is written to the disk.
						//However the characters $F5 and $F6 are used to write respectively the Sync Characters $A1 and $C2 with a missing clock transition and the character $F7 is used to generate two CRC bytes.
						//This implies that it is not possible to create a sector with an ID ranging from 245 through 247 ($F5 - $F7).
						//In fact the WD1772 documentation indicates that the sector number should be kept in the range 1 to 240.

						// An $F7 input byte results in the FDC writing two CRC bytes with no $F7 in them at all.
						// $FC - Index Address Mark (clock pattern D7)
						// $FE - ID Address Mark (clock pattern C7)
						// $FB - Data Address Mark (clock pattern C7)
						// $F8 - Deleted Data Address Mark (clock pattern C7)
						DEBUG_LOG("WRITE_TRACK\r\n");

						commandType = 3;
					break;

					//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					// Type IV Commands
					//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
					case FORCE_INTERRUPT:
						// This command is loaded into the Command Register at any time.
						// If there is a current command under execution(Busy Status Bit set) the command is terminated and the Busy Status Bit reset (the rest of the status bits are unchanged)

						if (statusRegister & BUSY)
						{
							// If the Force Interrupt Command is received when there is not a current command under execution, the Busy Status Bit is reset and the rest
							// of the status bits are updated or cleared. In this case, Status reflects the Type I commands.
						}

						CommandComplete();

						DEBUG_LOG("FORCE_INTERRUPT\r\n");


						// Don't understand this? Shouldn't it be opposite (D8 enables the IRQ and D0 clears it)?
						// Reading the status or writing to the Command Register does not automatically clear the IRQ.
						// The Hex D0 is the only command that enables the immediate IRQ (D8) to clear on a subsequent load Command Register or Read Status Register operation.
						// Follow a Hex D8 with D0 command.
						if (value & 8)
						{
							// I3
							AssertIRQ();
						}
						else
						{
							// I assume D0 releases the IRQ
							ReleaseIRQ();
						}

						commandType = 4;
					break;
				}
			}
		}
		break;
		case TRACK:
			// During disk reading, writing, and verifying, the 177x compares the Track Register to the track number in the sector ID field.
			// When the 177x is busy, it ignores CPU writes to this register. (!Check)
			trackRegister = value;
		break;
		case SECTOR:
			// When the 177x is busy, it ignores CPU writes to this register.
			sectorRegister = value;
		break;
		case DATA:
			dataRegister = value;
			// A write to the Data Register also causes both DRQ's to reset. 
			statusRegister &= ~INDEX_DATAREQUEST;
		break;
	}
}

bool WD177x::SpinUp()
{
	if ((statusRegister & MOTOR_ON) == 0)
	{
		statusRegister |= MOTOR_ON;

		//DEBUG_LOG("Motor on\r\n");
		// Need to wait 6 revoiutions before executing the command.
		motorSpinCount = CYCLES_8Mhz_PER_ROTATION * 6;
		//statusRegister &= ~SPINUP_DATAMARK;
		// Just spin it for now.
		statusRegister |= SPINUP_DATAMARK;
		return true;
	}
	return false;
}

void WD177x::SpinDown()
{
	//DEBUG_LOG("Motor off\r\n");
	statusRegister &= ~MOTOR_ON;
	statusRegister &= ~SPINUP_DATAMARK;
}

void WD177x::AssertExternalMotor(bool assert)
{
	externalMotorAsserted = assert;

	if (externalMotorAsserted)
		SpinUp();
	else
		SpinDown();
}

void WD177x::SetDataRegister(unsigned char byteRead)
{
	if (statusRegister & INDEX_DATAREQUEST)		// If the CPU has not read the last value yet
		statusRegister |= TRACKZERO_LOSTDATA;	// then it missed the bus.
	statusRegister |= INDEX_DATAREQUEST;
	dataRegister = byteRead;
	//DEBUG_LOG("%02x\r\n", byteRead);
}

unsigned char WD177x::GetDataRegister()
{
	if (statusRegister & INDEX_DATAREQUEST)		// If the CPU has not written the last value yet
		statusRegister |= TRACKZERO_LOSTDATA;	// then it missed the bus.
	statusRegister |= INDEX_DATAREQUEST;
	return dataRegister;
}

void WD177x::Insert(DiskImage* diskImage)
{
	this->diskImage = diskImage;
	writeProtectAsserted = diskImage->GetReadOnly();
}
