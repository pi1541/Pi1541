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

#ifndef IEC_COMMANDS_H
#define IEC_COMMANDS_H

#include "iec_bus.h"
#include "ff.h"
#include "debug.h"

struct TimerMicroSeconds
{
	TimerMicroSeconds()
	{
		count = 0;
		timeout = 0;
	}

	void Start(u32 amount)
	{
		count = 0;
		timeout = amount;
	}

	inline bool TimedOut() { return count >= timeout; }

	bool Tick()
	{
		delay_us(1);
		count++;
		return TimedOut();
	}

	u32 count;
	u32 timeout;
};

class IEC_Commands
{
public:
	enum UpdateAction
	{
		NONE,
		IMAGE_SELECTED,
		DIR_PUSHED,
		POP_DIR,
		POP_TO_ROOT,
		REFRESH,
		DEVICEID_CHANGED,
		RESET
	};

	IEC_Commands();
	void Initialise();

	void SetDeviceId(u8 id) { deviceID = id; }
	u8 GetDeviceId() { return deviceID; }

	void Reset(void);
	void SimulateIECBegin(void);
	UpdateAction SimulateIECUpdate(void);

	const char* GetNameOfImageSelected() const { return selectedImageName; }
	const FILINFO* GetImageSelected() const { return &filInfoSelectedImage; }
protected:
	enum ATNSequence 
	{
		ATN_SEQUENCE_IDLE,
		ATN_SEQUENCE_ATN,
		ATN_SEQUENCE_RECEIVE_COMMAND_CODE,
		ATN_SEQUENCE_HANDLE_COMMAND_CODE,
		ATN_SEQUENCE_COMPLETE
	};

	enum DeviceRole
	{
		DEVICE_ROLE_PASSIVE,
		DEVICE_ROLE_LISTEN,
		DEVICE_ROLE_TALK
	};

	struct Channel
	{
		FILINFO filInfo;
		FIL file;
		bool writing;
		u32 cursor;
		u32 bytesSent;
		bool open;

		u8 buffer[0x1000];
		u8 command[0x100];

		void Close();
		bool WriteFull() const { return cursor >= sizeof(buffer); }
		bool CanFit(u32 bytes) const { return bytes <= sizeof(buffer) - cursor; }
	};

	bool CheckATN(void);
	bool WriteIECSerialPort(u8 data, bool eoi);
	bool ReadIECSerialPort(u8& byte);

	void Listen();
	void Talk();
	void LoadFile();
	void SaveFile();

	void AddDirectoryEntry(Channel& channel, const char* name, u16 blocks, int fileType);
	void LoadDirectory();
	void OpenFile();
	void CloseFile(u8 secondary);
	void CloseAllChannels();
	void SendError();

	bool Enter(DIR& dir, FILINFO& filInfo);
	bool FindFirst(DIR& dir, const char* matchstr, FILINFO& filInfo);

	void FolderCommand(void);
	void CD(int partition, char* filename);
	void MKDir(int partition, char* filename);
	void RMDir(void);

	void Copy(void);
	void New(void);
	void Rename(void);
	void Scratch(void);

	void Memory(void);
	void User(void);

	void ProcessCommand(void);

	bool SendBuffer(Channel& channel, bool eoi);

	UpdateAction updateAction;
	u8 commandCode;
	bool receivedCommand : 1;
	bool receivedEOI : 1;	// End Or Identify
	bool usingVIC20 : 1;	// When sending data we need to wait longer for the 64 as its VICII may be stealing its cycles. VIC20 does not have this problem and can accept data faster.

	u8 deviceID;
	u8 secondaryAddress;
	ATNSequence atnSequence;
	DeviceRole deviceRole;

	TimerMicroSeconds timer;

	Channel channels[16];

	char selectedImageName[256];
	FILINFO filInfoSelectedImage;
};
#endif