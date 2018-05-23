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
extern "C"
{
#include "rpi-gpio.h"
}

unsigned char DiskImage::readBuffer[READBUFFER_SIZE];

static unsigned char compressionBuffer[HALF_TRACK_COUNT * NIB_TRACK_LENGTH];

static const unsigned short SECTOR_LENGTH = 256;
static const unsigned short SECTOR_LENGTH_WITH_CHECKSUM = 260;
static const unsigned char GCR_SYNC_BYTE = 0xff;
static const unsigned char GCR_GAP_BYTE = 0x55;
static const int SECTOR_HEADER_LENGTH = 8;
static const unsigned MAX_D64_SIZE = 0x30000;

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
		break;
		case G64:
			CloseG64();
		break;
		case NIB:
			CloseNIB();
		break;
		case NBZ:
			CloseNBZ();
		break;
		default:
		break;
	}
	memset(tracks, 0x55, sizeof(tracks));
	memset(trackLengths, 0, sizeof(trackLengths));
	diskType = NONE;
	fileInfo = 0;
}

void DiskImage::DumpTrack(unsigned track)
{
	unsigned char* src = tracks[track];
	unsigned trackLength = trackLengths[track];
	DEBUG_LOG("track = %d trackLength = %d\r\n", track, trackLength);
	for (unsigned index = 0; index < trackLength; ++index)
	{
		DEBUG_LOG("%d %02x\r\n", index, src[index]);
	}
}

bool DiskImage::OpenD64(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size)
{
	Close();

	this->fileInfo = fileInfo;

	unsigned offset = 0;

	if (size > MAX_D64_SIZE)
		size = MAX_D64_SIZE;

	attachedImageSize = size;

	for (unsigned halfTrackIndex = 0; halfTrackIndex < HALF_TRACK_COUNT; ++halfTrackIndex)
	{
		unsigned char track = (halfTrackIndex >> 1);
		unsigned char* dest = tracks[halfTrackIndex];

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

	diskType = D64;
	return true;
}

bool DiskImage::WriteD64()
{
	if (readOnly)
		return true;

	FIL fp;
	FRESULT res = f_open(&fp, fileInfo->fname, FA_CREATE_ALWAYS | FA_WRITE);
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

		f_utime(fileInfo->fname, fileInfo);
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

bool DiskImage::OpenG64(const FILINFO* fileInfo, unsigned char* diskImage, unsigned size)
{
	Close();

	this->fileInfo = fileInfo;

	attachedImageSize = size;

	if (memcmp(diskImage, "GCR-1541", 8) == 0)
	{
		//DEBUG_LOG("Is G64\r\n");

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
				memcpy(tracks[track], trackData, trackLength);
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

bool DiskImage::WriteG64()
{
	if (readOnly)
		return true;

	FIL fp;
	FRESULT res = f_open(&fp, fileInfo->fname, FA_CREATE_ALWAYS | FA_WRITE);
	if (res == FR_OK)
	{
		u32 bytesToWrite;
		u32 bytesWritten;
		int track_inc = 1;

		BYTE header[12];
		DWORD gcr_track_p[MAX_HALFTRACKS_1541] = { 0 };
		DWORD gcr_speed_p[MAX_HALFTRACKS_1541] = { 0 };
		BYTE gcr_track[NIB_TRACK_LENGTH + 2];
		size_t track_len;
		int index = 0, track;
		BYTE buffer[NIB_TRACK_LENGTH], tempfillbyte;

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
			memcpy(buffer, tracks[track], track_len);

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
			trackLengths[track] = extract_GCR_track(tracks[track], nibdata, &align
				//, ALIGN_GAP
				, ALIGN_NONE
				, capacity_min[trackDensity[track]],
				capacity_max[trackDensity[track]]);

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
			for (track = 0; track < (MAX_TRACKS_1541 * 2); ++track)
			{
				if (trackUsed[track])
				{
					if (f_write(&fp, tracks[track], bytesToWrite, &bytesWritten) != FR_OK || bytesToWrite != bytesWritten)
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
		else if (toupper((char)ext[1]) == 'L' && toupper((char)ext[2]) == 'S' && toupper((char)ext[3]) == 'T')
			return LST;
	}
	return NONE;
}

bool DiskImage::IsDiskImageExtention(const char* diskImageName)
{
	return GetDiskImageTypeViaExtention(diskImageName) != NONE;
}

bool DiskImage::IsLSTExtention(const char* diskImageName)
{
	char* ext = strrchr((char*)diskImageName, '.');

	if (ext && toupper((char)ext[1]) == 'L' && toupper((char)ext[2]) == 'S' && toupper((char)ext[3]) == 'T')
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
	unsigned char* end = tracks[track] + trackLengths[track];

	shift = bitIndex & 7;
	offset = tracks[track] + (bitIndex >> 3);

	byte = offset[0] << shift;
	for (i = 0; i < num; i++, buf += 4)
	{
		for (j = 0; j < 5; j++)
		{
			offset++;
			if (offset >= end)
				offset = tracks[track];
		
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
	unsigned char byte = tracks[track][bitIndex >> 3] << (bitIndex & 7);
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
			if (bitIndex >= NIB_TRACK_LENGTH * 8)
				bitIndex = 0;
			byte = tracks[track][bitIndex >> 3];
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
