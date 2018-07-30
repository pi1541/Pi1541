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

#include "ROMs.h"
#include "debug.h"
#include <strings.h>

void ROMs::ResetCurrentROMIndex()
{
	currentROMIndex = lastManualSelectedROMIndex;

	DEBUG_LOG("Reset ROM back to %d %s\r\n", currentROMIndex, ROMNames[currentROMIndex]);
}

void ROMs::SelectROM(const char* ROMName)
{
	unsigned index;

	for (index = 0; index < MAX_ROMS; ++index)
	{
		if (ROMNames[index] && strcasecmp(ROMNames[index], ROMName) == 0)
		{
			DEBUG_LOG("LST switching ROM %d %s\r\n", index, ROMNames[index]);

			currentROMIndex = index;
			break;
		}
	}
}

unsigned ROMs::UpdateLongestRomNameLen( unsigned maybeLongest )
{
	if (maybeLongest > longestRomNameLen)
		longestRomNameLen = maybeLongest;

	return longestRomNameLen;
}
