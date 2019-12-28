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

#ifndef DISKCADDY_H
#define DISKCADDY_H

#include <vector>
#include "DiskImage.h"
#include "Screen.h"
#include "ROMs.h"

class DiskCaddy
{
public:
	DiskCaddy()
		: selectedIndex(0)
#if not defined(EXPERIMENTALZERO)
		, screen(0)
#endif
		, screenLCD(0)
		, roms(0)
	{
	}
	void SetScreen(Screen* screen, ScreenBase* screenLCD, ROMs* roms)
	{ 
#if not defined(EXPERIMENTALZERO)
		this->screen = screen;
#endif
		this->screenLCD = screenLCD;
		this->roms = roms;
	}

	bool Empty();

	bool Insert(const FILINFO* fileInfo, bool readOnly);

	DiskImage* GetCurrentDisk()
	{
#if defined(EXPERIMENTALZERO)
		Update();
#endif
		if (selectedIndex < disks.size())
			return disks[selectedIndex];

		return 0;
	}

	DiskImage* NextDisk()
	{
		selectedIndex = (selectedIndex + 1) % (u32)disks.size();
		return GetCurrentDisk();
	}

	DiskImage* PrevDisk()
	{
		--selectedIndex;
		if ((int)selectedIndex < 0)
			selectedIndex += (u32)disks.size();
		return GetCurrentDisk();
	}

	u32 GetNumberOfImages() const { return disks.size(); }
	u32 GetSelectedIndex() const { return selectedIndex; }

	DiskImage* GetImage(unsigned index) { return disks[index]; }
	DiskImage* SelectImage(unsigned index)
	{
		if (selectedIndex != index && index < disks.size())
		{
			selectedIndex = index;
			return GetCurrentDisk();
		}
		return 0;
	}
	DiskImage* SelectFirstImage()
	{
		if (disks.size())
		{
			selectedIndex = 0;
			return GetCurrentDisk();
		}
		return 0;
	}

	void Display();
	bool Update();

private:
	bool InsertD64(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly);
	bool InsertG64(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly);
	bool InsertNIB(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly);
	bool InsertNBZ(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly);
	bool InsertD81(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly);
	bool InsertT64(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly);
	bool InsertPRG(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly);

	void ShowSelectedImage(u32 index);

	std::vector<DiskImage*> disks;
	u32 selectedIndex;
	u32 oldCaddyIndex;
#if not defined(EXPERIMENTALZERO)
	ScreenBase* screen;
#endif
	ScreenBase* screenLCD;
	ROMs* roms;
};

#endif