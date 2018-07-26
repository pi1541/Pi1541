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

	inline u8 Read(u16 address)
	{
		return ROMImages[currentROMIndex][address & 0x3fff];
	}

	void ResetCurrentROMIndex();

	static const int ROM_SIZE = 16384;
	static const int MAX_ROMS = 7;

	unsigned char ROMImages[MAX_ROMS][ROM_SIZE];
	char ROMNames[MAX_ROMS][256];
	bool ROMValid[MAX_ROMS];

	unsigned currentROMIndex;
	unsigned lastManualSelectedROMIndex;
};
#endif
