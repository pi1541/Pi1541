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


//TODO when we have different file types we then need to detect these type of wildcards;-
//*=S selects only sequential files
//*=P	selects program files
//*=R	selects relative files
//*=U	selects user-files

#include "iec_commands.h"
#include "iec_bus.h"
#include "ff.h"
#include "DiskImage.h"
#include "Petscii.h"
#include "FileBrowser.h"
#include "DiskImage.h"
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <algorithm>

#define CBM_NAME_LENGTH 16
#define CBM_NAME_LENGTH_MINUS_D64 CBM_NAME_LENGTH-4

#define DIRECTORY_ENTRY_SIZE 32

#define EOI_RECVD       (1<<0)
#define COMMAND_RECVD   (1<<1)

extern unsigned versionMajor;
extern unsigned versionMinor;

extern void Reboot_Pi();

extern void SwitchDrive(const char* drive);
extern int numberOfUSBMassStorageDevices;
extern void DisplayMessage(int x, int y, bool LCD, const char* message, u32 textColour, u32 backgroundColour);

#define WaitWhile(checkStatus) \
	do\
	{\
		IEC_Bus::ReadBrowseMode();\
		if (CheckATN()) return true;\
	} while (checkStatus)

#define VERSION_OFFSET_IN_DIR_HEADER 17
static u8 DirectoryHeader[] =
{
	1, 4,	// BASIC start address
	1, 1,	// next line pointer
	0, 0,	// line number 0
	0x12,	// Reverse on
	0x22,	// Quote
	'P', 'I', '1', '5', '4', '1', ' ', 'V', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', // Name
	0x22,	// Quote
	0x20,	// Space
	'P', 'I', ' ', '2', 'A',	// ID, Dos Disk 2A
	00
};

static const u8 DirectoryBlocksFree[] = {
	1, 1,	// Next line pointer
	0, 0,	// 16bit free blocks value
	'B', 'L', 'O', 'C', 'K', 'S', ' ', 'F', 'R', 'E', 'E', '.',
	0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00
};

static const u8 filetypes[] = {
	'D', 'E', 'L', // 0
	'S', 'E', 'Q', // 1
	'P', 'R', 'G', // 2
	'U', 'S', 'R', // 3
	'R', 'E', 'L', // 4
	'C', 'B', 'M', // 5
	'D', 'I', 'R', // 6
};

static char ErrorMessage[64];

static u8* InsertNumber(u8* msg, u8 value)
{
	if (value >= 100)
	{
		*msg++ = '0' + value / 100;
		value %= 100;
	}
	*msg++ = '0' + value / 10;
	*msg++ = '0' + value % 10;
	return msg;
}

static void SetHeaderVersion()
{
	u8* ptr = DirectoryHeader + VERSION_OFFSET_IN_DIR_HEADER;
	ptr = InsertNumber(ptr, versionMajor);
	*ptr++ = '.';
	ptr = InsertNumber(ptr, versionMinor);
}

#define ERROR_00_OK 0
//01,FILES SCRATCHED,XX,00
//20,READ ERROR,TT,SS		header not found
//21,READ ERROR,TT,SS		sync not found
//22,READ ERROR,TT,SS		header checksum fail
//23,READ ERROR,TT,SS		data block checksum fail
//24,READ ERROR,TT,SS
#define ERROR_25_WRITE_ERROR 25	//25,WRITE ERROR,TT,SS		verify error
//26,WRITE PROTECT ON,TT,SS
//27,READ ERROR,TT,SS		header checksum fail
//28,WRITE ERROR,TT,SS	sync not found after write
//29,DISK ID MISMATCH,TT,SS
#define ERROR_30_SYNTAX_ERROR 30	//30,SYNTAX ERROR,00,00		could not parse the command
#define ERROR_31_SYNTAX_ERROR 31	//31,SYNTAX ERROR,00,00		unknown command
#define ERROR_32_SYNTAX_ERROR 32	//32,SYNTAX ERROR,00,00		command too long (ie > 40 characters)
#define ERROR_33_SYNTAX_ERROR 33	//33,SYNTAX ERROR,00,00		Wildcard * and ? was used in an open or save command
#define ERROR_34_SYNTAX_ERROR 34	//34,SYNTAX ERROR,00,00		File name could not be found in the command
#define ERROR_39_FILE_NOT_FOUND 39	//39,FILE NOT FOUND,00,00	User program of type 'USR' was not found
//50,RECORD NOT PRESENT,00,00
//51,OVERFLOW IN RECORD,00,00
//52,FILE TOO LARGE,00,00
//60,WRITE FILE OPEN,00,00	An at tempt was made to OPEN a file that had not previously been CLOSEd after writing.
//61,FILE NOT OPEN,00,00	A file was accessed that had not been OPENed. 
#define ERROR_62_FILE_NOT_FOUND 62		//62,FILE NOT FOUND,00,00	An attempt was made to load a program or open does not exist
#define ERROR_63_FILE_EXISTS 63		//63,FILE EXISTS,00,00		Tried to rename a file to the same name of an existing file
//65,NO BLOCK,TT,SS
//66,ILLEGAL TRACK OR SECTOR,TT,SS	
//67,ILLEGAL TRACK OR SECTOR,TT,SS
//70,NO CHANNEL,00,00		An attempt was made to open more files than channels available
//71,DIR ERROR,TT,SS
//72,DISK FULL,00,00
#define ERROR_73_DOSVERSION 73		// 73,VERSION,00,00
#define ERROR_74_DRlVE_NOT_READY 74		//74,DRlVE NOT READY,00,00
//75,FORMAT SPEED ERROR,00,00

void Error(u8 errorCode, u8 track = 0, u8 sector = 0)
{
	char* msg = "UNKNOWN";
	switch (errorCode)
	{
		case ERROR_00_OK:
			msg = " OK";
		break;
		case ERROR_25_WRITE_ERROR:
			msg = "WRITE ERROR";
		break;
		case ERROR_73_DOSVERSION:
			sprintf(ErrorMessage, "%02d,PI1541 V%02d.%02d,%02d,%02d", errorCode,
						versionMajor, versionMinor, track, sector);
			return;
		break;
		case ERROR_30_SYNTAX_ERROR:
		case ERROR_31_SYNTAX_ERROR:
		case ERROR_32_SYNTAX_ERROR:
		case ERROR_33_SYNTAX_ERROR:
		case ERROR_34_SYNTAX_ERROR:
			msg = "SYNTAX ERROR";
		break;
		case ERROR_39_FILE_NOT_FOUND:
			msg = "FILE NOT FOUND";
		break;
		case ERROR_62_FILE_NOT_FOUND:
			msg = "FILE NOT FOUND";
		break;
		case ERROR_63_FILE_EXISTS:
			msg = "FILE EXISTS";
		break;
		default:
			DEBUG_LOG("EC=%d?\r\n", errorCode);
		break;
	}
	sprintf(ErrorMessage, "%02d,%s,%02d,%02d", errorCode, msg, track, sector);
}

static inline bool IsDirectory(FILINFO& filInfo)
{
	return (filInfo.fattrib & AM_DIR) == AM_DIR;
}

void IEC_Commands::Channel::Close()
{
	if (open)
	{
		if (writing)
		{
			u32 bytesWritten;
			if (f_write(&file, buffer, cursor, &bytesWritten) != FR_OK)
			{
			}
		}
		f_close(&file);
		open = false;
	}
	cursor = 0;
	bytesSent = 0;
}

IEC_Commands::IEC_Commands()
{
	deviceID = 8;
	usingVIC20 = false;
	autoBootFB128 = false;
	Reset();
	starFileName = 0;
	C128BootSectorName = 0;
	displayingDevices = false;
	lowercaseBrowseModeFilenames = false;
	newDiskType = DiskImage::D64;
}

void IEC_Commands::Reset(void)
{
	receivedCommand = false;
	receivedEOI = false;
	secondaryAddress = 0;
	selectedImageName[0] = 0;
	atnSequence = ATN_SEQUENCE_IDLE;
	deviceRole = DEVICE_ROLE_PASSIVE;
	commandCode = 0;
	Error(ERROR_00_OK);
	CloseAllChannels();
}

void IEC_Commands::CloseAllChannels()
{
	for (int i = 0; i < 15; ++i)
	{
		channels[i].Close();
	}
}

bool IEC_Commands::CheckATN(void)
{
	bool atnAsserted = IEC_Bus::IsAtnAsserted();
	if (atnSequence == ATN_SEQUENCE_RECEIVE_COMMAND_CODE)
	{
		// TO CHECK is this case needed? Just let it complete
		if (!atnAsserted) atnSequence = ATN_SEQUENCE_HANDLE_COMMAND_CODE;
		return !atnAsserted;
	}
	else
	{
		if (atnAsserted) atnSequence = ATN_SEQUENCE_ATN;
		return atnAsserted;
	}
}

// Paraphrasing Jim Butterfield;- https://www.atarimagazines.com/compute/issue38/073_1_HOW_THE_VIC_64_SERIAL_BUS_WORKS.php
// The talker is asserting the Clock line.
// The listener is asserting the Data line.
// There could be more than one listener, in which case all of the listeners are asserting the Data line.
// When the talker is ready it releases the Clock line.
// The listener must detect this and respond, but it doesn't have to do so immediately.
// When the listener is ready to listen, it releases the Data line.
// The Data line will go "unasserted" only when all listeners have released it - in other words, when all listeners are ready to accept data.

// What happens next is variable. Either the talker will assert the Clock line again in less than 200 microseconds - usually within 60 microseconds - or 
// it will do nothing. The listener should be watching, and if 200 microseconds pass without the Clock line going to true, it has a special task to perform : note EOI (End Or Identify).
// If the Ready for Data signal isn't acknowledged by the talker within 200 microseconds, the listener knows that the talker is trying to signal EOI ie, "this character will be the last one."

// So if the listener sees the 200 microsecond time - out, it must signal "OK, I noticed the EOI" back to the talker, I does this by asserting Data line for at least 60 microseconds, and then releasing it.
// The talker will then revert to transmitting the character in the usual way; within 60 microseconds it will assert Clock line, and transmission will continue.
// At this point, the Clock line is asserted whether or not we have gone through the EOI sequence; we're back to a common transmission sequence. 

// The talker has eight bits to send. They will go out without handshake; in other words, the listener had better be there to catch them, since the talker won't wait to hear from the listener.
// At this point, the talker controls both lines, Clock and Data. At the beginning of the sequence, it is asserting the Clock, while the Data line is released.
// The Data line will change soon, since we'll send the data over it.

// For each bit, we set the Data line true or false according to whether the bit is one or zero.
// As soon as that's set, the Clock line is released, signalling "data ready."
// The talker will typically have a bit in place and be signalling ready in 70 microseconds or less. Once the talker has signalled "data ready, "it will hold the two lines steady for at least 20 microseconds timing needs to be increased to 
// 60 microseconds if the Commodore 64 is listening, since the 64's video chip may interrupt the processor for 42 microseconds at a time, and without the extra wait the 64 might completely miss a bit.
// The listener plays a passive role here; it sends nothing, and just watches. 
// As soon as it sees the Clock line false, it grabs the bit from the Data line and puts it away.
// It then waits for the clock line to go true, in order to prepare for the next bit.
// When the talker figures the data has been held for a sufficient length of time, it pulls the Clock line true and releases the Data line to false.
// Then it starts to prepare the next bit.

// After the eighth bit has been sent, it's the listener's turn to acknowledge. At this moment, the Clock line is asserted and the Data line is released.
// The listener must acknowledge receiving the byte OK by asserting the Data line. The talker is now watching the Data line.
// If the listener doesn't pull the Data line true within one millisecond it will know that something's wrong and may alarm appropriately. 

// We're finished, and back where we started. The talker is holding the Clock line true, and the listener is holding the Data line true. We're ready for step 1; we may send another character - unless EOI has happened.
// If EOI was sent or received in this last transmission, both talker and listener "let go." 
// After a suitable pause, the Clock and Data lines are released to false and transmission stops. 

bool IEC_Commands::WriteIECSerialPort(u8 data, bool eoi)
{
	IEC_Bus::WaitMicroSeconds(50); //sidplay64-sd2iec needs this?

	// When the talker is ready it releases the Clock line.
	IEC_Bus::ReleaseClock();

	// Wait for all listeners to be ready. They singal this by releasing the Data line.
	WaitWhile(IEC_Bus::IsDataAsserted());

	if (eoi) // End Or Identify
	{
		WaitWhile(IEC_Bus::IsDataReleased());
		WaitWhile(IEC_Bus::IsDataAsserted());
	}

	IEC_Bus::AssertClock();
	IEC_Bus::WaitMicroSeconds(40);
	WaitWhile(IEC_Bus::IsDataAsserted());
	IEC_Bus::WaitMicroSeconds(21);

	// At this point, the talker controls both lines, Clock and Data. At the beginning of the sequence, it is asserting the Clock, while the Data line is released.
	for (u8 i = 0; i < 8; ++i)
	{
		IEC_Bus::WaitMicroSeconds(45);
		if (data & 1 << i) IEC_Bus::ReleaseData();
		else IEC_Bus::AssertData();
		IEC_Bus::WaitMicroSeconds(22);
		IEC_Bus::ReleaseClock();
		if (usingVIC20) IEC_Bus::WaitMicroSeconds(34);
		else IEC_Bus::WaitMicroSeconds(75);
		IEC_Bus::AssertClock();
		IEC_Bus::WaitMicroSeconds(22);
		IEC_Bus::ReleaseData();
		IEC_Bus::WaitMicroSeconds(14);
	}

	// After the eighth bit has been sent, it's the listener's turn to acknowledge. At this moment, the Clock line is asserted and the Data line is released.
	WaitWhile(IEC_Bus::IsDataReleased());
	return false;
}

bool IEC_Commands::ReadIECSerialPort(u8& byte)
{
	byte = 0;

	// When the talker is ready it releases the Clock line.
	WaitWhile(IEC_Bus::IsClockAsserted());

	// We release data first
	IEC_Bus::ReleaseData();
	WaitWhile(IEC_Bus::IsDataAsserted());

	timer.Start(200);
	do
	{
		IEC_Bus::ReadBrowseMode();
		if (CheckATN()) return true;
	}
	while (IEC_Bus::IsClockReleased() && !timer.Tick());

	if (timer.TimedOut())
	{
		IEC_Bus::AssertData();
		IEC_Bus::WaitMicroSeconds(73);
		IEC_Bus::ReleaseData();
		WaitWhile(IEC_Bus::IsClockReleased());
		receivedEOI = true;
	}

	for (u8 i = 0; i < 8; ++i)
	{
		WaitWhile(IEC_Bus::IsClockAsserted());
		byte = (byte >> 1) | (!!IEC_Bus::IsDataReleased() << 7);
		WaitWhile(IEC_Bus::IsClockReleased());
	}

	IEC_Bus::AssertData();
	return false;
}

void IEC_Commands::SimulateIECBegin(void)
{
	SetHeaderVersion();
	Reset();
	IEC_Bus::ReadBrowseMode();
}

// Paraphrasing Jim Butterfield
// The computer is the only device that will assert ATN.
// When it does so, all other devices drop what they are doing and become listeners.
// Bytes sent by the computer during an ATN period are commands "Talk," "Listen," "Untalk," and "Unlisten" telling a specific device that it will become (or cease to be) a talker or listener.
// The commands go to all devices, and all devices acknowledge them, but only the ones with the suitable device numbers will switch into talk and listen mode.
// These commands are sometimes followed by a secondary address, and after ATN is released, perhaps by a file name.

// ATN SEQUENCES
// When ATN is asserted, everybody stops what they are doing.
// The computer will quickly assert the Clock line (it's going to send soon).
// At the same time, the processor releases the Data line to false, but all other devices are getting ready to listen and will each assert the Data line.
// They had better do this within one millisecond, since the processor is watching and may sound an alarm("device not present") if it doesn't see this take place.
// Under normal circumstances, transmission now takes place.
// The computer is sending commands rather than data, but the characters are exchanged with exactly the same timing and handshakes.
// All devices receive the commands, but only the specified device acts upon it.

// TURNAROUND
// An unusual sequence takes place following ATN if the computer wishes the remote device to become a talker.
// This will usually take place only after a Talk command has been sent.
// Immediately after ATN is released, the selected device will be behaving like a listener.
// After all, it's been listening during the ATN cycle, and the computer has been a talker.
// At this instant, we have "wrong way" logic; the device is asserting the Data line, and the computer is asserting the Clock line.
// We must turn this around. The computer quickly realizes what's going on, and asserts the Data line, as well as releasing the Clock line.
// The device waits for this: when it sees the Clock line released, it releases the Data line (which stays asserted anyway since the computer is asserting it)
// and then asserts the Clock line. We're now in our starting position, with the talker (that's the device) asserting the Clock, and the listener (the computer) asserting the Data line true.
IEC_Commands::UpdateAction IEC_Commands::SimulateIECUpdate(void)
{
	if (IEC_Bus::IsReset())
	{
		// If the computer is resetting then just wait until it has come out of reset.
		do
		{
			//DEBUG_LOG("Reset during SimulateIECUpdate\r\n");
			IEC_Bus::ReadBrowseMode();
			IEC_Bus::WaitMicroSeconds(100);
		}
		while (IEC_Bus::IsReset());
		IEC_Bus::WaitMicroSeconds(20);
		return RESET;
	}

	updateAction = NONE;

	if (selectedImageName[0] != 0) updateAction = IMAGE_SELECTED;

	switch (atnSequence)
	{
		case ATN_SEQUENCE_IDLE:
			IEC_Bus::ReadBrowseMode();
			if (IEC_Bus::IsAtnAsserted()) atnSequence = ATN_SEQUENCE_ATN;
			else if (selectedImageName[0] != 0) updateAction = IMAGE_SELECTED;
		break;
		case ATN_SEQUENCE_ATN:
			// All devices must release the Clock line as the computer will be the one assering it.
			IEC_Bus::ReleaseClock();
			// Tell computer we are ready to listen by asserting the Data line.
			IEC_Bus::AssertData();

			deviceRole = DEVICE_ROLE_PASSIVE;
			atnSequence = ATN_SEQUENCE_RECEIVE_COMMAND_CODE;
			receivedEOI = false;

			// Wait until the computer is ready to talk
			// TODO: should set a timer here and if it times out (before the clock is released) go back to IDLE?
			while (IEC_Bus::IsClockReleased())
			{
				IEC_Bus::ReadBrowseMode();
			}
		break;
		case ATN_SEQUENCE_RECEIVE_COMMAND_CODE:
			ReadIECSerialPort(commandCode);
			// Command Code
			// 20 Listen + device address (0-1e)
			// 3f Unlisten
			// 40 Talk + device address (0-1e)
			// 5f Untalk
			// 60 Open Channel or Data + secondary address (0-f)
			// 70 Undefined
			// 80 Undefined
			// 90 Undefined
			// a0 Undefined
			// b0 Undefined
			// c0 Undefined
			// d0 Undefined
			// e0 Close + secondary address or channel (0-f)
			// f0 Open + secondary address or channel (0-f)

			// Notes: from various CBM-DOS books highlighting various DOS requirements.

			// Secondary addresses 0 and 1 are reserved by the DOS for saving and loading programs.

			// Secondary address 15 is designated as the command and error channel.
			// The command/error channel 15 may be opened while a file is open, but when channel 15 is closed, all other channels are closed as well.

			// OPEN lfn, 8, sa, "filename,filetype,mode"
			// lfn - logical file number
			//		 When the logical file number is between 1 and 127, a PRINT# statement sends a RETURN character to the file after each variable.
			//		 If the logical file number is greater than 127 (128 - 255), the PRINT# statement sends an additional linefeed after each RETURN.
			//       The lfn simply a way of assiging a deviceID and a secondary address/channel to a single ID the computer can reference. 

			// Should several files be open at once, they must all use different secondary addresses/channels, as only one file can use a channel.

			// If a file is opened with the secondary address of a previously opened file, the previous file is closed.

			// A maximum of 3 channels can be opened with the 1541 at a time.

			// When specifying the filename to be written to (in the OPEN command), you must be sure that the file name does not already exist.
			// If a file that already exists is to be to opened for writing, the file must first be deleted.

			//DEBUG_LOG("%0x\r\n", commandCode);

			if (commandCode == 0x20 + deviceID)	// Listen
			{
				secondaryAddress = commandCode & 0x0f;
				deviceRole = DEVICE_ROLE_LISTEN;
				if (IEC_Bus::IsAtnAsserted()) atnSequence = ATN_SEQUENCE_RECEIVE_COMMAND_CODE;
				else atnSequence = ATN_SEQUENCE_HANDLE_COMMAND_CODE;
			}
			else if (commandCode == 0x3f)	// Unlisten
			{
				if (deviceRole == DEVICE_ROLE_LISTEN) deviceRole = DEVICE_ROLE_PASSIVE;
				atnSequence = ATN_SEQUENCE_HANDLE_COMMAND_CODE;
			}
			else if (commandCode == 0x40 + deviceID) // Talk
			{
				secondaryAddress = commandCode & 0x0f;
				deviceRole = DEVICE_ROLE_TALK;
				if (IEC_Bus::IsAtnAsserted()) atnSequence = ATN_SEQUENCE_RECEIVE_COMMAND_CODE;
				else atnSequence = ATN_SEQUENCE_HANDLE_COMMAND_CODE;
			}
			else if (commandCode == 0x5f)	// Untalk
			{
				if (deviceRole == DEVICE_ROLE_TALK) deviceRole = DEVICE_ROLE_PASSIVE;
				atnSequence = ATN_SEQUENCE_HANDLE_COMMAND_CODE;
			}
			else if ((commandCode & 0x60) == 0x60)	// Set secondary addresses for 6*, e* and f* commands
			{
				secondaryAddress = commandCode & 0x0f;
				if ((commandCode & 0xf0) == 0xe0)	// Close
				{
					CloseFile(secondaryAddress);

					if (IEC_Bus::IsAtnAsserted()) atnSequence = ATN_SEQUENCE_RECEIVE_COMMAND_CODE;
					else atnSequence = ATN_SEQUENCE_HANDLE_COMMAND_CODE;
				}
				else	// Open
				{
					atnSequence = ATN_SEQUENCE_HANDLE_COMMAND_CODE;
				}
			}
			else
			{
				IEC_Bus::ReleaseClock();
				IEC_Bus::ReleaseData();
				IEC_Bus::WaitWhileAtnAsserted();
				atnSequence = ATN_SEQUENCE_COMPLETE;
			}
		break;
		case ATN_SEQUENCE_HANDLE_COMMAND_CODE:
			IEC_Bus::WaitWhileAtnAsserted();
			if (deviceRole == DEVICE_ROLE_LISTEN)
			{
				Listen();
			}
			else if (deviceRole == DEVICE_ROLE_TALK)
			{
				// Do the turn around and become the talker
				IEC_Bus::ReleaseData();
				IEC_Bus::AssertClock();
				Talk();
			}
			atnSequence = ATN_SEQUENCE_COMPLETE;
		break;
		case ATN_SEQUENCE_COMPLETE:
			IEC_Bus::ReleaseClock();
			IEC_Bus::ReleaseData();

			if (receivedCommand)
			{
				Channel& channelCommand = channels[15];

				//DEBUG_LOG("%s sa = %d\r\n", channelCommand.buffer, secondaryAddress);

				if (secondaryAddress == 0xf) //channel 0xf (15) is reserved as the command channel.
				{
					ProcessCommand();
				}
				else
				{
					// We have no time to do anything now. I don't know why!
					// The ATN sequence is complete and we go back to idle. Maybe anther ATN sequence starts immediately?
					// We should be able to open files now but doing so takes time and breaks communication.
					// So instead, we just cache what we need to know and open the file later (at the start of talking or listening).
					Channel& channel = channels[secondaryAddress];
					memcpy(channel.command, channelCommand.buffer, channelCommand.cursor);
				}

				// Command has been processed so reset it now.
				receivedCommand = false;
			}
			atnSequence = ATN_SEQUENCE_IDLE;
		break;
	}
	return updateAction;
}

static bool CopyFile(char* filenameNew, char* filenameOld, bool concatenate)
{
	FRESULT res;
	FIL fpIn;
	bool success = false;
	res = f_open(&fpIn, filenameOld, FA_READ);
	if (res == FR_OK)
	{
		FIL fpOut;
		u8 mode = FA_WRITE;
		if (!concatenate) mode |= FA_CREATE_ALWAYS;
		else mode |= FA_OPEN_APPEND;

		res = f_open(&fpOut, filenameNew, mode);
		if (res == FR_OK)
		{
			char buffer[1024];
			u32 bytes;

			success = true;
			do
			{
				f_read(&fpIn, buffer, sizeof(buffer), &bytes);
				if (bytes > 0)
				{
					// TODO: Should check for disk full.
					if (f_write(&fpOut, buffer, bytes, &bytes) != FR_OK)
					{
						success = false;
						break;
					}
				}
			} while (bytes != 0);

			f_close(&fpOut);
		}
		f_close(&fpIn);
	}
	return success;
}

static const char* ParseName(const char* text, char* name, bool convert, bool includeSpace = false)
{
	char* ptrOut = name;
	const char* ptr = text;
	*name = 0;

	if (isspace(*ptr & 0x7f) || *ptr == ',' || *ptr == '=' || *ptr == ':')
	{
		ptr++;
	}

	// TODO: Should not do this - should use command length to test for the end of a command (use indicies instead of pointers?)
	while (*ptr != '\0')
	{
		if (!isspace(*ptr & 0x7f))
			break;
		ptr++;
	}
	if (*ptr != 0)
	{
		while (*ptr != '\0')
		{
			if ((!includeSpace && isspace(*ptr & 0x7f)) || *ptr == ',' || *ptr == '=' || *ptr == ':')
				break;
			if (convert) *ptrOut++ = petscii2ascii(*ptr++);
			else *ptrOut++ = *ptr++;
		}
	}
	*ptrOut = 0;
	return ptr;
}

static const char* ParseNextName(const char* text, char* name, bool convert)
{
	char* ptrOut = name;
	const char* ptr;
	*name = 0;

	// TODO: looking for these is bad for binary parameters (binary parameter commands should not come through here)
	ptr = strchr(text, ':');
	if (ptr == 0) ptr = strchr(text, '=');
	if (ptr == 0) ptr = strchr(text, ',');

	if (ptr)
		return ParseName(ptr, name, convert);
	*ptrOut = 0;
	return ptr;
}

static bool ParseFilenames(const char* text, char* filenameA, char* filenameB)
{
	bool success = false;
	text = ParseNextName(text, filenameA, true);
	if (text)
	{
		ParseNextName(text, filenameB, true);
		if (filenameB[0] != 0) success = true;
		else Error(ERROR_34_SYNTAX_ERROR);	// File name could not be found in the command
	}
	else
	{
		Error(ERROR_31_SYNTAX_ERROR);	// could not parse the command
	}
	return success;
}

// If it is a disk image then we select it,
// else if it is a folder then we enter it,
// else it is some other random file we do nothing.
bool IEC_Commands::Enter(DIR& dir, FILINFO& filInfo)
{
	filInfoSelectedImage = filInfo;

	if (DiskImage::IsDiskImageExtention(filInfo.fname))
	{
		strcpy((char*)selectedImageName, filInfo.fname);
		return true;
	}
	else if (IsDirectory(filInfo))
	{
		if (f_chdir(filInfo.fname) == FR_OK) updateAction = DIR_PUSHED;
		else Error(ERROR_62_FILE_NOT_FOUND);
	}
	return false;
}

static int ParsePartition(char** buf)
{
	int part = 0;

	while ((isdigit(**buf & 0x7f)) || **buf == ' ' || **buf == '@')
	{
		if (isdigit(**buf & 0x7f))	part = part * 10 + (**buf - '0');
		(*buf)++;
	}
	return 0;
}

void IEC_Commands::CD(int partition, char* filename)
{
	char filenameEdited[256];

	if (filename[0] == '/' && filename[1] == '/')
		sprintf(filenameEdited, "\\\\1541\\%s", filename + 2);
	else
		strcpy(filenameEdited, filename);

	int len = strlen(filenameEdited);

	for (int i = 0; i < len; i++)
	{
		if (filenameEdited[i] == '/')
			filenameEdited[i] = '\\';
		filenameEdited[i] = petscii2ascii(filenameEdited[i]);
	}

	DEBUG_LOG("CD %s\r\n", filenameEdited);
	if (filenameEdited[0] == '_' && len == 1)
	{
		updateAction = POP_DIR;
	}
	else
	{
		if (displayingDevices)
		{
			if (strncmp(filename, "SD", 2) == 0)
			{
				SwitchDrive("SD:");
				displayingDevices = false;
				updateAction = DEVICE_SWITCHED;
			}
			else
			{
				for (int USBDriveIndex = 0; USBDriveIndex < numberOfUSBMassStorageDevices; ++USBDriveIndex)
				{
					char USBDriveId[16];
					sprintf(USBDriveId, "USB%02d:", USBDriveIndex + 1);

					if (strncmp(filename, USBDriveId, 5) == 0)
					{
						SwitchDrive(USBDriveId);
						displayingDevices = false;
						updateAction = DEVICE_SWITCHED;
					}
				}
			}
		}
		else
		{
			DIR dir;
			FILINFO filInfo;

			char path[256] = { 0 };
			char* pattern = strrchr(filenameEdited, '\\');

			if (pattern)
			{
				// Now we look for a folder
				int len = pattern - filenameEdited;
				strncpy(path, filenameEdited, len);

				pattern++;

				if ((f_stat(path, &filInfo) != FR_OK) || !IsDirectory(filInfo))
				{
					Error(ERROR_62_FILE_NOT_FOUND);
				}
				else
				{
					char cwd[1024];
					if (f_getcwd(cwd, 1024) == FR_OK)
					{
						f_chdir(path);

						char cwd2[1024];
						f_getcwd(cwd2, 1024);

						bool found = f_findfirst(&dir, &filInfo, ".", pattern) == FR_OK && filInfo.fname[0] != 0;

						//DEBUG_LOG("%s pattern = %s\r\n", filInfo.fname, pattern);

						if (found)
						{
							if (DiskImage::IsDiskImageExtention(filInfo.fname))
							{
								if (f_stat(filInfo.fname, &filInfoSelectedImage) == FR_OK)
								{
									strcpy((char*)selectedImageName, filInfo.fname);
								}
								else
								{
									f_chdir(cwd);
									Error(ERROR_62_FILE_NOT_FOUND);
								}
							}
							else
							{
								//DEBUG_LOG("attemting changing dir %s\r\n", filInfo.fname);
								if (f_chdir(filInfo.fname) != FR_OK)
								{
									Error(ERROR_62_FILE_NOT_FOUND);
									f_chdir(cwd);
								}
								else
								{
									updateAction = DIR_PUSHED;
								}
							}
						}
						else
						{
							Error(ERROR_62_FILE_NOT_FOUND);
							f_chdir(cwd);
						}

					}
					//if (f_getcwd(cwd, 1024) == FR_OK)
					//	DEBUG_LOG("CWD on exit = %s\r\n", cwd);
				}
			}
			else
			{
				bool found = FindFirst(dir, filenameEdited, filInfo);

				if (found)
				{
					if (DiskImage::IsDiskImageExtention(filInfo.fname))
					{
						if (f_stat(filInfo.fname, &filInfoSelectedImage) == FR_OK)
							strcpy((char*)selectedImageName, filInfo.fname);
						else
							Error(ERROR_62_FILE_NOT_FOUND);
					}
					else
					{
						//DEBUG_LOG("attemting changing dir %s\r\n", filInfo.fname);
						if (f_chdir(filInfo.fname) != FR_OK)
							Error(ERROR_62_FILE_NOT_FOUND);
						else
							updateAction = DIR_PUSHED;
					}
				}
				else
				{
					Error(ERROR_62_FILE_NOT_FOUND);
				}
			}
		}
	}
}

void IEC_Commands::MKDir(int partition, char* filename)
{
	char filenameEdited[256];

	if (filename[0] == '/' && filename[1] == '/')
		sprintf(filenameEdited, "\\\\1541\\%s", filename + 2);
	else
		strcpy(filenameEdited, filename);
	int len = strlen(filenameEdited);

	for (int i = 0; i < len; i++)
	{
		if (filenameEdited[i] == '/')
			filenameEdited[i] = '\\';

		filenameEdited[i] = petscii2ascii(filenameEdited[i]);
	}

	f_mkdir(filenameEdited);

	// Force the FileBrowser to refresh incase it just heppeded to be in the folder that they are looking at
	updateAction = REFRESH;
}

void IEC_Commands::RMDir(void)
{
	DIR dir;
	FILINFO filInfo;
	FRESULT res;
	char filename[256];
	Channel& channel = channels[15];

	const char* text = (char*)channel.buffer;

	text = ParseNextName(text, filename, true);
	if (filename[0])
	{
		res = f_findfirst(&dir, &filInfo, ".", (const TCHAR*)filename);
		if (res == FR_OK)
		{
			if (filInfo.fname[0] != 0 && IsDirectory(filInfo))
			{
				DEBUG_LOG("rmdir %s\r\n", filInfo.fname);
				f_unlink(filInfo.fname);
				updateAction = REFRESH;
			}
		}
		else
		{
			Error(ERROR_62_FILE_NOT_FOUND);
		}
	}
	else
	{
		Error(ERROR_34_SYNTAX_ERROR);
	}
}

void IEC_Commands::FolderCommand(void)
{
	Channel& channel = channels[15];

	switch (toupper(channel.buffer[0]))
	{
		case 'M':
		{
			char* in = (char*)channel.buffer;
			int part;

			part = ParsePartition(&in);
			if (part > 0)
			{
				// Only have one drive partition
				//Error(ERROR_74_DRlVE_NOT_READY);
				return;
			}
			in += 2;	// Skip command
			if (*in == ':')
				in++;
			MKDir(part, in);
		}
		break;
		case 'C':
		{
			char* in = (char*)channel.buffer;
			int part;

			part = ParsePartition(&in);
			if (part > 0)
			{
				// Only have one drive partition
				//Error(ERROR_74_DRlVE_NOT_READY);
				return;
			}
			in += 2;	// Skip command
			if (*in == ':')
				in++;
			CD(part, in);
		}
		break;
		case 'R':
			RMDir();
		break;
		default:
			Error(ERROR_31_SYNTAX_ERROR);
		break;
	}
}

void IEC_Commands::Copy(void)
{
	//COPY:newfile = oldfile1, oldfile2,...

	// Only named data records can be combined.

	// TODO: checkfor wildcards and set the error if found.
	char filenameNew[256];
	char filenameToCopy[256];
	Channel& channel = channels[15];

	FILINFO filInfo;
	FRESULT res;
	const char* text = (char*)channel.buffer;

	text = ParseNextName(text, filenameNew, true);

	//DEBUG_LOG("Copy %s\r\n", filenameNew);
	if (filenameNew[0] != 0)
	{
		res = f_stat(filenameNew, &filInfo);
		if (res == FR_NO_FILE)
		{
			int fileCount = 0;
			do
			{
				text = ParseNextName(text, filenameToCopy, true);
				if (filenameToCopy[0] != 0)
				{
					//DEBUG_LOG("Copy source %s\r\n", filenameToCopy);
					res = f_stat(filenameToCopy, &filInfo);
					if (res == FR_OK)
					{
						if (!IsDirectory(filInfo))
						{
							//DEBUG_LOG("copying %s to %s\r\n", filenameToCopy, filenameNew);
							if (CopyFile(filenameNew, filenameToCopy, fileCount != 0)) updateAction = REFRESH;
							else Error(ERROR_25_WRITE_ERROR);
						}
					}
					else
					{
						// If you want to copy the entire folder then implement that here.
						Error(ERROR_62_FILE_NOT_FOUND);
					}
				}
				fileCount++;
			} while (filenameToCopy[0] != 0);
		}
		else
		{
			DEBUG_LOG("Copy file exists\r\n");
			Error(ERROR_63_FILE_EXISTS);
		}
	}
	else
	{
		Error(ERROR_34_SYNTAX_ERROR);
	}
}

void IEC_Commands::ChangeDevice(void)
{
	Channel& channel = channels[15];
	const char* text = (char*)channel.buffer;

	if (strlen(text) > 2)
	{
		int deviceIndex = atoi(text + 2);

		if (deviceIndex == 0)
		{
			SwitchDrive("SD:");
			displayingDevices = false;
			updateAction = DEVICE_SWITCHED;
		}
		else if ((deviceIndex - 1) < numberOfUSBMassStorageDevices)
		{
			char USBDriveId[16];
			sprintf(USBDriveId, "USB%02d:", deviceIndex);
			SwitchDrive(USBDriveId);
			displayingDevices = false;
			updateAction = DEVICE_SWITCHED;
		}
		else
		{
			Error(ERROR_74_DRlVE_NOT_READY);
		}
	}
	else
	{
		Error(ERROR_31_SYNTAX_ERROR);
	}
}

void IEC_Commands::Memory(void)
{
	Channel& channel = channels[15];
	char* text = (char*)channel.buffer;
	u16 address;
	int length;
	u8 bytes = 1;
	u8* ptr;

	if (channel.cursor > 2)
	{
		char code = toupper(channel.buffer[2]);
		if (code == 'R' || code == 'W' || code == 'E')
		{
			ptr = (u8*)strchr(text, ':');
			if (ptr == 0) ptr = (u8*)&channel.buffer[3];
			else ptr++;

			length = channel.cursor - 3;

			address = (u16)((u8)(ptr[1]) << 8) | (u16)ptr[0];
			if (length > 2)
			{
				bytes = ptr[2];
				if (bytes == 0)
					bytes = 1;
			}

			switch (code)
			{
				case 'R':
					DEBUG_LOG("M-R %04x %d\r\n", address, bytes);
				break;
				case 'W':
					DEBUG_LOG("M-W %04x %d\r\n", address, bytes);
				break;
				case 'E':
					// Memory execute impossible at this level of emulation!
					DEBUG_LOG("M-E %04x\r\n", address);
				break;
			}
		}
		else
		{
			Error(ERROR_31_SYNTAX_ERROR);
		}
	}
}

void IEC_Commands::New(void)
{
	Channel& channel = channels[15];
	char filenameNew[256];
	char ID[256];

	if (ParseFilenames((char*)channel.buffer, filenameNew, ID))
	{
		FILINFO filInfo;

		int ret = CreateNewDisk(filenameNew, ID, true);

		if (ret==0)
			updateAction = REFRESH;
		else
			Error(ret);
	}
}

void IEC_Commands::Rename(void)
{
	// RENAME:newname=oldname

	// Note: 1541 ROM will not allow you to rename a file until it is closed.

	Channel& channel = channels[15];
	char filenameNew[256];
	char filenameOld[256];

	if (ParseFilenames((char*)channel.buffer, filenameNew, filenameOld))
	{
		FRESULT res;
		FILINFO filInfo;

		res = f_stat(filenameNew, &filInfo);
		if (res == FR_NO_FILE)
		{
			res = f_stat(filenameOld, &filInfo);
			if (res == FR_OK)
			{
				// Rename folders too.
				//DEBUG_LOG("Renaming %s to %s\r\n", filenameOld, filenameNew);
				f_rename(filenameOld, filenameNew);
			}
			else
			{
				Error(ERROR_62_FILE_NOT_FOUND);
			}
		}
		else
		{
			Error(ERROR_63_FILE_EXISTS);
		}
	}
}

void IEC_Commands::Scratch(void)
{
	// SCRATCH: filename1, filename2...

	// More than one file can be deleted by using a single command.
	// But remember that only 40 characters at a time can be sent
	// over the transmission channel to the disk drive.

	// wildcard characters can be used

	Channel& channel = channels[15];
	DIR dir;
	FILINFO filInfo;
	FRESULT res;
	char filename[256];
	const char* text = (const char*)channel.buffer;

	text = ParseNextName(text, filename, true);
	while (filename[0])
	{
		res = f_findfirst(&dir, &filInfo, ".", (const TCHAR*)filename);
		while (res == FR_OK && filInfo.fname[0])
		{
			if (filInfo.fname[0] != 0 && !IsDirectory(filInfo))
			{
				//DEBUG_LOG("Scratching %s\r\n", filInfo.fname);
				f_unlink(filInfo.fname);
			}
			res = f_findnext(&dir, &filInfo);
			updateAction = REFRESH;
		}
		text = ParseNextName(text, filename, true);
	}
}

void IEC_Commands::User(void)
{
	Channel& channel = channels[15];

	//DEBUG_LOG("User channel.buffer[1] = %c\r\n", channel.buffer[1]);

	switch (toupper(channel.buffer[1]))
	{
		case 'A':
		case 'B':
		case '1':	// Direct access block read (Jumps via FFEA to B-R function)
		case '2':	// Direct access block write (Jumps via FFEC to B-W function)
			// Direct acces is unsupported. Without a mounted disk image tracks and sectors have no meaning.
			Error(ERROR_31_SYNTAX_ERROR);
		break;

		// U3/C - U8/H meaningless at this level of emulation!

		// U9 (UI)
		case 'I':
		case '9':
			//DEBUG_LOG("ui c=%d\r\n", channel.cursor);
			if (channel.cursor == 2)
			{
				// Soft reset
				Error(ERROR_73_DOSVERSION);
				return;
			}
			switch (channel.buffer[2])
			{
				case '+':
					usingVIC20 = true;
				break;
				case '-':
					usingVIC20 = false;
				break;
				default:
					Error(ERROR_73_DOSVERSION);
				break;
			}
		break;

		case 'J':
		case ':':
			// Hard reset
			Error(ERROR_73_DOSVERSION);
		break;
		case 202:
			// Really hard reset - reboot Pi
			Reboot_Pi();
		break;
		case '0':
			//OPEN1,8,15,"U0>"+CHR$(9):CLOSE1
			if ((channel.buffer[2] & 0x1f) == 0x1e && channel.buffer[3] >= 4 && channel.buffer[3] <= 30)
			{
				SetDeviceId(channel.buffer[3]);
				updateAction = DEVICEID_CHANGED;
				DEBUG_LOG("Changed deviceID to %d\r\n", channel.buffer[3]);
			}
			else
			{
				Error(ERROR_31_SYNTAX_ERROR);
			}
		break;
		default:
			Error(ERROR_31_SYNTAX_ERROR);
		break;
	}
}

void IEC_Commands::Extended(void)
{
	Channel& channel = channels[15];

	//DEBUG_LOG("User channel.buffer[1] = %c\r\n", channel.buffer[1]);

	switch (toupper(channel.buffer[1]))
	{
		case '?':
			Error(ERROR_73_DOSVERSION);
		break;
		default:
			// Extended commands not implemented yet
			Error(ERROR_31_SYNTAX_ERROR);
		break;
	}
}

// http://www.n2dvm.com/UIEC.pdf
void IEC_Commands::ProcessCommand(void)
{
	Error(ERROR_00_OK);

	Channel& channel = channels[15];

	//DEBUG_LOG("CMD %s %d\r\n", channel.buffer, channel.cursor);

	if (channel.cursor > 0 && channel.buffer[channel.cursor - 1] == 0x0d)
		channel.cursor--;

	if (channel.cursor == 0)
	{
		Error(ERROR_30_SYNTAX_ERROR);
	}
	else
	{
		//DEBUG_LOG("ProcessCommand %s", channel.buffer);

		if (toupper(channel.buffer[0]) != 'X' && toupper(channel.buffer[1]) == 'D')
		{
			FolderCommand();
			return;
		}

		switch (toupper(channel.buffer[0]))
		{
			case 'B':
				// B-P not implemented
				// B-A allocate bit in BAM not implemented
				// B-F free bit in BAM not implemented
				// B-E block execute impossible at this level of emulation!
				Error(ERROR_31_SYNTAX_ERROR);
			break;
			case 'C':
				if (channel.buffer[1] == 'P')
					ChangeDevice();
				else
					Copy();
			break;
			case 'D':
				Error(ERROR_31_SYNTAX_ERROR);	// DI, DR, DW not implemented yet
			break;
			case 'G':
				Error(ERROR_31_SYNTAX_ERROR);	// G-P not implemented yet
			break;
			case 'I':
				// Initialise
			break;
			case 'M':
				Memory();
			break;
			case 'N':
				New();
			break;
			case 'P':
				Error(ERROR_31_SYNTAX_ERROR);	// P not implemented yet
			break;
			case 'R':
				Rename();
			break;
			case 'S':
				if (channel.buffer[1] == '-')
				{
					// Swap drive number 
					Error(ERROR_31_SYNTAX_ERROR);
					break;
				}
				Scratch();
			break;
			case 'T':
				// RTC support
				Error(ERROR_31_SYNTAX_ERROR);	// T-R and T-W not implemented yet
			break;
			case 'U':
				User();
			break;
			case 'V':
			break;
			case 'W':
				// Save out current options?
				//OPEN1, 9, 15, "XW":CLOSE1
			break;
			case 'X':
				Extended();
			break;
			case '/':
				
			break;
			default:
				Error(ERROR_31_SYNTAX_ERROR);
			break;
		}
	}
}

void IEC_Commands::Listen()
{
	u8 byte;

	if ((commandCode & 0x0f) == 0x0f || (commandCode & 0xf0) == 0xf0)
	{
		Channel& channel = channels[15];
		channel.Close();

		while (!ReadIECSerialPort(byte))
		{
			if (!channel.WriteFull())
			{
				channel.buffer[channel.cursor++] = byte;
			}
			if (receivedEOI)
				receivedCommand = true;
		}

		if (channel.cursor > 1)
		{
			// Strip any CRs from the command
			if (channel.buffer[channel.cursor - 1] == 0x0d) channel.cursor -= 1;
			else if (channel.buffer[channel.cursor - 2] == 0x0d) channel.cursor -= 2;
		}

		// TODO: Should not do this - should use command length to test for the end of a command
		if (!channel.WriteFull())
			channel.buffer[channel.cursor++] = 0;
	}
	else
	{
		OpenFile();
		SaveFile();
	}
}

void IEC_Commands::Talk()
{
	if (commandCode == 0x6f)
	{
		SendError();
	}
	else
	{
		Channel& channelCommand = channels[15];
		//DEBUG_LOG("cmd = %s\r\n", channelCommand.buffer);

		if (channelCommand.buffer[0] == '$')
		{
			LoadDirectory();
		}
		else
		{
			OpenFile();
			LoadFile();
		}
	}
}

bool IEC_Commands::FindFirst(DIR& dir, const char* matchstr, FILINFO& filInfo)
{
	char pattern[256];
	FRESULT res;

	// CBM-FileBrowser can only determine if it is a disk image if the extention is in the name.
	// So for files that are too long we stomp the last 4 characters with the image extention and pattern match it.
	// This basically changes a file name from something like
	// SOMELONGDISKIMAGENAME.D64 to SOMELONGDISKIMAGENAME*.D64
	// so the actual SOMELONGDISKIMAGENAMETHATISWAYTOOLONGFORCBMFILEBROWSERTODISPLAY.D64 will be found.
	bool diskImage = DiskImage::IsDiskImageExtention(matchstr);
	strcpy(pattern, matchstr);
	if (strlen(pattern) > CBM_NAME_LENGTH_MINUS_D64)
	{
		char* ext = strrchr(matchstr, '.');
		if (ext && diskImage)
		{
			char* ptr = strrchr(pattern, '.');
			*ptr++ = '*';
			for (int i = 0; i < 4; i++)
			{
				*ptr++ = *ext++;
			}
			*ptr = 0;
		}
		else
		{
			// For folders we do the same except we need to change the last character to a *
			int len = strlen(matchstr);
			if (len >= CBM_NAME_LENGTH)
				pattern[CBM_NAME_LENGTH - 1] = '*';
		}
	}
	//DEBUG_LOG("Pattern %s -> %s\r\n", matchstr, pattern);
	res = f_findfirst(&dir, &filInfo, ".", (const TCHAR*)pattern);
	//DEBUG_LOG("found file %s\r\n", filInfo.fname);
	if (res != FR_OK || filInfo.fname[0] == 0)
	{
		//Error(ERROR_62_FILE_NOT_FOUND);
		return false;
	}
	else
	{
		return true;
	}
}

bool IEC_Commands::SendBuffer(Channel& channel, bool eoi)
{
	for (u32 i = 0; i < channel.cursor; ++i)
	{
		u8 finalbyte = eoi && (channel.bytesSent == (channel.fileSize - 1));
		if (WriteIECSerialPort(channel.buffer[i], finalbyte))
		{
			return true;
		}
		channel.bytesSent++;
	}
	channel.cursor = 0;
	return false;
}

void IEC_Commands::LoadFile()
{
	Channel& channel = channels[secondaryAddress];

	//DEBUG_LOG("LoadFile %s %s\r\n", channel.buffer, channel.filInfo.fname);

	if (channel.filInfo.fname[0] != 0)
	{
		FSIZE_t size = f_size(&channel.file);
		FSIZE_t sizeRemaining = size;
		u32 bytesRead;
		channel.fileSize = (u32)channel.filInfo.fsize;

		char* ext = strrchr((char*)channel.filInfo.fname, '.');
		if (toupper((char)ext[1]) == 'P' && isdigit(ext[2]) && isdigit(ext[3]))
		{
			bool validP00 = false;

			f_read(&channel.file, channel.buffer, 26, &bytesRead);
			if (bytesRead > 0)
			{
				if (strncmp((const char*)channel.buffer, "C64File", 7) == 0)
				{
					validP00 = channel.buffer[0x19] == 0;
					sizeRemaining -= bytesRead;
					channel.bytesSent += bytesRead;
				}

				if (!validP00)
					f_lseek(&channel.file, 0);
			}
		}
		else if (toupper((char)ext[1]) == 'T' && ext[2] == '6' && ext[3] == '4')
		{
			bool validT64 = false;

			f_read(&channel.file, channel.buffer, sizeof(channel.buffer), &bytesRead);

			if (bytesRead > 0)
			{
				if ((memcmp(channel.buffer, "C64 tape image file", 20) == 0) || (memcmp(channel.buffer, "C64s tape image file", 21) == 0))
				{
					DEBUG_LOG("T64\r\n");
					u16 version = channel.buffer[0x20] | (channel.buffer[0x21] << 8);
					u16 entries = channel.buffer[0x22] | (channel.buffer[0x23] << 8);
					u16 entriesUsed = channel.buffer[0x24] | (channel.buffer[0x25] << 8);
					char name[25] = { 0 };
					strncpy(name, (const char*)(channel.buffer + 0x28), 24);

					DEBUG_LOG("%x %d %d %s\r\n", version, entries, entriesUsed, name);

					u16 entryIndex;

					for (entryIndex = 0; entryIndex < entriesUsed; ++entryIndex)
					{
						char nameEntry[17] = { 0 };
						int offset = 0x40 + entryIndex * 32;
						u8 type = channel.buffer[offset];
						u8 fileType = channel.buffer[offset + 1];
						u16 startAddress = channel.buffer[offset + 2] | (channel.buffer[offset + 3] << 8);
						u16 endAddress = channel.buffer[offset + 4] | (channel.buffer[offset + 5] << 8);
						u32 fileOffset = channel.buffer[offset + 8] | (channel.buffer[offset + 9] << 8) | (channel.buffer[offset + 10] << 16) | (channel.buffer[offset + 11] << 24);
						strncpy(nameEntry, (const char*)(channel.buffer + offset + 0x10), 16);

						DEBUG_LOG("%d %02x %04x %04x %0x8 %s\r\n", type, fileType, startAddress, endAddress, fileOffset, nameEntry);

						channel.bytesSent = 0;
						channel.buffer[0] = startAddress & 0xff;
						channel.buffer[1] = (startAddress >> 8) & 0xff;
	
						validT64 = true;
						sizeRemaining = endAddress - startAddress;
						channel.fileSize = sizeRemaining + 2;
						channel.bytesSent = 0;
						channel.cursor = 2;

						SendBuffer(channel, false);

						f_lseek(&channel.file, fileOffset);

						break; // For now only load the first file.
					}
				}

				if (!validT64)
					f_lseek(&channel.file, 0);
			}
		}

		do
		{
			f_read(&channel.file, channel.buffer, sizeof(channel.buffer), &bytesRead);
			if (bytesRead > 0)
			{
				//DEBUG_LOG("%d %d %d\r\n", (int)size, bytesRead, (int)sizeRemaining);
				sizeRemaining -= bytesRead;
				channel.cursor = bytesRead;
				if (SendBuffer(channel, sizeRemaining <= 0))
					return;
			}
		}
		while (bytesRead > 0);
	}
	else
	{
		//DEBUG_LOG("Can't find %s", channel.buffer);
		Error(ERROR_62_FILE_NOT_FOUND);
	}
}

void IEC_Commands::SaveFile()
{
	u32 bytesWritten;
	u8 byte;

	Channel& channel = channels[secondaryAddress];
	if (channel.open && channel.writing)
	{
		while (!ReadIECSerialPort(byte))
		{
			channel.buffer[channel.cursor++] = byte;
			if (channel.WriteFull())
			{
				if (f_write(&channel.file, channel.buffer, sizeof(channel.buffer), &bytesWritten) != FR_OK)
				{
				}
				channel.cursor = 0;
			}
		}
	}
}

void IEC_Commands::SendError()
{
	int len = strlen(ErrorMessage);
	int index = 0;
	bool finalByte;
	do
	{
		finalByte = index == len;
		if (WriteIECSerialPort(ErrorMessage[index++], finalByte))
			break;
	}
	while (!finalByte);
}

u8 IEC_Commands::GetFilenameCharacter(u8 value)
{
	if (lowercaseBrowseModeFilenames)
		value = tolower(value);

	return ascii2petscii(value);
}

void IEC_Commands::AddDirectoryEntry(Channel& channel, const char* name, u16 blocks, int fileType)
{
	u8* data = channel.buffer + channel.cursor;
	const u32 dirEntryLength = DIRECTORY_ENTRY_SIZE;
	int i = 0;
	int index = 0;
	bool diskImage = DiskImage::IsDiskImageExtention(name);

	//DEBUG_LOG("name = %s blocks = %d", name, blocks);

	memset(data, ' ', dirEntryLength);
	data[dirEntryLength - 1] = 0;

	data[index++] = 0x01;
	data[index++] = 0x01;
	data[index++] = blocks & 0xff;
	data[index++] = blocks >> 8;

	if (blocks < 1000)
		index++;
	if (blocks < 100)
		index++;
	if (blocks < 10)
		index++;

	data[index++] = '"';

	// CBM-FileBrowser can only determine if it is a disk image if the extention is in the name.
	// So for files that are too long we stomp the last 4 characters with the image extention and pattern match it.
	// This basically changes a file name from something like
	// SOMELONGDISKIMAGENAME.D64 to SOMELONGDISKIMAGENAME*.D64
	// so the actual SOMELONGDISKIMAGENAMETHATISWAYTOOLONGFORCBMFILEBROWSERTODISPLAY.D64 will be found.
	if (strlen(name) > CBM_NAME_LENGTH && diskImage)
	{
		const char* extName = strrchr(name, '.');

		do
		{
			data[index + i++] = GetFilenameCharacter(*name++);
		}
		while (!(*name == 0x22 || *name == 0 || i == CBM_NAME_LENGTH_MINUS_D64));

		for (int extIndex = 0; extIndex < 4; ++extIndex)
		{
			data[index + i++] = GetFilenameCharacter(*extName++);
		}
	}
	else
	{
		do
		{
			data[index + i++] = GetFilenameCharacter(*name++);
		}
		while (!(*name == 0x22 || *name == 0 || i == CBM_NAME_LENGTH));
	}
	data[index + i] = '"';
	index++;
	index += CBM_NAME_LENGTH;
	index++;

	for (i = 0; i < 3; ++i)
	{
		data[index++] = filetypes[fileType * 3 + i];
	}
	channel.cursor += dirEntryLength;
}

struct greater
{
	bool operator()(const FileBrowser::BrowsableList::Entry& lhs, const FileBrowser::BrowsableList::Entry& rhs) const
	{
		if (strcasecmp(lhs.filImage.fname, "..") == 0)
			return true;
		else if (strcasecmp(rhs.filImage.fname, "..") == 0)
			return false;
		else if (((lhs.filImage.fattrib & AM_DIR) && (rhs.filImage.fattrib & AM_DIR)) || (!(lhs.filImage.fattrib & AM_DIR) && !(rhs.filImage.fattrib & AM_DIR)))
			return strcasecmp(lhs.filImage.fname, rhs.filImage.fname) < 0;
		else if ((lhs.filImage.fattrib & AM_DIR) && !(rhs.filImage.fattrib & AM_DIR))
			return true;
		else
			return false;
	}
};

void IEC_Commands::LoadDirectory()
{
	DIR dir;
	char* ext;
	FRESULT res;

	Channel& channel = channels[0];

	memcpy(channel.buffer, DirectoryHeader, sizeof(DirectoryHeader));
	channel.cursor = sizeof(DirectoryHeader);


	FileBrowser::BrowsableList::Entry entry;
	std::vector<FileBrowser::BrowsableList::Entry> entries;

	if (displayingDevices)
	{
		FileBrowser::RefreshDevicesEntries(entries, true);
	}
	else
	{
		res = f_opendir(&dir, ".");
		if (res == FR_OK)
		{
			do
			{
				res = f_readdir(&dir, &entry.filImage);
				ext = strrchr(entry.filImage.fname, '.');
				if (res == FR_OK && entry.filImage.fname[0] != 0 && !(ext && strcasecmp(ext, ".png") == 0) && (entry.filImage.fname[0] != '.'))
					entries.push_back(entry);
			} while (res == FR_OK && entry.filImage.fname[0] != 0);
			f_closedir(&dir);

			std::sort(entries.begin(), entries.end(), greater());
		}
	}

	for (u32 i = 0; i < entries.size(); ++i)
	{
		FILINFO* filInfo = &entries[i].filImage;
		const char* fileName = filInfo->fname;

		if (!channel.CanFit(DIRECTORY_ENTRY_SIZE))
			SendBuffer(channel, false);

		if (filInfo->fattrib & AM_DIR) AddDirectoryEntry(channel, fileName, 0, 6);
		else AddDirectoryEntry(channel, fileName, filInfo->fsize / 256 + 1, 2);
	}



	SendBuffer(channel, false);

	memcpy(channel.buffer, DirectoryBlocksFree, sizeof(DirectoryBlocksFree));

	FATFS* fs;
	DWORD fre_clust, fre_sect, free_blocks;
	res = f_getfree("", &fre_clust, &fs);
	if (res == FR_OK)
	{
		fre_sect = fre_clust * fs->csize;
		free_blocks = fre_sect << 1;

		if (free_blocks > 0x10000)
		{
			channel.buffer[2] = 0xff;
			channel.buffer[3] = 0xff;
		}
		else
		{
			channel.buffer[2] = free_blocks & 0xff;
			channel.buffer[3] = (free_blocks >> 8) & 0xff;;
		}
	}
	else
	{
		channel.buffer[2] = 0;
		channel.buffer[3] = 0;
	}
	channel.cursor = sizeof(DirectoryBlocksFree);
	
	channel.filInfo.fsize = channel.bytesSent + channel.cursor;
	channel.fileSize = (u32)channel.filInfo.fsize;
	SendBuffer(channel, true);
}

void IEC_Commands::OpenFile()
{
	// OPEN lfn,id,sa,"filename,filetype,mode"
	// lfn
	//		When the logical file number(lfn) is between 1 and 127, a PRINT# statement sends a RETURN character to the file after each variable.
	//		If the logical file number is greater than 127 (128 - 255), the PRINT# statement sends an additional linefeed after each RETURN.
	// sa
	//		Should several files be open at once, they must all use different secondary addresses, as only one file can use a channel.
	// mode
	// The last parameter(mode) establishes how the channel will used. There are four possibilities:
	//	W  Write a file
	//	R  Read a file
	//	A  Add to a sequential file
	//	M  read a file that has not been closed	

	// If a file that already exists is to be to opened for writing, the file 	must first be deleted.

	// The file type must be given when the file is opened. The file type may be shortened to one of following:
	//	S - sequential file
	//	U - user file
	//	P - program
	//	R - relative file
	u8 secondary = secondaryAddress;
	Channel& channel = channels[secondary];
	if (channel.command[0] == '#')
	{
		Channel& channelCommand = channels[15];

		// Direct acces is unsupported. Without a mounted disk image tracks and sectors have no meaning.
		//DEBUG_LOG("Driect access\r\n");
		if (strcmp((char*)channelCommand.buffer, "U1:13 0 01 00") == 0)
		{
			// This is a 128 trying to auto boot
			memset(channel.buffer, 0, 256);
			channel.cursor = 256;

			if (autoBootFB128)
			{
				int index = 0;
				channel.buffer[0] = 'C';
				channel.buffer[1] = 'B';
				channel.buffer[2] = 'M';
				index += 3;
				index += 4;
				channel.buffer[index++] = 'P';
				channel.buffer[index++] = 'I';
				channel.buffer[index++] = '1';
				channel.buffer[index++] = '5';
				channel.buffer[index++] = '4';
				channel.buffer[index++] = '1';
				channel.buffer[index++] = ' ';
				channel.buffer[index++] = 'F';
				channel.buffer[index++] = 'B';
				channel.buffer[index++] = '1';
				channel.buffer[index++] = '2';
				channel.buffer[index++] = '8';
				index++;
				channel.buffer[index++] = 'F';
				channel.buffer[index++] = 'B';
				channel.buffer[index++] = '1';
				channel.buffer[index++] = '2';
				channel.buffer[index++] = '8';
				index++;
				channel.buffer[index++] = 0xa2;
				channel.buffer[index] = (index + 5);
				index++;
				channel.buffer[index++] = 0xa0;
				channel.buffer[index++] = 0xb;
				channel.buffer[index++] = 0x4c;
				channel.buffer[index++] = 0xa5;
				channel.buffer[index++] = 0xaf;
				channel.buffer[index++] = 'R';
				channel.buffer[index++] = 'U';
				channel.buffer[index++] = 'N';
				channel.buffer[index++] = '\"';
				channel.buffer[index++] = 'F';
				channel.buffer[index++] = 'B';
				channel.buffer[index++] = '1';
				channel.buffer[index++] = '2';
				channel.buffer[index++] = '8';
				channel.buffer[index++] = '\"';
				channel.fileSize = 256;
			}
			if (C128BootSectorName)
			{
				FIL fpBS;
				u32 bytes;
				if (FR_OK == f_open(&fpBS, C128BootSectorName, FA_READ))
					f_read(&fpBS, channel.buffer, 256, &bytes);
				else
					memset(channel.buffer, 0, 256);
				channel.fileSize = 256;
			}

			if (SendBuffer(channel, true))
				return;
		}
	}
	else if (channel.command[0] == '$')
	{
	}
	else
	{
		if (!channel.open)
		{
			bool found = false;
			DIR dir;
			FRESULT res;
			const char* text;
			char filename[256];
			char filetype[8];
			char filemode[8];
			bool needFileToExist = true;
			bool writing = false;
			u8 mode = FA_READ;

			filetype[0] = 0;
			filemode[0] = 0;

			if (secondary == 1)
				strcat(filemode, "W");

			char* in = (char*)channel.command;
			int part = ParsePartition(&in);
			if (part > 0)
			{
				// Only have one drive partition
				//Error(ERROR_74_DRlVE_NOT_READY);
				return;
			}
			if (*in == ':')
				in++;
			else
				in = (char*)channel.command;

			text = ParseName((char*)in, filename, true, true);
			if (text)
			{
				text = ParseNextName(text, filetype, true);
				if (text)
					text = ParseNextName(text, filemode, true);
			}

			if (starFileName && starFileName[0] != 0 && filename[0] == '*')
			{
				char cwd[1024];
				if (f_getcwd(cwd, 1024) == FR_OK)
				{
					const char* folder = strstr(cwd, "/");
					if (folder)
					{
						if (strcasecmp(folder, "/1541") == 0)
						{
							strncpy(filename, starFileName, sizeof(filename) - 1);
						}
					}
				}
			}
			

			if (toupper(filetype[0]) == 'L')
			{
				//DEBUG_LOG("Rel file\r\n");
				return;
			}
			else
			{
				switch (toupper(filemode[0]))
				{
					case 'W':
						needFileToExist = false;
						writing = true;
						mode = FA_CREATE_ALWAYS | FA_WRITE;
					break;
					case 'A':
						needFileToExist = true;
						writing = true;
						mode = FA_OPEN_APPEND | FA_WRITE;
					break;
					case 'R':
						needFileToExist = true;
						writing = false;
						mode = FA_READ;
					break;
				}
			}

			channel.writing = writing;

			//DEBUG_LOG("OpenFile %s %d NE=%d T=%c M=%c W=%d %0x\r\n", filename, secondary, needFileToExist, filetype[0], filemode[0], writing, mode);

			if (needFileToExist)
			{
				if (FindFirst(dir, filename, channel.filInfo))
				{
					//DEBUG_LOG("found\r\n");
					res = FR_OK;
					while ((channel.filInfo.fattrib & AM_DIR) == AM_DIR)
					{
						res = f_findnext(&dir, &channel.filInfo);
					}

					if (res == FR_OK && channel.filInfo.fname[0] != 0)
					{
						found = true;
						res = f_open(&channel.file, channel.filInfo.fname, mode);
						if (res == FR_OK)
							channel.open = true;
						//DEBUG_LOG("Opened existing size = %d\r\n", (int)channel.filInfo.fsize);
					}
				}
				else
				{
					Error(ERROR_62_FILE_NOT_FOUND);
				}

				if (!found)
				{
					DEBUG_LOG("Can't find %s", filename);
					Error(ERROR_62_FILE_NOT_FOUND);
				}
			}
			else
			{
				res = f_open(&channel.file, filename, mode);
				if (res == FR_OK)
				{
					channel.open = true;
					channel.cursor = 0;
					//DEBUG_LOG("Opened new sa=%d m=%0x\r\n", secondary, mode);
					res = f_stat(filename, &channel.filInfo);
				}
				else
				{
					//DEBUG_LOG("Open failed %d\r\n", res);
				}
			}
		}
		else
		{
			//DEBUG_LOG("Channel aready opened %d\r\n", channel.cursor);
		}
	}
}

void IEC_Commands::CloseFile(u8 secondary)
{
	Channel& channel = channels[secondary];

	channel.Close();
}

int IEC_Commands::CreateNewDisk(char* filenameNew, char* ID, bool automount)
{
	DisplayMessage(240, 280, false, "Creating new disk", RGBA(0xff, 0xff, 0xff, 0xff), RGBA(0xff, 0, 0, 0xff));
	DisplayMessage(0, 0, true, "Creating new disk", RGBA(0xff, 0xff, 0xff, 0xff), RGBA(0xff, 0, 0, 0xff));

	switch (newDiskType)
	{
		case DiskImage::D64:
			if (!(strstr(filenameNew, ".d64") || strstr(filenameNew, ".D64")))
				strcat(filenameNew, ".d64");
		break;
		case DiskImage::G64:
			if (!(strstr(filenameNew, ".g64") || strstr(filenameNew, ".G64")))
				strcat(filenameNew, ".g64");
		break;
		default:
			return ERROR_25_WRITE_ERROR;
		break;
	}

	unsigned length = DiskImage::CreateNewDiskInRAM(filenameNew, ID);

	return WriteNewDiskInRAM(filenameNew, automount, length);
}


int IEC_Commands::WriteNewDiskInRAM(char* filenameNew, bool automount, unsigned length)
{
	FILINFO filInfo;
	FRESULT res;

	res = f_stat(filenameNew, &filInfo);
	if (res == FR_NO_FILE)
	{
		DiskImage diskImage;
		diskImage.OpenD64((const FILINFO*)0, (unsigned char*)DiskImage::readBuffer, length);

		switch (newDiskType)
		{
			case DiskImage::D64:
				if (!diskImage.WriteD64(filenameNew))
					return ERROR_25_WRITE_ERROR;
			break;
			case DiskImage::G64:
				if (!diskImage.WriteG64(filenameNew))
					return ERROR_25_WRITE_ERROR;
			break;
			default:
				return ERROR_25_WRITE_ERROR;
			break;
		}

		// Mount the new disk? Shoud we do this or let them do it manually?
		if (automount && f_stat(filenameNew, &filInfo) == FR_OK)
		{
			DIR dir;
			Enter(dir, filInfo);
		}
		return(ERROR_00_OK);
	}
	else
	{
		return(ERROR_63_FILE_EXISTS);
	}
}
