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

#ifndef ROMs_H
#define ROMs_H

#include "types.h"

class ROMs
{
public:
	void SelectROM(const char* ROMName);
	void SelectROM(unsigned index);


	inline u8 Read(u16 address)
	{
		return ROMImages[currentROMIndex][address & 0x7fff];
	}

	void ResetCurrentROMIndex();

	static const int MAX_ROM_SIZE = 32768;
	static const int MAX_ROMS = 7;

	unsigned char ROMImages[MAX_ROMS][MAX_ROM_SIZE];
	char ROMNames[MAX_ROMS][256];
	unsigned ROMSizes[MAX_ROMS];
	bool ROMValid[MAX_ROMS];

	unsigned lastManualSelectedROMIndex;

	unsigned GetLongestRomNameLen() { return longestRomNameLen; }
	unsigned GetCurrentROMIndex() { return currentROMIndex; }
	unsigned GetCurrentROMSize() { return ROMSizes[currentROMIndex]; }
	unsigned UpdateLongestRomNameLen( unsigned maybeLongest );

protected:
	unsigned longestRomNameLen;
	unsigned currentROMIndex;
};

#endif
