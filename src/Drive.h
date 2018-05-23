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

#ifndef DRIVE_H
#define DRIVE_H

#include "m6522.h"
#include "DiskImage.h"
#include <stdlib.h>

class Drive
{
public:
	Drive();

	void SetVIA(m6522* pVIA)
	{
		m_pVIA = pVIA;
		pVIA->GetPortB()->SetPortOut(this, OnPortOut);
	}

	static void OnPortOut(void*, unsigned char status);

	bool Update();
	void Insert(DiskImage* diskImage);
	inline const DiskImage* GetDiskImage() const { return diskImage; }
	void Eject();
	void Reset();
	inline unsigned Track() const { return headTrackPos; }
	inline unsigned SectorPos() const { return headBitOffset >> 3; }
	inline unsigned GetHeadBitOffset() const { return headBitOffset; }
	inline bool IsMotorOn() const { return motor; }
	inline bool IsLEDOn() const { return LED; }

	inline unsigned char GetLastHeadDirection() const { return lastHeadDirection; } // For simulated head movement sounds
private:
	inline float GenerateRandomFluxReversalTime(float min, float max) { return ((max - min) * ((float)rand() / RAND_MAX)) + min; } // Inputs in micro seconds

	inline void ResetEncoderDecoder(float min, float max)
	{
		UE7Counter = CLOCK_SEL_AB;	// A and B inputs of UE7 come from the VIA's CLOCK SEL A/B outputs (ie PB5/6)
		UF4Counter = 0;
		randomFluxReversalTime = GenerateRandomFluxReversalTime(min, max);
	}
	inline void UpdateHeadSectorPosition()
	{
		// Disk spins at 300rpm = 5rps so to calculate how many 16Mhz cycles one rotation takes;-
		// 16000000 / 5 = 3200000;
		static const float CYCLES_16Mhz_PER_ROTATION = 3200000.0f;

		bitsInTrack = diskImage->BitsInTrack(headTrackPos);
		headBitOffset %= bitsInTrack;
		cyclesPerBit = CYCLES_16Mhz_PER_ROTATION / (float)bitsInTrack;
	}

	inline void MoveHead(unsigned char headDirection)
	{
		if (lastHeadDirection != headDirection)
		{
			if (((lastHeadDirection - 1) & 3) == headDirection)
			{
				if (headTrackPos > 0) headTrackPos--;
				// else head bang
			}
			else if (((lastHeadDirection + 1) & 3) == headDirection)
			{
				if (headTrackPos < HALF_TRACK_COUNT - 1) headTrackPos++;
			}
			lastHeadDirection = headDirection;
			UpdateHeadSectorPosition();
		}
	}

	void DumpTrack(unsigned track); // Used for debugging disk images.

	u32 AdvanceSectorPositionR(int& byte_offset); // No reason why I seperate these into individual read and write versions. I was just trying to get the bit stream to line up when rewriting over existing data.
	u32 AdvanceSectorPositionW(int& byte_offset);
	bool GetNextBit();
	void SetNextBit(bool value);

	DiskImage* diskImage;
	// When swapping disks some code waits for the write protect signal to go high which will happen if a human ejects a disk.
	// Emulate this by asserting the write protect signal for a few cycles before inserting the new disk image.
	u32	newDiskImageQueuedCylesRemaining;

	// CA1 (input)
	//	- !BYTE SYNC
	// CA2 (output)
	//	- BYTE SYNC enable
	// CB1 (NC)
	//	- check pulled H/L
	// CB2 (output)
	//	- R/!W
	m6522* m_pVIA;

	float cyclesForBit;
	u32 readShiftRegister;
	unsigned headTrackPos;
	u32 headBitOffset;
	float randomFluxReversalTime;
	int UE7Counter;
	int UF4Counter;
	int UE3Counter;
	int CLOCK_SEL_AB;
	bool SO;
	unsigned char lastHeadDirection;
	u32 bitsInTrack;
	float cyclesPerBit;
	bool motor;
	bool LED;
	u8 writeShiftRegister;
};
#endif
