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

#define READBUFFER_SIZE 1024 * 512

#define NIB_TRACK_LENGTH 0x2000

#define BAM_OFFSET 4
#define BAM_ENTRY_SIZE 4

#define DIR_ENTRY_OFFSET_TYPE 0
#define DIR_ENTRY_OFFSET_NAME 3
#define DIR_ENTRY_OFFSET_BLOCKS 28

#define DIR_ENTRY_NAME_LENGTH 18-2

static const unsigned char HALF_TRACK_COUNT = 84;
static const unsigned short GCR_SYNC_LENGTH = 5;
static const unsigned short GCR_HEADER_LENGTH = 10;
static const unsigned short GCR_HEADER_GAP_LENGTH = 8;
static const unsigned short GCR_SECTOR_DATA_LENGTH = 325;
static const unsigned short GCR_SECTOR_GAP_LENGTH = 8;
static const unsigned short GCR_SECTOR_LENGTH = GCR_SYNC_LENGTH + GCR_HEADER_LENGTH + GCR_HEADER_GAP_LENGTH + GCR_SYNC_LENGTH + GCR_SECTOR_DATA_LENGTH + GCR_SECTOR_GAP_LENGTH;	//361

static const unsigned short G64_MAX_TRACK_LENGTH = 7928;

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
		RAW
	};

	DiskImage();

	bool OpenD64(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size);
	bool OpenG64(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size);
	bool OpenNIB(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size);
	bool OpenNBZ(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size);
	
	void Close();

	bool GetDecodedSector(u32 track, u32 sector, u8* buffer);

	inline bool GetNextBit(u32 track, u32 byte, u32 bit)
	{
		if (attachedImageSize == 0)
			return 0;

		return ((tracks[track][byte] >> bit) & 1) != 0;
	}

	inline void SetBit(u32 track, u32 byte, u32 bit, bool value)
	{
		if (attachedImageSize == 0)
			return;

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
	}

	static const unsigned char SectorsPerTrack[42];

	void DumpTrack(unsigned track);

	const char* GetName() { return fileInfo->fname; }

	inline unsigned BitsInTrack(unsigned track) const { return trackLengths[track] << 3; }

	static DiskType GetDiskImageTypeViaExtention(const char* diskImageName);
	static bool IsDiskImageExtention(const char* diskImageName);
	static bool IsLSTExtention(const char* diskImageName);

	bool GetReadOnly() const { return readOnly; }
	void SetReadOnly(bool readOnly) { this->readOnly = readOnly; }

	unsigned LastTrackUsed();

	static unsigned char readBuffer[READBUFFER_SIZE];

private:
	void CloseD64();
	void CloseG64();
	void CloseNIB();
	void CloseNBZ();

	bool WriteD64();
	bool WriteG64();
	bool WriteNIB();
	bool WriteNBZ();

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

	bool readOnly;
	bool dirty;
	unsigned attachedImageSize;
	DiskType diskType;
	const FILINFO* fileInfo;

	unsigned char tracks[HALF_TRACK_COUNT][NIB_TRACK_LENGTH];
	unsigned short trackLengths[HALF_TRACK_COUNT];
	unsigned char trackDensity[HALF_TRACK_COUNT];
	bool trackDirty[HALF_TRACK_COUNT];
	bool trackUsed[HALF_TRACK_COUNT];
};

#endif