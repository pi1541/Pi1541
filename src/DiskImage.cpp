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

// Pete Rittwage and Markus Brenner's code was heavly referenced and functions converted to CPP
// Used with Pete Rittwage's permission

#include "DiskImage.h"
#include "gcr.h"
#include "debug.h"
#include <string.h>
#include <ctype.h>
#include "lz.h"
#include "Petscii.h"
#include <malloc.h>
extern "C"
{
#include "rpi-gpio.h"
}

extern u32 HashBuffer(const void* pBuffer, u32 length);

#define MAX_DIRECTORY_SECTORS 18
#define DIRECTORY_SIZE 32
#define DISK_SECTOR_OFFSET_FIRST_DIRECTORY_SECTOR 357
#define DISKNAME_OFFSET_IN_DIR_BLOCK 144
#define DISKID_OFFSET_IN_DIR_BLOCK 162

#define DIRECTRY_ENTRY_FILE_TYPE_PRG 0x82

static u8 blankD64DIRBAM[] =
{
	0x12, 0x01, 0x41, 0x00, 0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f,
	0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f,
	0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f,
	0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f,
	0x15, 0xff, 0xff, 0x1f, 0x15, 0xff, 0xff, 0x1f, 0x11, 0xfc, 0xff, 0x07, 0x13, 0xff, 0xff, 0x07,
	0x13, 0xff, 0xff, 0x07, 0x13, 0xff, 0xff, 0x07, 0x13, 0xff, 0xff, 0x07, 0x13, 0xff, 0xff, 0x07,
	0x13, 0xff, 0xff, 0x07, 0x12, 0xff, 0xff, 0x03, 0x12, 0xff, 0xff, 0x03, 0x12, 0xff, 0xff, 0x03,
	0x12, 0xff, 0xff, 0x03, 0x12, 0xff, 0xff, 0x03, 0x12, 0xff, 0xff, 0x03, 0x11, 0xff, 0xff, 0x01,
	0x11, 0xff, 0xff, 0x01, 0x11, 0xff, 0xff, 0x01, 0x11, 0xff, 0xff, 0x01, 0x11, 0xff, 0xff, 0x01,
	0x42, 0x4c, 0x41, 0x4e, 0x4b, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0,
	0xa0, 0xa0, 0x31, 0x41, 0xa0, 0x32, 0x41, 0xa0, 0xa0, 0xa0, 0xa0, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	//	0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

unsigned char DiskImage::readBuffer[READBUFFER_SIZE];

static unsigned char compressionBuffer[HALF_TRACK_COUNT * MAX_TRACK_LENGTH];

static const unsigned short SECTOR_LENGTH = 256;
static const unsigned short SECTOR_LENGTH_WITH_CHECKSUM = 260;
static const unsigned char GCR_SYNC_BYTE = 0xff;
static const unsigned char GCR_GAP_BYTE = 0x55;
static const int SECTOR_HEADER_LENGTH = 8;
static const unsigned MAX_D64_SIZE = 0x30000;
static const unsigned MAX_D71_SIZE = 0x55600 + 1366;
static const unsigned MAX_D81_SIZE = 822400;

// CRC-16-CCITT
// CRC(x) = x^16 + x^12 + x^5 + x^0
unsigned short DiskImage::CRC1021[256] =
{
	0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,
	0x1231,0x0210,0x3273,0x2252,0x52b5,0x4294,0x72f7,0x62d6,0x9339,0x8318,0xb37b,0xa35a,0xd3bd,0xc39c,0xf3ff,0xe3de,
	0x2462,0x3443,0x0420,0x1401,0x64e6,0x74c7,0x44a4,0x5485,0xa56a,0xb54b,0x8528,0x9509,0xe5ee,0xf5cf,0xc5ac,0xd58d,
	0x3653,0x2672,0x1611,0x0630,0x76d7,0x66f6,0x5695,0x46b4,0xb75b,0xa77a,0x9719,0x8738,0xf7df,0xe7fe,0xd79d,0xc7bc,
	0x48c4,0x58e5,0x6886,0x78a7,0x0840,0x1861,0x2802,0x3823,0xc9cc,0xd9ed,0xe98e,0xf9af,0x8948,0x9969,0xa90a,0xb92b,
	0x5af5,0x4ad4,0x7ab7,0x6a96,0x1a71,0x0a50,0x3a33,0x2a12,0xdbfd,0xcbdc,0xfbbf,0xeb9e,0x9b79,0x8b58,0xbb3b,0xab1a,
	0x6ca6,0x7c87,0x4ce4,0x5cc5,0x2c22,0x3c03,0x0c60,0x1c41,0xedae,0xfd8f,0xcdec,0xddcd,0xad2a,0xbd0b,0x8d68,0x9d49,
	0x7e97,0x6eb6,0x5ed5,0x4ef4,0x3e13,0x2e32,0x1e51,0x0e70,0xff9f,0xefbe,0xdfdd,0xcffc,0xbf1b,0xaf3a,0x9f59,0x8f78,
	0x9188,0x81a9,0xb1ca,0xa1eb,0xd10c,0xc12d,0xf14e,0xe16f,0x1080,0x00a1,0x30c2,0x20e3,0x5004,0x4025,0x7046,0x6067,
	0x83b9,0x9398,0xa3fb,0xb3da,0xc33d,0xd31c,0xe37f,0xf35e,0x02b1,0x1290,0x22f3,0x32d2,0x4235,0x5214,0x6277,0x7256,
	0xb5ea,0xa5cb,0x95a8,0x8589,0xf56e,0xe54f,0xd52c,0xc50d,0x34e2,0x24c3,0x14a0,0x0481,0x7466,0x6447,0x5424,0x4405,
	0xa7db,0xb7fa,0x8799,0x97b8,0xe75f,0xf77e,0xc71d,0xd73c,0x26d3,0x36f2,0x0691,0x16b0,0x6657,0x7676,0x4615,0x5634,
	0xd94c,0xc96d,0xf90e,0xe92f,0x99c8,0x89e9,0xb98a,0xa9ab,0x5844,0x4865,0x7806,0x6827,0x18c0,0x08e1,0x3882,0x28a3,
	0xcb7d,0xdb5c,0xeb3f,0xfb1e,0x8bf9,0x9bd8,0xabbb,0xbb9a,0x4a75,0x5a54,0x6a37,0x7a16,0x0af1,0x1ad0,0x2ab3,0x3a92,
	0xfd2e,0xed0f,0xdd6c,0xcd4d,0xbdaa,0xad8b,0x9de8,0x8dc9,0x7c26,0x6c07,0x5c64,0x4c45,0x3ca2,0x2c83,0x1ce0,0x0cc1,
	0xef1f,0xff3e,0xcf5d,0xdf7c,0xaf9b,0xbfba,0x8fd9,0x9ff8,0x6e17,0x7e36,0x4e55,0x5e74,0x2e93,0x3eb2,0x0ed1,0x1ef0
};

void DiskImage::CRC(unsigned short& runningCRC, unsigned char data)
{
	runningCRC = CRC1021[(runningCRC >> 8) ^ data] ^ (runningCRC << 8);
}

void DiskImage::OutputD81HeaderByte(unsigned char*& dest, unsigned char data)
{
	*dest++ = data;
	//crc = CRC1021[(crc >> 8) ^ byte] ^ (crc << 8);
	CRC(crc, data);
}

void DiskImage::OutputD81DataByte(unsigned char*& src, unsigned char*& dest)
{
	unsigned char data;
	data = *src++;
	*dest++ = data;
	//crc = CRC1021[(crc >> 8) ^ data] ^ (crc << 8);
	CRC(crc, data);
}


#define NIB_HEADER_SIZE 0xFF

int gap_match_length = 7;	// Used by gcr.cpp

const unsigned char DiskImage::SectorsPerTrack[42] =
{
	21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, //  1 - 17
	19, 19, 19, 19, 19, 19, 19,				// 18 - 24
	18, 18, 18, 18, 18, 18,					// 25 - 30
	17, 17, 17, 17, 17, 17, 17, 17, 17, 17,	// 31 - 40
	17, 17									// 41 - 42
	// total 683-768 sectors
};

DiskImage::DiskImage()
	: readOnly(false)
	, dirty(false)
	, attachedImageSize(0)
	, fileInfo(0)
{
	memset(tracks, 0x55, sizeof(tracks));
}

void DiskImage::Close()
{
	switch (diskType)
	{
		case D64:
			CloseD64();
			memset(tracks, 0x55, sizeof(tracks));
		break;
		case G64:
			CloseG64();
			memset(tracks, 0x55, sizeof(tracks));
		break;
		case NIB:
			CloseNIB();
			memset(tracks, 0x55, sizeof(tracks));
		break;
		case NBZ:
			CloseNBZ();
			memset(tracks, 0x55, sizeof(tracks));
		break;
		case D71:
			CloseD71();
			memset(tracksD81, 0x55, sizeof(tracksD81));
		break;
		case D81:
			CloseD81();
			memset(tracksD81, 0, sizeof(tracksD81));
		break;
		case T64:
			CloseT64();
			memset(tracks, 0x55, sizeof(tracks));
		break;
		default:
			memset(tracks, 0x55, sizeof(tracks));
		break;
	}
	memset(trackLengths, 0, sizeof(trackLengths));
	diskType = NONE;
	fileInfo = 0;
	hash = 0;
}

void DiskImage::DumpTrack(unsigned track)
{

#if defined(EXPERIMENTALZERO)
	unsigned char* src = &tracks[track << 13];
#else
	unsigned char* src = tracks[track];
#endif
	unsigned trackLength = trackLengths[track];
	DEBUG_LOG("track = %d trackLength = %d\r\n", track, trackLength);
	for (unsigned index = 0; index < trackLength; ++index)
	{
		DEBUG_LOG("%d %02x\r\n", index, src[index]);
	}
}

bool DiskImage::OpenD64(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size)
{
	unsigned char errorinfo[MAXBLOCKSONDISK];
	unsigned last_track = MAXBLOCKSONDISK;
	unsigned sector_ref;
	unsigned char error;

	Close();

	this->fileInfo = fileInfo;

	unsigned offset = 0;

	if (size > MAX_D64_SIZE)
		size = MAX_D64_SIZE;

	attachedImageSize = size;

	memset(errorinfo, SECTOR_OK, sizeof(errorinfo));

	switch (size)
	{
		case (BLOCKSONDISK * 257):		// 35 track image with errorinfo
			memcpy(errorinfo, diskImage + (BLOCKSONDISK * 256), BLOCKSONDISK);
			/* FALLTHROUGH */
		case (BLOCKSONDISK * 256):		// 35 track image w/o errorinfo
			last_track = 35;
			break;

		case (MAXBLOCKSONDISK * 257):	// 40 track image with errorinfo
			memcpy(errorinfo, diskImage + (MAXBLOCKSONDISK * 256), MAXBLOCKSONDISK);
			/* FALLTHROUGH */
		case (MAXBLOCKSONDISK * 256):	// 40 track image w/o errorinfo
			last_track = 40;
			break;

		default:  // non-standard images, attempt to load anyway
			last_track = 40;
			break;
	}

	sector_ref = 0;
	for (unsigned halfTrackIndex = 0; halfTrackIndex < last_track * 2; ++halfTrackIndex)
	{
		unsigned char track = (halfTrackIndex >> 1);
#if defined(EXPERIMENTALZERO)
		unsigned char* dest = &tracks[halfTrackIndex << 13];
#else
		unsigned char* dest = tracks[halfTrackIndex];
#endif

		trackLengths[halfTrackIndex] = SectorsPerTrack[track] * GCR_SECTOR_LENGTH;

		if ((halfTrackIndex & 1) == 0)
		{
			if (offset < size)	// This will allow for >35 tracks.
			{
				trackUsed[halfTrackIndex] = true;
				//DEBUG_LOG("Track %d used\r\n", halfTrackIndex);
				for (unsigned sectorNo = 0; sectorNo < SectorsPerTrack[track]; ++sectorNo)
				{
					error = errorinfo[sector_ref++];

					convert_sector_to_GCR(diskImage + offset, dest, track + 1, sectorNo, diskImage + 0x165A2, error);
					dest += 361;

					offset += SECTOR_LENGTH;
				}
			}
			else
			{
				trackUsed[halfTrackIndex] = false;
				//DEBUG_LOG("Track %d not used\r\n", halfTrackIndex);
			}
		}
		else
		{
			trackUsed[halfTrackIndex] = false;
			//DEBUG_LOG("Track %d not used\r\n", halfTrackIndex);
		}
	}

	diskType = D64;
	return true;
}

bool DiskImage::WriteD64(char* name)
{
	if (readOnly)
		return true;

	FIL fp;
	FRESULT res = f_open(&fp, fileInfo ? fileInfo->fname : name, FA_CREATE_ALWAYS | FA_WRITE);
	if (res == FR_OK)
	{
		u32 bytesToWrite;
		u32 bytesWritten;

		int track, sector;
		BYTE id[3];
		BYTE d64data[MAXBLOCKSONDISK * 256], *d64ptr;
		int blocks_to_save = 0;

		DEBUG_LOG("Writing D64 file...\r\n");

		memset(d64data, 0, sizeof(d64data));

		if (!GetID(34, id))
		{
			DEBUG_LOG("Cannot find directory sector.\r\n");
			return false;
		}
		d64ptr = d64data;
		for (track = 0; track <= 40 * 2; track += 2)
		{
			if (trackUsed[track])
			{
				//printf("Track %d\n", track);

				for (sector = 0; sector < SectorsPerTrack[track / 2]; sector++)
				{
					ConvertSector(track, sector, d64ptr);
					d64ptr += 256;
					blocks_to_save++;
				}
			}
		}

		bytesToWrite = blocks_to_save * 256;
		SetACTLed(true);
		if (f_write(&fp, d64data, bytesToWrite, &bytesWritten) != FR_OK || bytesToWrite != bytesWritten)
		{
			SetACTLed(false);
			DEBUG_LOG("Cannot write d64 data.\r\n");
			f_close(&fp);
			return false;
		}

		f_close(&fp);

		//f_utime(fileInfo->fname, fileInfo);
		SetACTLed(false);

		DEBUG_LOG("Converted %d blocks into D64 file\r\n", blocks_to_save);

		return true;
	}
	else
	{
		DEBUG_LOG("Failed to open %s for write\r\n", fileInfo->fname);
		return false;
	}
}

void DiskImage::CloseD64()
{
	if (dirty)
	{
		WriteD64();
		dirty = false;
	}
	attachedImageSize = 0;
}

bool DiskImage::OpenD71(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size)
{
	Close();

	this->fileInfo = fileInfo;

	unsigned offset = 0;

	if (size > MAX_D71_SIZE)
		size = MAX_D71_SIZE;

	attachedImageSize = size;

	for (unsigned headIndex = 0; headIndex < 2; ++headIndex)
	{
		for (unsigned halfTrackIndex = 0; halfTrackIndex < D71_HALF_TRACK_COUNT; ++halfTrackIndex)
		{
			unsigned char track = (halfTrackIndex >> 1);
			unsigned char* dest = tracksD81[halfTrackIndex][headIndex];

			trackLengths[halfTrackIndex] = SectorsPerTrack[track] * GCR_SECTOR_LENGTH;

			if ((halfTrackIndex & 1) == 0)
			{
				if (offset < size)	// This will allow for >35 tracks.
				{
					trackUsed[halfTrackIndex] = true;
					//DEBUG_LOG("Track %d used\r\n", halfTrackIndex);
					for (unsigned sectorNo = 0; sectorNo < SectorsPerTrack[track]; ++sectorNo)
					{
						convert_sector_to_GCR(diskImage + offset, dest, track + 1, sectorNo, diskImage + 0x165A2, 0);
						dest += 361;

						offset += SECTOR_LENGTH;
					}
				}
				else
				{
					trackUsed[halfTrackIndex] = false;
					//DEBUG_LOG("Track %d not used\r\n", halfTrackIndex);
				}
			}
			else
			{
				trackUsed[halfTrackIndex] = false;
				//DEBUG_LOG("Track %d not used\r\n", halfTrackIndex);
			}
		}
	}

	diskType = D71;
	return true;
}

bool DiskImage::WriteD71()
{
	if (readOnly)
		return true;

	//FIL fp;
	//FRESULT res = f_open(&fp, fileInfo->fname, FA_CREATE_ALWAYS | FA_WRITE);
	//if (res == FR_OK)
	//{
	//	u32 bytesToWrite;
	//	u32 bytesWritten;

	//	int track, sector;
	//	BYTE id[3];
	//	BYTE d71data[MAXBLOCKSONDISK * 256 * 2], *d71ptr;
	//	int blocks_to_save = 0;

	//	DEBUG_LOG("Writing D71 file...\r\n");

	//	memset(d71data, 0, sizeof(d71data));

	//	if (!GetID(34, id, tracksD81[34][0]))
	//	{
	//		DEBUG_LOG("Cannot find directory sector.\r\n");
	//		return false;
	//	}
	//	d71ptr = d71data;
	//	//for (track = 0; track <= 40 * 2; track += 2)
	//	for (track = 0; track <= 35 * 2; track += 2)
	//	{
	//		if (trackUsed[track])
	//		{
	//			//printf("Track %d\n", track);

	//			for (sector = 0; sector < SectorsPerTrack[track / 2]; sector++)
	//			{
	//				ConvertSector(track, sector, d71ptr, tracksD81[track][0]);
	//				d71ptr += 256;
	//				blocks_to_save++;
	//			}
	//		}
	//	}

	//	bytesToWrite = blocks_to_save * 256;
	//	SetACTLed(true);
	//	if (f_write(&fp, d71data, bytesToWrite, &bytesWritten) != FR_OK || bytesToWrite != bytesWritten)
	//	{
	//		SetACTLed(false);
	//		DEBUG_LOG("Cannot write d71 data.\r\n");
	//		f_close(&fp);
	//		return false;
	//	}

	//	f_close(&fp);

	//	//f_utime(fileInfo->fname, fileInfo);
	//	SetACTLed(false);

	//	DEBUG_LOG("Converted %d blocks into D71 file\r\n", blocks_to_save);

	//	return true;
	//}
	//else
	{
		DEBUG_LOG("Failed to open %s for write\r\n", fileInfo->fname);
		return false;
	}
}

void DiskImage::CloseD71()
{
	if (dirty)
	{
		WriteD71();
		dirty = false;
	}
	attachedImageSize = 0;
}

bool DiskImage::OpenD81(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size)
{
	const unsigned physicalSectors = 10;
	unsigned char headIndex;
	unsigned headPos;

	Close();

	this->fileInfo = fileInfo;

	unsigned offsetSource = 0;

	if (size > MAX_D81_SIZE)
		size = MAX_D81_SIZE;

	attachedImageSize = size;

	unsigned char* src = diskImage;

	for (unsigned trackIndex = 0; trackIndex < D81_TRACK_COUNT; ++trackIndex)
	{
		unsigned offsetDest = 0;
		unsigned index;

		trackUsed[trackIndex] = true;
		memset(trackD81SyncBits[trackIndex][0], 0, MAX_TRACK_LENGTH >> 3);
		memset(trackD81SyncBits[trackIndex][1], 0, MAX_TRACK_LENGTH >> 3);
//32x	4e
// For 10 sectors
//		12x	00	// SYNC
//		*3x	a1
//		1x	fe	// ID
//		1x	TrackNo (0 indexed)
//		1x	Head (0 = 1 1 = 0)
//		1x	sector (1 indexed)
//		1x	02	(sector size)
//		1x	crc high byte
//		1x	crc low byte
//		22x	4e	(gap2)
//		For 2 loops (logical sectors to physical sectors)
//			if first loop
//				12x	00	// SYNC
//				*3x	a1
//				1x	fb	// Data mark
//			endif
//			256x	data (indexed sequentailly through D81)
//		end for
//		1x	crc high byte
//		1x	crc low byte
//		35x	4e	(gap3)
// end for

// 38/9/0 0x26(38) s9 h1 = 0x60000	27 11		- 27 16d
// 38/10/0 0x26(38) s10 h1 = 0x60200 27 13		- 27 18d
// 38/1/1 0x26(38) s1 h0 = 0x60400 27 15		- 27 20d
// 38/2/2 0x26(38) s2 h0 = 0x60600 27 17		- 27 22d
// 3, 4, 5, 6, 7, 8, 9, 10
// 37/6/0 0x25(37) s6 h1 = 0x5d200 26 0b		- 26 10d
// 37/7/0 0x25(37) s6 h1 = 0x5d400 26 0d		- 26 12d

// sectors 0-19 H1
// sectors 20-39 H0

// 4d800 20 01
// 4d900 20 02
// 4da00 20 03
// 4db00 20 04
// 4dc00 20 05
// 4dd00 20 06
// 4de00 20 07

// 50000 21 01
// 50100 21 02
// 50200 21 03
// 50300 21 04
// 50400 21 05
// 50500 21 06
// 50600 21 07
// 50700 21 08
// 50800 21 09
// 50900 21 0a
// 50a00 21 0b
// 50b00 21 0c
// 50c00 21 0d
// 50d00 21 0e
// 50e00 21 0f
// 50f00 21 10
// 51000 21 11
// 51100 21 12
// 51200 21 13
// 51300 21 14
// 51400 21 15
// 51500 21 16
// 51600 21 17
// 51700 21 18
// 51800 21 19
// 51900 21 1a
// 51a00 21 1b
// 51b00 21 1c
// 51c00 21 1d
// 51d00 21 1e
// 51e00 21 1f
// 51f00 21 20
// 52000 21 21
// 52100 21 22
// 52200 21 23
// 52300 21 24
// 52400 21 25
// 52500 21 26
// 52600 21 27
// 52700 20 00
// 52800 22 01
// 52900 22 02
// 52a00 22 03
// 52b00 22 04
// 52c00 22 05
// 52d00 22 06
// 52e00 22 07
// 52f00 22 08
// ..
// 54200 22 1b
// ..
// 54d00 22 26
// 54e00 22 27		- 22 38d
// 54f00 21 00		- 22 39d
// 55000 23 01

		unsigned int physicalSectorIndex;

		// (sectors 20 - 39 are on physical side 2)
		for (headIndex = 0; headIndex < 2; ++headIndex)
		{
			unsigned char* dest = tracksD81[trackIndex][headIndex];
			memset(dest, 0x4e, 32); dest += 32;
			for (physicalSectorIndex = 0; physicalSectorIndex < physicalSectors; ++physicalSectorIndex)
			{
				// If a sequence of zeros followed by a sequence of three Sync Bytes is found, then the PLL(phase locked loop) and data separator are synchronized and data bytes can be read.

				memset(dest, 0, 12); dest += 12;	// SYNC - This sequence provides to the DPLL enough time to adjust the frequency and center the inspection window.

				headPos = dest - tracksD81[trackIndex][headIndex];
				SetD81SyncBit(trackIndex, headIndex, headPos++, true);
				SetD81SyncBit(trackIndex, headIndex, headPos++, true);
				SetD81SyncBit(trackIndex, headIndex, headPos++, true);

				// The CRC includes all information starting with the address mark and up to the CRC characters.
				// The CRC Register is preset to ones.
				crc = 0xffff;

				OutputD81HeaderByte(dest, 0xa1);	// Special bytes are encoded that violates the MFM encoding rules with a missing clock in one of the sequential zero bits.
				OutputD81HeaderByte(dest, 0xa1);
				OutputD81HeaderByte(dest, 0xa1);
				OutputD81HeaderByte(dest, 0xfe);	// Header ID
				OutputD81HeaderByte(dest, (unsigned char)trackIndex);	// 0 indexed
				OutputD81HeaderByte(dest, headIndex);
				OutputD81HeaderByte(dest, (unsigned char)physicalSectorIndex + 1);	// 1 indexed
				OutputD81HeaderByte(dest, 2);		// sector length code (0=128, 1=256, 2=512, 3=1024)
				*dest++ = (unsigned char)(crc >> 8);
				*dest++ = (unsigned char)(crc & 0xff);
				memset(dest, 0x4e, 22); dest += 22;

				memset(dest, 0, 12); dest += 12;	// SYNC

				headPos = dest - tracksD81[trackIndex][headIndex];
				SetD81SyncBit(trackIndex, headIndex, headPos++, true);
				SetD81SyncBit(trackIndex, headIndex, headPos++, true);
				SetD81SyncBit(trackIndex, headIndex, headPos++, true);

				// The CRC Register is preset to ones.
				crc = 0xffff;
				OutputD81HeaderByte(dest, 0xa1);
				OutputD81HeaderByte(dest, 0xa1);
				OutputD81HeaderByte(dest, 0xa1);
				OutputD81HeaderByte(dest, 0xfb);		// Data ID

				for (index = 0; index < D81_SECTOR_LENGTH; ++index)
				{
					OutputD81DataByte(src, dest);
				}

				*dest++ = (unsigned char)(crc >> 8);
				*dest++ = (unsigned char)(crc & 0xff);

				memset(dest, 0x4e, 35); dest += 35;
			}

			trackLengths[trackIndex] = dest - tracksD81[trackIndex][headIndex];
		}
	}

	diskType = D81;
	return true;
}

bool DiskImage::WriteD81()
{
	const unsigned physicalSectors = 10;

	if (readOnly)
		return true;

	FIL fp;
	FRESULT res = f_open(&fp, fileInfo->fname, FA_CREATE_ALWAYS | FA_WRITE);
	if (res == FR_OK)
	{
		u32 bytesToWrite;
		u32 bytesWritten;

		for (unsigned trackIndex = 0; trackIndex < D81_TRACK_COUNT; ++trackIndex)
		{
			unsigned offsetDest = 0;
			unsigned index;

			if (trackLengths[trackIndex] != 0 && trackUsed[trackIndex])
			{
				unsigned int physicalSectorIndex;

				// (sectors 20 - 39 are on physical side 2)
				for (unsigned headIndex = 0; headIndex < 2; ++headIndex)
				{
					unsigned char* src = tracksD81[trackIndex][headIndex];
					src += 32;
					for (physicalSectorIndex = 0; physicalSectorIndex < physicalSectors; ++physicalSectorIndex)
					{
						// If a sequence of zeros followed by a sequence of three Sync Bytes is found, then the PLL(phase locked loop) and data separator are synchronized and data bytes can be read.

						src += 12;	// 12x00 SYNC - This sequence provides to the DPLL enough time to adjust the frequency and center the inspection window.
						src += 3;	// 3xA1
						src += 1;	// 1xFE	header ID
						src += 1;	// 1x track index
						src += 1;	// 1x head index
						src += 1;	// 1x physical sector index
						src += 1;	// 1x sector length code
						src += 1;	// 1x crc high
						src += 1;	// 1x crc low
						src += 22;	// 22x4e

						src += 12;	// 12x00 SYNC
						src += 3;	// 3xA1
						src += 1;	// 1xFB	header ID

						SetACTLed(true);
						bytesToWrite = D81_SECTOR_LENGTH;
						if (f_write(&fp, src, bytesToWrite, &bytesWritten) != FR_OK || bytesToWrite != bytesWritten)
						{
							SetACTLed(false);
							f_close(&fp);
							return false;
						}
						src += D81_SECTOR_LENGTH;
						SetACTLed(false);

						src += 1;	// 1x crc high
						src += 1;	// 1x crc low
						src += 35;	// 35x4e
					}
				}
			}
			else
			{
				const unsigned trackLength = physicalSectors * 2 * D81_SECTOR_LENGTH;

				SetACTLed(true);
				for (index = 0; index < trackLength; ++index)
				{
					unsigned char zero = 0;
					bytesToWrite = 1;
					if (f_write(&fp, &zero, bytesToWrite, &bytesWritten) != FR_OK || bytesToWrite != bytesWritten)
					{
						SetACTLed(false);
						f_close(&fp);
						return false;
					}
				}
				SetACTLed(false);
			}
		}

		f_close(&fp);

		//f_utime(fileInfo->fname, fileInfo);
		SetACTLed(false);

		return true;
	}
	else
	{
		DEBUG_LOG("Failed to open %s for write\r\n", fileInfo->fname);
		return false;
	}
}

void DiskImage::CloseD81()
{
	if (dirty)
	{
		WriteD81();
		dirty = false;
	}
	attachedImageSize = 0;
}

bool DiskImage::OpenG64(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size)
{
	Close();

	this->fileInfo = fileInfo;

	attachedImageSize = size;

	if (memcmp(diskImage, "GCR-1541", 8) == 0)
	{
		hash = HashBuffer(diskImage, size);

		//DEBUG_LOG("Is G64 %08x\r\n", hash);

		unsigned char numTracks = diskImage[9];
		//DEBUG_LOG("numTracks = %d\r\n", numTracks);

		unsigned char* data = diskImage + 12;
		unsigned char* speedZoneData = diskImage + 0x15c;
		unsigned short trackLength = 0;

		unsigned track;

		for (track = 0; track < numTracks; ++track)
		{
			unsigned offset = *(unsigned*)data;
			data += 4;

			//DEBUG_LOG("Track = %d Offset = %x\r\n", track, offset);

			trackDensity[track] = *(unsigned*)(speedZoneData + track * 4);

			if (offset == 0)
			{
				trackLengths[track] = capacity_max[trackDensity[track]];
				trackUsed[track] = false;
			}
			else
			{
				unsigned char* trackData = diskImage + offset;

				trackLength = *(unsigned short*)(trackData);
				//DEBUG_LOG("trackLength = %d offset = %d\r\n", trackLength, offset);
				trackData += 2;
				trackLengths[track] = trackLength;
#if defined(EXPERIMENTALZERO)
				memcpy(&tracks[track << 13], trackData, trackLength);
#else
				memcpy(tracks[track], trackData, trackLength);
#endif
				trackUsed[track] = true;
				//DEBUG_LOG("%d has data\r\n", track);
			}
		}

		diskType = G64;
		return true;
	}
	return false;
}

static bool WriteDwords(FIL* fp, u32* values, u32 amount)
{
	u32 index;
	u32 bytesToWrite = 4;
	u32 bytesWritten;

	for (index = 0; index < amount; ++index)
	{
		if (f_write(fp, &values[index], bytesToWrite, &bytesWritten) != FR_OK || bytesToWrite != bytesWritten)
			return false;
	}
	return true;
}

bool DiskImage::WriteG64(char* name)
{
	if (readOnly)
		return true;

	FIL fp;
	FRESULT res = f_open(&fp, fileInfo ? fileInfo->fname : name, FA_CREATE_ALWAYS | FA_WRITE);
	if (res == FR_OK)
	{
		u32 bytesToWrite;
		u32 bytesWritten;
		int track_inc = 1;

		BYTE header[12];
		DWORD gcr_track_p[MAX_HALFTRACKS_1541] = { 0 };
		DWORD gcr_speed_p[MAX_HALFTRACKS_1541] = { 0 };
		BYTE gcr_track[MAX_TRACK_LENGTH + 2];
		size_t track_len;
		int index = 0, track;
		BYTE buffer[MAX_TRACK_LENGTH], tempfillbyte;

		DEBUG_LOG("Writing G64 file...\r\n");
		//DEBUG_LOG("G64 Track Length = %d", G64_TRACK_MAXLEN);

		strcpy((char *)header, "GCR-1541");
		header[8] = 0;
		header[9] = MAX_HALFTRACKS_1541;
		header[10] = (BYTE)(G64_TRACK_MAXLEN % 256);
		header[11] = (BYTE)(G64_TRACK_MAXLEN / 256);

		bytesToWrite = sizeof(header);
		SetACTLed(true);
		if (f_write(&fp, header, bytesToWrite, &bytesWritten) != FR_OK || bytesToWrite != bytesWritten)
		{
			SetACTLed(false);
			DEBUG_LOG("Cannot write G64 header.\r\n");
			f_close(&fp);
			return false;
		}
		SetACTLed(false);

		for (track = 0; track < MAX_HALFTRACKS_1541; track += track_inc)
		{
			if (trackLengths[track] == 0 || !trackUsed[track])
			{
				gcr_track_p[track] = 0;
				gcr_speed_p[track] = 0;
			}
			else
			{
				gcr_track_p[track] = 0xc + (MAX_TRACKS_1541 * 16) + (index++ * (G64_TRACK_MAXLEN + 2));
				gcr_speed_p[track] = trackDensity[track] & 3;
			}
		}

		SetACTLed(true);
		WriteDwords(&fp, (u32*)gcr_track_p, MAX_HALFTRACKS_1541);
		WriteDwords(&fp, (u32*)gcr_speed_p, MAX_HALFTRACKS_1541);
		SetACTLed(false);

		for (track = 0; track < MAX_HALFTRACKS_1541; track += track_inc)
		{
			track_len = trackLengths[track];
			if (track_len>G64_TRACK_MAXLEN) track_len = G64_TRACK_MAXLEN;

			if (!track_len || !trackUsed[track]) continue;

			tempfillbyte = 0x55;

			memset(&gcr_track[2], tempfillbyte, G64_TRACK_MAXLEN);

			gcr_track[0] = (BYTE)(track_len % 256);
			gcr_track[1] = (BYTE)(track_len / 256);
#if defined(EXPERIMENTALZERO)
			memcpy(buffer, &tracks[track << 13], track_len);
#else
			memcpy(buffer, tracks[track], track_len);
#endif

			memcpy(gcr_track + 2, buffer, track_len);
			bytesToWrite = G64_TRACK_MAXLEN + 2;
			SetACTLed(true);
			if (f_write(&fp, gcr_track, bytesToWrite, &bytesWritten) != FR_OK || bytesToWrite != bytesWritten)
			{
				SetACTLed(false);
				DEBUG_LOG("Cannot write track data.\r\n");
				f_close(&fp);
				return false;
			}
			SetACTLed(false);
		}

		f_close(&fp);
		DEBUG_LOG("nSuccessfully saved G64\r\n");

		return true;
	}
	else
	{
		DEBUG_LOG("Failed to open %s for write\r\n", fileInfo->fname);
		return false;
	}
}

void DiskImage::CloseG64()
{
	if (dirty)
	{
		WriteG64();

		dirty = false;
	}
	attachedImageSize = 0;
}

bool DiskImage::OpenNIB(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size)
{
	int track, t_index = 0, h_index = 0;
	Close();

	this->fileInfo = fileInfo;

	attachedImageSize = size;

	if (memcmp(diskImage, "MNIB-1541-RAW", 13) == 0)
	{

		for (track = 0; track < (MAX_TRACKS_1541 * 2); ++track)
		{
			trackLengths[track] = capacity_max[trackDensity[track]];
			trackUsed[track] = false;
		}

		while (diskImage[0x10 + h_index])
		{
			track = diskImage[0x10 + h_index] - 2;
			unsigned char v = diskImage[0x11 + h_index];
			trackDensity[track] = (v & 0x03);

			DEBUG_LOG("Converting NIB track %d (%d.%d)\r\n", track, track >> 1, track & 1 ? 5 : 0);

			unsigned char* nibdata = diskImage + (t_index * NIB_TRACK_LENGTH) + 0x100;
			int align;
#if defined(EXPERIMENTALZERO)
			trackLengths[track] = extract_GCR_track(&tracks[track << 13], nibdata, &align
				//, ALIGN_GAP
				, ALIGN_NONE
				, capacity_min[trackDensity[track]],
				capacity_max[trackDensity[track]]);
#else
			trackLengths[track] = extract_GCR_track(tracks[track], nibdata, &align
				//, ALIGN_GAP
				, ALIGN_NONE
				, capacity_min[trackDensity[track]],
				capacity_max[trackDensity[track]]);
#endif

			trackUsed[track] = true;

			h_index += 2;
			t_index++;
		}


		DEBUG_LOG("Successfully parsed NIB data for %d tracks\n", t_index);
		diskType = NIB;
		return true;
	}
	return false;
}

bool DiskImage::WriteNIB()
{
	if (readOnly)
		return true;

	FIL fp;
	FRESULT res = f_open(&fp, fileInfo->fname, FA_CREATE_ALWAYS | FA_WRITE);
	if (res == FR_OK)
	{
		u32 bytesToWrite;
		u32 bytesWritten;

		int track;
		char header[0x100];
		int header_entry = 0;

		DEBUG_LOG("Converting to NIB format...\n");

		memset(header, 0, sizeof(header));

		sprintf(header, "MNIB-1541-RAW%c%c%c", 1, 0, 0);

		for (track = 0; track < (MAX_TRACKS_1541 * 2); ++track)
		{
			if (trackUsed[track])
			{
				header[0x10 + (header_entry * 2)] = (BYTE)track + 2;
				header[0x10 + (header_entry * 2) + 1] = trackDensity[track];

				header_entry++;
			}
		}

		bytesToWrite = sizeof(header);
		SetACTLed(true);
		if (f_write(&fp, header, bytesToWrite, &bytesWritten) != FR_OK || bytesToWrite != bytesWritten)
		{
			DEBUG_LOG("Cannot write track data.\r\n");
		}
		else
		{
			bytesToWrite = NIB_TRACK_LENGTH;
			for (track = 0; track < HALF_TRACK_COUNT; ++track)
			{
				if (trackUsed[track])
				{
#if defined(EXPERIMENTALZERO)
					if (f_write(&fp, &tracks[track << 13], bytesToWrite, &bytesWritten) != FR_OK || bytesToWrite != bytesWritten)
#else
					if (f_write(&fp, tracks[track], bytesToWrite, &bytesWritten) != FR_OK || bytesToWrite != bytesWritten)
#endif
					{
						DEBUG_LOG("Cannot write track data.\r\n");
					}
				}
			}
		}
		SetACTLed(false);

		f_close(&fp);

		DEBUG_LOG("nSuccessfully saved NIB\r\n");

		return true;
	}
	else
	{
		DEBUG_LOG("Failed to open %s for write\r\n", fileInfo->fname);
		return false;
	}
}

void DiskImage::CloseNIB()
{
	if (dirty)
	{
		WriteNIB();

		dirty = false;
	}
	attachedImageSize = 0;
}

bool DiskImage::OpenNBZ(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size)
{
	Close();

	if ((size = LZ_Uncompress(diskImage, compressionBuffer, size)))
	{
		if (OpenNIB(fileInfo, compressionBuffer, size))
		{
			diskType = NIB;
			return true;
		}
	}
	return false;
}

bool DiskImage::WriteNBZ()
{
	bool success = false;

	if (readOnly)
		return true;

	SetACTLed(true);
	if (WriteNIB())
	{
		FIL fp;
		FRESULT res = f_open(&fp, fileInfo->fname, FA_READ);
		if (res == FR_OK)
		{
			u32 bytesRead;
			f_read(&fp, readBuffer, READBUFFER_SIZE, &bytesRead);
			f_close(&fp);
			DEBUG_LOG("Reloaded %s - %d for compression\r\n", fileInfo->fname, bytesRead);
			bytesRead = LZ_Compress(readBuffer, compressionBuffer, bytesRead);

			if (bytesRead)
			{
				res = f_open(&fp, fileInfo->fname, FA_CREATE_ALWAYS | FA_WRITE);
				if (res == FR_OK)
				{
					u32 bytesToWrite = bytesRead;
					u32 bytesWritten;

					if (f_write(&fp, compressionBuffer, bytesToWrite, &bytesWritten) != FR_OK || bytesToWrite != bytesWritten)
					{
						DEBUG_LOG("Cannot write NBZ data.\r\n");
					}
					else
					{
						success = true;
					}
					f_close(&fp);
				}
			}
		}
	}
	SetACTLed(false);
	return success;
}

void DiskImage::CloseNBZ()
{
	if (dirty)
	{
		WriteNBZ();

		dirty = false;
	}
	attachedImageSize = 0;
}

bool DiskImage::OpenT64(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size)
{
	bool success = false;

	Close();

	this->fileInfo = fileInfo;

	attachedImageSize = size;

	if ((memcmp(diskImage, "C64 tape image file", 20) == 0) || (memcmp(diskImage, "C64s tape image file", 21) == 0))
	{
		u16 version = diskImage[0x20] | (diskImage[0x21] << 8);
		u16 entries = diskImage[0x22] | (diskImage[0x23] << 8);
		u16 entriesUsed = diskImage[0x24] | (diskImage[0x25] << 8);
		char name[25] = { 0 };
		for (int i = 0; i < 24; ++i)
		{
			name[i] = tolower(diskImage[i + 0x28]);
		}

		unsigned char* newDiskImage = (unsigned char*)malloc(READBUFFER_SIZE);

		if (newDiskImage)
		{
			unsigned length = DiskImage::CreateNewDiskInRAM(name, "00", newDiskImage);

			if (length)
			{

				u16 entryIndex;

				for (entryIndex = 0; entryIndex < entriesUsed; ++entryIndex)
				{
					char nameEntry[17] = { 0 };
					int offset = 0x40 + entryIndex * 32;
					u8 type = diskImage[offset];
					u8 fileType = diskImage[offset + 1];
					if (fileType == DIRECTRY_ENTRY_FILE_TYPE_PRG)
					{
						u16 startAddress = diskImage[offset + 2] | (diskImage[offset + 3] << 8);
						u16 endAddress = diskImage[offset + 4] | (diskImage[offset + 5] << 8);
						unsigned length = endAddress - startAddress;

						u32 fileOffset = diskImage[offset + 8] | (diskImage[offset + 9] << 8) | (diskImage[offset + 10] << 16) | (diskImage[offset + 11] << 24);
						strncpy(nameEntry, (const char*)(diskImage + offset + 0x10), 16);

						unsigned char* data = (unsigned char*)malloc(length + 2);

						if (data)
						{
							data[0] = startAddress & 0xff;
							data[1] = (startAddress >> 8) & 0xff;

							memcpy(data + 2, diskImage + fileOffset, length);
							length += 2;
							bool addFileSuccess = AddFileToRAMD64(newDiskImage, nameEntry, data, length);

							free(data);
						}
					}

				}

				if (OpenD64(fileInfo, newDiskImage, length))
				{
					success = true;
					diskType = T64;
				}
			}

			free(newDiskImage);
		}
	}

	return success;
}

bool DiskImage::WriteT64(char* name)
{
	if (readOnly)
		return true;

	return true;
}

void DiskImage::CloseT64()
{
	if (dirty)
	{
		WriteT64();
		dirty = false;
	}
	attachedImageSize = 0;
}

bool DiskImage::OpenPRG(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size)
{
	bool success = false;

	Close();

	this->fileInfo = fileInfo;

	attachedImageSize = size;

	unsigned char* newDiskImage = (unsigned char*)malloc(READBUFFER_SIZE);

	if (newDiskImage)
	{
		unsigned length = DiskImage::CreateNewDiskInRAM(fileInfo->fname, "00", newDiskImage);

		if (length)
		{
			bool addFileSuccess = AddFileToRAMD64(newDiskImage, fileInfo->fname, diskImage, size);

			if (addFileSuccess && OpenD64(fileInfo, newDiskImage, length))
			{
				success = true;
				DEBUG_LOG("Success\r\n");
				diskType = PRG;
			}
		}

		free(newDiskImage);
	}

	return success;
}

bool DiskImage::GetDecodedSector(u32 track, u32 sector, u8* buffer)
{
	if (track > 0)
	{
		track = (track - 1) * 2;
		if (trackUsed[track])
			return ConvertSector(track, sector, buffer);
	}

	return false;
}

DiskImage::DiskType DiskImage::GetDiskImageTypeViaExtention(const char* diskImageName)
{
	char* ext = strrchr((char*)diskImageName, '.');

	if (ext)
	{
		if (toupper((char)ext[1]) == 'G' && ext[2] == '6' && ext[3] == '4')
			return G64;
		else if (toupper((char)ext[1]) == 'N' && toupper((char)ext[2]) == 'I' && toupper((char)ext[3]) == 'B')
			return NIB;
		else if (toupper((char)ext[1]) == 'N' && toupper((char)ext[2]) == 'B' && toupper((char)ext[3]) == 'Z')
			return NBZ;
		else if (toupper((char)ext[1]) == 'D' && ext[2] == '6' && ext[3] == '4')
			return D64;
		else if (toupper((char)ext[1]) == 'T' && ext[2] == '6' && ext[3] == '4')
			return T64;
		else if (IsLSTExtention(diskImageName))
			return LST;
		else if (toupper((char)ext[1]) == 'D' && ext[2] == '8' && ext[3] == '1')
			return D81;
		else if (toupper((char)ext[1]) == 'P' && toupper((char)ext[2]) == 'R' && toupper((char)ext[3]) == 'G')
			return PRG;
	}
	return NONE;
}

bool DiskImage::IsDiskImageExtention(const char* diskImageName)
{
	return GetDiskImageTypeViaExtention(diskImageName) != NONE;
}

bool DiskImage::IsDiskImageD81Extention(const char* diskImageName)
{
	char* ext = strrchr((char*)diskImageName, '.');

	if (ext)
	{
		if (toupper((char)ext[1]) == 'D' && ext[2] == '8' && ext[3] == '1')
			return true;
	}
	return false;
}

bool DiskImage::IsDiskImageD71Extention(const char* diskImageName)
{
	char* ext = strrchr((char*)diskImageName, '.');

	if (ext)
	{
		if (toupper((char)ext[1]) == 'D' && ext[2] == '7' && ext[3] == '1')
			return true;
	}
	return false;
}

bool DiskImage::IsLSTExtention(const char* diskImageName)
{
	char* ext = strrchr((char*)diskImageName, '.');

	if (ext && 
			(
				(toupper((char)ext[1]) == 'L' && toupper((char)ext[2]) == 'S' && toupper((char)ext[3]) == 'T')
				||
				(toupper((char)ext[1]) == 'V' && toupper((char)ext[2]) == 'F' && toupper((char)ext[3]) == 'L')
			)
		)

		return true;
	return false;
}

bool DiskImage::ConvertSector(unsigned track, unsigned sector, unsigned char* data)
{
	unsigned char buffer[SECTOR_LENGTH_WITH_CHECKSUM];
	unsigned char checkSum;
	int index;
	int bitIndex;

	bitIndex = FindSectorHeader(track, sector, 0);
	if (bitIndex < 0)
		return false;

	bitIndex = FindSync(track, bitIndex, (SECTOR_LENGTH_WITH_CHECKSUM * 2) * 8);
	if (bitIndex < 0)
		return false;

	DecodeBlock(track, bitIndex, buffer, SECTOR_LENGTH_WITH_CHECKSUM / 4);

	checkSum = buffer[257];
	for (index = 0; index < SECTOR_LENGTH; ++index)
	{
		data[index] = buffer[index + 1];
		checkSum ^= data[index];
	}

	if (buffer[0] != 0x07)
		return false;			// No data block

	return checkSum == 0;
}

void DiskImage::DecodeBlock(unsigned track, int bitIndex, unsigned char* buf, int num)
{
	int shift, i, j;
	unsigned char gcr[5];
	unsigned char byte;
	unsigned char* offset;
#if defined(EXPERIMENTALZERO)
	unsigned char* end = &tracks[track << 13] + trackLengths[track];
#else
	unsigned char* end = tracks[track] + trackLengths[track];
#endif

	shift = bitIndex & 7;
#if defined(EXPERIMENTALZERO)
	offset = &tracks[track << 13] + (bitIndex >> 3);
#else
	offset = tracks[track] + (bitIndex >> 3);
#endif

	byte = offset[0] << shift;
	for (i = 0; i < num; i++, buf += 4)
	{
		for (j = 0; j < 5; j++)
		{
			offset++;
			if (offset >= end)
#if defined(EXPERIMENTALZERO)
				offset = &tracks[track << 13];
#else
				offset = tracks[track];
#endif
		
			if (shift)
			{
				gcr[j] = byte | ((offset[0] << shift) >> 8);
				byte = offset[0] << shift;
			}
			else 
			{
				gcr[j] = byte;
				byte = offset[0];
			}
		}
		convert_4bytes_from_GCR(gcr, buf);
	}
}

int DiskImage::FindSync(unsigned track, int bitIndex, int maxBits, int* syncStartIndex)
{
	int readShiftRegister = 0;
#if defined(EXPERIMENTALZERO)
	unsigned char byte = tracks[(track << 13) + (bitIndex >> 3)] << (bitIndex & 7);
#else
	unsigned char byte = tracks[track][bitIndex >> 3] << (bitIndex & 7);
#endif
	bool prevBitZero = true;

	while (maxBits--)
	{
		if (byte & 0x80)
		{
			if (syncStartIndex && prevBitZero)
				*syncStartIndex = bitIndex;

			prevBitZero = false;
			readShiftRegister = (readShiftRegister << 1) | 1;
		}
		else
		{
			prevBitZero = true;

			if (~readShiftRegister & 0x3ff)
				readShiftRegister <<= 1;
			else
				return bitIndex;
		}
		if (~bitIndex & 7)
		{
			bitIndex++;
			byte <<= 1;
		}
		else
		{
			bitIndex++;
			if (bitIndex >= MAX_TRACK_LENGTH * 8)
				bitIndex = 0;
#if defined(EXPERIMENTALZERO)
			byte = tracks[(track << 13)+(bitIndex >> 3)];
#else
			byte = tracks[track][bitIndex >> 3];
#endif
		}
	}
	return -1;
}

int DiskImage::FindSectorHeader(unsigned track, unsigned sector, unsigned char* id)
{
	unsigned char header[10];
	int bitIndex;
	int bitIndexPrev;

	bitIndex = 0;
	bitIndexPrev = -1;
	for (;;)
	{
		bitIndex = FindSync(track, bitIndex, NIB_TRACK_LENGTH * 8);
		if (bitIndexPrev == bitIndex)
			break;
		if (bitIndexPrev < 0)
			bitIndexPrev = bitIndex;
		DecodeBlock(track, bitIndex, header, 2);

		if (header[0] == 0x08 && header[2] == sector)
		{
			if (id)
			{
				id[0] = header[5];
				id[1] = header[4];
			}
			return bitIndex;
		}
	}
	return -1;
}

unsigned DiskImage::GetID(unsigned track, unsigned char* id)
{
	if (FindSectorHeader(track, 0, id) >= 0)
		return 1;
	return 0;
}

unsigned DiskImage::LastTrackUsed()
{
	unsigned i;
	unsigned lastTrackUsed = 0;

	for (i = 0; i < HALF_TRACK_COUNT; ++i)
	{
		if (trackUsed[i])
			lastTrackUsed = i;
	}
	return lastTrackUsed;
}

unsigned DiskImage::CreateNewDiskInRAM(const char* filenameNew, const char* ID, unsigned char* destBuffer)
{
	unsigned char* dest;
	unsigned char* ptr;
	int i;

	DEBUG_LOG("CreateNewDiskInRAM %s\r\n", filenameNew);

	unsigned char buffer[256];
	u32 bytes;
	u32 blocks;

	if (destBuffer == 0)
		destBuffer = DiskImage::readBuffer;

	dest = destBuffer;

	memset(buffer, 0, sizeof(buffer));

	for (blocks = 0; blocks < DISK_SECTOR_OFFSET_FIRST_DIRECTORY_SECTOR; ++blocks)
	{
		for (i = 0; i < 256; ++i)
		{
			*dest++ = buffer[i];
		}
	}
	ptr = (unsigned char*)&blankD64DIRBAM[DISKNAME_OFFSET_IN_DIR_BLOCK];
	int len = strlen(filenameNew);
	for (i = 0; i < len; ++i)
	{
		*ptr++ = ascii2petscii(filenameNew[i]);
	}
	for (; i < 18; ++i)
	{
		*ptr++ = 0xa0;
	}
	for (i = 0; i < 2; ++i)
	{
		*ptr++ = ascii2petscii(ID[i]);
	}
	for (i = 0; i < 256; ++i)
	{
		*dest++ = blankD64DIRBAM[i];
	}
	buffer[1] = 0xff;
	for (i = 0; i < 256; ++i)
	{
		*dest++ = buffer[i];
	}
	buffer[1] = 0;
	for (blocks = 0; blocks < 324; ++blocks)
	{
		for (i = 0; i < 256; ++i)
		{
			*dest++ = buffer[i];
		}
	}

	return (unsigned)(dest - destBuffer);
}

int DiskImage::RAMD64GetSectorOffset(int track, int sector)
{
	int index;
	int sectorOffset = 0;

	for (index = 0; index < (track - 1); ++index)
	{
		sectorOffset += SectorsPerTrack[index];
	}
	sectorOffset += sector;
	return sectorOffset;
}

unsigned char* DiskImage::RAMD64AddDirectoryEntry(unsigned char* ramD64, const char* name, const unsigned char* data, unsigned length)
{
	unsigned char* ptrTrackSector = 0;
	unsigned char* ptr;
	int directorySector;
	int track = 18;
	int i;
	int numberOfBlocks = length / (256 - 2) + ((length % (256 - 2)) ? 1 : 0);

	int sectorOffset = RAMD64GetSectorOffset(18, 0);
	ptr = ramD64 + sectorOffset * 256;

	track = ptr[0];

	for (directorySector = 1; directorySector < 19; ++directorySector)
	{
		int entryIndex;
		bool found = false;

		sectorOffset = RAMD64GetSectorOffset(track, directorySector);

		DEBUG_LOG("directory sector offset = %d\r\n", sectorOffset);
		ptr = ramD64 + sectorOffset * 256;

		entryIndex = 0;
		while (!found && entryIndex < 8)
		{
			unsigned char* ptrEntry = ptr + 2 + entryIndex * DIRECTORY_SIZE;

			if (*ptrEntry == 0)
			{
				*ptrEntry++ = DIRECTRY_ENTRY_FILE_TYPE_PRG;

				ptrTrackSector = ptrEntry;

				ptrEntry++; // track data start
				ptrEntry++; // sector data start

				int len = strlen(name);
				len = Min(len, 16);

				for (i = 0; i < len; ++i)
				{
					*ptrEntry++ = ascii2petscii(tolower(name[i]));
				}
				for (; i < 16; ++i)
				{
					*ptrEntry++ = 0xa0;
				}

				ptrEntry++;	// track and sector of the first side sector block
				ptrEntry++;

				ptrEntry++;	// record length

				ptrEntry++;	// not used
				ptrEntry++;
				ptrEntry++;
				ptrEntry++;

				ptrEntry++;	// Track and sector of the new file when overwrtiting with the at sign
				ptrEntry++;
				
				*ptrEntry++ = (unsigned char)(numberOfBlocks & 0xff);
				*ptrEntry++ = (unsigned char)(numberOfBlocks >> 8);
				ptrEntry++;	// not used
				ptrEntry++;
				found = true;
			}
			else
			{
				++entryIndex;
			}
		}

		if (entryIndex < 8)
		{
			ptr[0] = 0;
			ptr[1] = 0xff;

			if (directorySector > 1)
			{
				unsigned char* ptrPrevSector = ramD64 + RAMD64GetSectorOffset(track, directorySector - 1) * 256;
				
				ptrPrevSector[0] = track;
				ptrPrevSector[1] = directorySector - 1;
				// Allocate directory sector in the BAM
				RAMD64AllocateSector(ramD64, track, directorySector);
			}
			break;
		}
	}

	return ptrTrackSector;
}

int DiskImage::RAMD64FreeSectors(unsigned char* ramD64)
{
	int freeSectors = 0;
	unsigned char* ptr;
	int sectorOffset = RAMD64GetSectorOffset(18, 0);

	DEBUG_LOG("sectorOffset bam = %d\r\n", sectorOffset);

	ptr = ramD64 + sectorOffset * 256;

	ptr += 4;

	int trackIndex;
	for (trackIndex = 0; trackIndex < 35; ++trackIndex)
	{
		freeSectors += *ptr;
		ptr += 4;
	}
	return freeSectors;
}

bool DiskImage::RAMD64AllocateSector(unsigned char* ramD64, int track, int sector)
{
	int sectorOffset = RAMD64GetSectorOffset(18, 0);
	unsigned bits;
	unsigned char* ptr = 4 + ramD64 + sectorOffset * 256 + 4 * (track - 1);
	unsigned bitMap = 1 << sector;
	bits = ptr[1];
	bits |= ptr[2] << 8;
	bits |= ptr[3] << 16;
	if (bits & bitMap)
	{
		ptr[0]--;
		bits &= ~bitMap;
		ptr[1] = (unsigned char)(bits & 0xff);
		ptr[2] = (unsigned char)((bits >> 8) & 0xff);
		ptr[3] = (unsigned char)((bits >> 16) & 0xff);
		return true;
	}
	return false;
}

bool DiskImage::RAMD64FindFreeSector(bool searchForwards, unsigned char* ramD64, int lastTrackUsed, int lastSectorUsed, int& track, int& sector)
{
	unsigned char* ptr;
	int sectorOffset = RAMD64GetSectorOffset(18, 0);
	int trackIndex;
	int sectorIndex;
	int stripAmount = 10;

	track = 0;
	sector = -1;
	if (searchForwards)
	{
		if (lastTrackUsed == -1)
			trackIndex = 18;
		else
			trackIndex = lastTrackUsed - 1;

		for (; trackIndex < 35; ++trackIndex)
		{
			if (lastTrackUsed != (trackIndex + 1))
			{
				lastSectorUsed = 0;
				stripAmount = 0;
			}
			if (lastSectorUsed < 0)
			{
				lastSectorUsed = 0;
				stripAmount = 0;
			}
			ptr = 4 + ramD64 + sectorOffset * 256 + 4 * trackIndex;
			if (*ptr != 0)
			{
				for (sectorIndex = 0; sectorIndex < SectorsPerTrack[trackIndex]; ++sectorIndex)
				{
					int sectorStripped = (sectorIndex + lastSectorUsed + stripAmount) % SectorsPerTrack[trackIndex];
					if (RAMD64AllocateSector(ramD64, trackIndex + 1, sectorStripped))
					{
						track = trackIndex + 1;
						sector = sectorStripped;
						return true;
					}
				}
			}
		}
	}
	else
	{
		if (lastTrackUsed == -1)
			trackIndex = 16;
		else
			trackIndex = lastTrackUsed - 1;
		for (; trackIndex >= 0; --trackIndex)
		{
			if (lastTrackUsed != (trackIndex + 1))
			{
				lastSectorUsed = 0;
				stripAmount = 0;
			}
			if (lastSectorUsed < 0)
			{
				lastSectorUsed = 0;
				stripAmount = 0;
			}

			ptr = 4 + ramD64 + sectorOffset * 256 + 4 * trackIndex;
			if (*ptr != 0)
			{
				for (sectorIndex = 0; sectorIndex < SectorsPerTrack[trackIndex]; ++sectorIndex)
				{
					int sectorStripped = (sectorIndex + lastSectorUsed + stripAmount) % SectorsPerTrack[trackIndex];
					if (RAMD64AllocateSector(ramD64, trackIndex + 1, sectorStripped))
					{


						track = trackIndex + 1;
						sector = sectorStripped;

						return true;
					}
				}
			}
		}
	}
	return false;
}

bool DiskImage::AddFileToRAMD64(unsigned char* ramD64, const char* name, const unsigned char* data, unsigned length)
{
	int numberOfSectorsRequired = length / (256 - 2) + ((length % (256 - 2)) ? 1 : 0);
	int freeSectors = RAMD64FreeSectors(ramD64);
	int bytesRemaining = length;
	bool success = false;
	int lastTrackUsed = -1;
	int lastSectorUsed = -1;

	if (freeSectors >= numberOfSectorsRequired)
	{
		unsigned char* ptrTrackSector = RAMD64AddDirectoryEntry(ramD64, name, data, length);
		if (ptrTrackSector)
		{
			int blocks;

			success = true;

			for (blocks = 0; blocks < numberOfSectorsRequired; ++blocks)
			{
				int track;
				int sector;

				bool found = RAMD64FindFreeSector(true, ramD64, lastTrackUsed, lastSectorUsed, track, sector);
				if (!found)
				{
					if (lastTrackUsed >= 18)
						lastTrackUsed = -1;
					found = RAMD64FindFreeSector(false, ramD64, lastTrackUsed, lastSectorUsed, track, sector);
				}
				if (found)
				{
					lastTrackUsed = track;
					lastSectorUsed = sector;
					ptrTrackSector[0] = track;
					ptrTrackSector[1] = sector;
					ptrTrackSector = ramD64 + RAMD64GetSectorOffset(track, sector) * 256;

					int bytesToCopy = (256 - 2);

					if (bytesRemaining < (256 - 2))
					{
						bytesToCopy = bytesRemaining + 1;
						ptrTrackSector[0] = 0;
						ptrTrackSector[1] = bytesToCopy;
					}

					bytesRemaining -= bytesToCopy;

					memcpy(ptrTrackSector + 2, data, bytesToCopy);
				}
				data += (256 - 2);
			}
		}
	}

	return success;
}
