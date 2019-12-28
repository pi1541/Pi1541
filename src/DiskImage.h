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

#ifndef DISKIMAGE_H
#define DISKIMAGE_H
#include "types.h"
#include "ff.h"

#define READBUFFER_SIZE 1024 * 512 * 2 // Now need over 800K for D81s

#define MAX_TRACK_LENGTH 0x2000
#define NIB_TRACK_LENGTH 0x2000

#define BAM_OFFSET 4
#define BAM_ENTRY_SIZE 4

#define DIR_ENTRY_OFFSET_TYPE 0
#define DIR_ENTRY_OFFSET_NAME 3
#define DIR_ENTRY_OFFSET_BLOCKS 28

#define DIR_ENTRY_NAME_LENGTH 18-2

static const unsigned char HALF_TRACK_COUNT = 84;
static const unsigned char D71_HALF_TRACK_COUNT = 70;
static const unsigned char D81_TRACK_COUNT = 80;
static const unsigned short GCR_SYNC_LENGTH = 5;
static const unsigned short GCR_HEADER_LENGTH = 10;
static const unsigned short GCR_HEADER_GAP_LENGTH = 8;
static const unsigned short GCR_SECTOR_DATA_LENGTH = 325;
static const unsigned short GCR_SECTOR_GAP_LENGTH = 8;
static const unsigned short GCR_SECTOR_LENGTH = GCR_SYNC_LENGTH + GCR_HEADER_LENGTH + GCR_HEADER_GAP_LENGTH + GCR_SYNC_LENGTH + GCR_SECTOR_DATA_LENGTH + GCR_SECTOR_GAP_LENGTH;	//361

static const unsigned short G64_MAX_TRACK_LENGTH = 7928;

static const unsigned short D81_SECTOR_LENGTH = 512;

class DiskImage
{
public:
	enum DiskType
	{
		NONE,
		D64,
		G64,
		NIB,
		NBZ,
		LST,
		D71,
		D81,
		T64,
		PRG,
		RAW
	};

	DiskImage();

	static unsigned CreateNewDiskInRAM(const char* filenameNew, const char* ID, unsigned char* destBuffer = 0);

	bool OpenD64(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size);
	bool OpenG64(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size);
	bool OpenNIB(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size);
	bool OpenNBZ(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size);
	bool OpenD71(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size);
	bool OpenD81(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size);
	bool OpenT64(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size);
	bool OpenPRG(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size);

	void Close();

	bool GetDecodedSector(u32 track, u32 sector, u8* buffer);

	inline unsigned char GetNextByte(u32 track, u32 byte)
	{
#if defined(EXPERIMENTALZERO)
		return tracks[(track << 13) + byte];
#else
		return tracks[track][byte];
#endif
	}


	inline bool GetNextBit(u32 track, u32 byte, u32 bit)
	{
		//if (attachedImageSize == 0)
		//	return 0;

#if defined(EXPERIMENTALZERO)
		return ((tracks[(track << 13) + byte] >> bit) & 1) != 0;
#else
		return ((tracks[track][byte] >> bit) & 1) != 0;
#endif
	}


	inline void SetBit(u32 track, u32 byte, u32 bit, bool value)
	{
		if (attachedImageSize == 0)
			return;

#if defined(EXPERIMENTALZERO)
		u8 dataOld = tracks[(track << 13) + byte];
		u8 bitMask = 1 << bit;
		if (value)
		{
			TestDirty(track, (dataOld & bitMask) == 0);
			tracks[(track << 13) + byte] |= bitMask;
		}
		else
		{
			TestDirty(track, (dataOld & bitMask) != 0);
			tracks[(track << 13) + byte] &= ~bitMask;
		}
#else
		u8 dataOld = tracks[track][byte];
		u8 bitMask = 1 << bit;
		if (value)
		{
			TestDirty(track, (dataOld & bitMask) == 0);
			tracks[track][byte] |= bitMask;
		}
		else
		{
			TestDirty(track, (dataOld & bitMask) != 0);
			tracks[track][byte] &= ~bitMask;
		}
#endif
	}

	static const unsigned char SectorsPerTrack[42];

	void DumpTrack(unsigned track);

	const char* GetName() { return fileInfo->fname; }

	inline unsigned BitsInTrack(unsigned track) const { return trackLengths[track] << 3; }
	inline unsigned TrackLength(unsigned track) const { return trackLengths[track]; }

	inline bool IsD81() const { return diskType == D81; }
	inline bool IsD71() const { return diskType == D71; }
	inline unsigned char GetD81Byte(unsigned track, unsigned headIndex, unsigned headPos) const { return tracksD81[track][headIndex][headPos]; }
	inline void SetD81Byte(unsigned track, unsigned headIndex, unsigned headPos, unsigned char data)
	{
		if (tracksD81[track][headIndex][headPos] != data)
		{
			tracksD81[track][headIndex][headPos] = data;
			trackDirty[track] = true;
			trackUsed[track] = true;
			dirty = true;
		}
	}

	inline bool IsD81ByteASync(unsigned track, unsigned headIndex, unsigned headPos) const
	{ 
		return (trackD81SyncBits[track][headIndex][headPos >> 3] & (1 << (headPos & 7))) != 0;
	}
	inline void SetD81SyncBit(unsigned track, unsigned headIndex, unsigned headPos, bool sync)
	{
		if (sync)
			trackD81SyncBits[track][headIndex][headPos >> 3] |= 1 << (headPos & 7);
		else
			trackD81SyncBits[track][headIndex][headPos >> 3] &= ~(1 << (headPos & 7));

	}

	static DiskType GetDiskImageTypeViaExtention(const char* diskImageName);
	static bool IsDiskImageExtention(const char* diskImageName);
	static bool IsDiskImageD81Extention(const char* diskImageName);
	static bool IsDiskImageD71Extention(const char* diskImageName);
	static bool IsLSTExtention(const char* diskImageName);

	bool GetReadOnly() const { return readOnly; }
	void SetReadOnly(bool readOnly) { this->readOnly = readOnly; }

	unsigned LastTrackUsed();

	bool IsDirty() const { return dirty; }

	static unsigned char readBuffer[READBUFFER_SIZE];

	static void CRC(unsigned short& runningCRC, unsigned char data);

	union
	{
#if defined(EXPERIMENTALZERO)
		unsigned char tracks[HALF_TRACK_COUNT * MAX_TRACK_LENGTH];
#else
		unsigned char tracks[HALF_TRACK_COUNT][MAX_TRACK_LENGTH];
#endif
		unsigned char tracksD81[HALF_TRACK_COUNT][2][MAX_TRACK_LENGTH];
	};

	bool WriteD64(char* name = 0);
	bool WriteG64(char* name = 0);

	unsigned GetHash() const { return hash; }

private:
	void CloseD64();
	void CloseG64();
	void CloseNIB();
	void CloseNBZ();
	void CloseD71();
	void CloseD81();
	void CloseT64();

	bool WriteNIB();
	bool WriteNBZ();
	bool WriteD71();
	bool WriteD81();
	bool WriteT64(char* name = 0);

	inline void TestDirty(u32 track, bool isDirty)
	{
		if (isDirty)
		{
			trackDirty[track] = true;
			trackUsed[track] = true;
			dirty = true;
		}
	}

	bool ConvertSector(unsigned track, unsigned sector, unsigned char* buffer);
	void DecodeBlock(unsigned track, int bitIndex, unsigned char* buf, int num);
	unsigned GetID(unsigned track, unsigned char* id);
	int FindSectorHeader(unsigned track, unsigned sector, unsigned char* id);
	int FindSync(unsigned track, int bitIndex, int maxBits, int* syncStartIndex = 0);

	void OutputD81HeaderByte(unsigned char*& dest, unsigned char byte);
	void OutputD81DataByte(unsigned char*& src, unsigned char*& dest);

	static bool AddFileToRAMD64(unsigned char* ramD64, const char* name, const unsigned char* data, unsigned length);
	static unsigned char* RAMD64AddDirectoryEntry(unsigned char* ramD64, const char* name, const unsigned char* data, unsigned length);
	static int RAMD64GetSectorOffset(int track, int sector);
	static int RAMD64FreeSectors(unsigned char* ramD64);
	static bool RAMD64FindFreeSector(bool searchForwards, unsigned char* ramD64, int lastTrackUsed, int lastSectorUsed, int& track, int& sector);
	static bool RAMD64AllocateSector(unsigned char* ramD64, int track, int sector);
	static bool WriteRAMD64(unsigned char* diskImage, unsigned size);
	
	bool readOnly;
	bool dirty;
	unsigned attachedImageSize;
	DiskType diskType;
	const FILINFO* fileInfo;
	unsigned hash;

	unsigned short trackLengths[HALF_TRACK_COUNT];
	union
	{
		unsigned char trackDensity[HALF_TRACK_COUNT];
		unsigned char trackD81SyncBits[HALF_TRACK_COUNT][2][MAX_TRACK_LENGTH >> 3];
	};
	bool trackDirty[HALF_TRACK_COUNT];
	bool trackUsed[HALF_TRACK_COUNT];

	unsigned short crc;
	static unsigned short CRC1021[256];
};

#endif