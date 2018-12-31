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

#include "DiskCaddy.h"
#include "debug.h"
#include <string.h>
#include <stdlib.h>
#include "ff.h"
extern "C"
{
#include "rpi-gpio.h"	// For SetACTLed
}

static const u32 screenPosXCaddySelections = 240;
static const u32 screenPosYCaddySelections = 280;
static char buffer[256] = { 0 };
static u32 white = RGBA(0xff, 0xff, 0xff, 0xff);
static u32 red = RGBA(0xff, 0, 0, 0xff);

bool DiskCaddy::Empty()
{
	int x;
	int y;
	int index;
	bool anyDirty = false;

	for (index = 0; index < (int)disks.size(); ++index)
	{
		if (disks[index].IsDirty())
		{
			anyDirty = true;
			if (screen)
			{
				x = screen->ScaleX(screenPosXCaddySelections);
				y = screen->ScaleY(screenPosYCaddySelections);

				snprintf(buffer, 256, "Saving %s\r\n", disks[index].GetName());
				screen->PrintText(false, x, y, buffer, RGBA(0xff, 0xff, 0xff, 0xff), red);
			}

			if (screenLCD)
			{
				RGBA BkColour = RGBA(0, 0, 0, 0xFF);
				screenLCD->Clear(BkColour);
				x = 0;
				y = 0;

				snprintf(buffer, 256, "Saving");
				screenLCD->PrintText(false, x, y, buffer, RGBA(0xff, 0xff, 0xff, 0xff), BkColour);
				y += screenLCD->GetFontHeight();
				snprintf(buffer, 256, "%s                ", disks[index].GetName());
				screenLCD->PrintText(false, x, y, buffer, RGBA(0xff, 0xff, 0xff, 0xff), red);
				screenLCD->SwapBuffers();
			}
		}
		disks[index].Close();
	}

	if (anyDirty)
	{
		if (screen)
		{
			x = screen->ScaleX(screenPosXCaddySelections);
			y = screen->ScaleY(screenPosYCaddySelections);

			snprintf(buffer, 256, "Saving Complete             \r\n");
			screen->PrintText(false, x, y, buffer, RGBA(0xff, 0xff, 0xff, 0xff), red);
		}

		if (screenLCD)
		{
			RGBA BkColour = RGBA(0, 0, 0, 0xFF);
			screenLCD->Clear(BkColour);
			x = 0;
			y = 0;

			snprintf(buffer, 256, "Saving");
			screenLCD->PrintText(false, x, y, buffer, RGBA(0xff, 0xff, 0xff, 0xff), BkColour);
			y += screenLCD->GetFontHeight();
			snprintf(buffer, 256, "Complete                ");
			screenLCD->PrintText(false, x, y, buffer, RGBA(0xff, 0xff, 0xff, 0xff), red);
			screenLCD->SwapBuffers();
		}
	}

	disks.clear();
	selectedIndex = 0;
	return anyDirty;
}

bool DiskCaddy::Insert(const FILINFO* fileInfo, bool readOnly)
{
	int x;
	int y;
	bool success;
	FIL fp;
	FRESULT res = f_open(&fp, fileInfo->fname, FA_READ);
	if (res == FR_OK)
	{
		if (screen)
		{
			x = screen->ScaleX(screenPosXCaddySelections);
			y = screen->ScaleY(screenPosYCaddySelections);

			snprintf(buffer, 256, "Loading %s\r\n", fileInfo->fname);
			screen->PrintText(false, x, y, buffer, RGBA(0xff, 0xff, 0xff, 0xff), red);
		}

		if (screenLCD)
		{
			RGBA BkColour = RGBA(0, 0, 0, 0xFF);
			screenLCD->Clear(BkColour);
			x = 0;
			y = 0;

			snprintf(buffer, 256, "Loading");
			screenLCD->PrintText(false, x, y, buffer, RGBA(0xff, 0xff, 0xff, 0xff), BkColour);
			y += screenLCD->GetFontHeight();
			snprintf(buffer, 256, "%s                ", fileInfo->fname);
			screenLCD->PrintText(false, x, y, buffer, RGBA(0xff, 0xff, 0xff, 0xff), red);
			screenLCD->SwapBuffers();
		}

		u32 bytesRead;
		SetACTLed(true);
		f_read(&fp, DiskImage::readBuffer, READBUFFER_SIZE, &bytesRead);
		SetACTLed(false);
		f_close(&fp);

		DiskImage::DiskType diskType = DiskImage::GetDiskImageTypeViaExtention(fileInfo->fname);
		switch (diskType)
		{
			case DiskImage::D64:
				success = InsertD64(fileInfo, (unsigned char*)DiskImage::readBuffer, bytesRead, readOnly);
				break;
			case DiskImage::G64:
				success = InsertG64(fileInfo, (unsigned char*)DiskImage::readBuffer, bytesRead, readOnly);
				break;
			case DiskImage::NIB:
				success = InsertNIB(fileInfo, (unsigned char*)DiskImage::readBuffer, bytesRead, readOnly);
				break;
			case DiskImage::NBZ:
				success = InsertNBZ(fileInfo, (unsigned char*)DiskImage::readBuffer, bytesRead, readOnly);
				break;
			case DiskImage::D81:
				success = InsertD81(fileInfo, (unsigned char*)DiskImage::readBuffer, bytesRead, readOnly);
				break;
			default:
				success = false;
				break;
		}
		if (success)
		{
			DEBUG_LOG("Mounted into caddy %s - %d\r\n", fileInfo->fname, bytesRead);
		}
	}
	else
	{
		DEBUG_LOG("Failed to open %s\r\n", fileInfo->fname);
		success = false;
	}

	oldCaddyIndex = 0;

	return success;
}

bool DiskCaddy::InsertD64(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly)
{
	DiskImage diskImage;
	if (diskImage.OpenD64(fileInfo, diskImageData, size))
	{
		diskImage.SetReadOnly(readOnly);
		disks.push_back(diskImage);
		selectedIndex = disks.size() - 1;
		return true;
	}
	return false;
}

bool DiskCaddy::InsertG64(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly)
{
	DiskImage diskImage;
	if (diskImage.OpenG64(fileInfo, diskImageData, size))
	{
		diskImage.SetReadOnly(readOnly);
		disks.push_back(diskImage);
		//DEBUG_LOG("Disks size = %d\r\n", disks.size());
		selectedIndex = disks.size() - 1;
		return true;
	}
	return false;
}

bool DiskCaddy::InsertNIB(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly)
{
	DiskImage diskImage;
	if (diskImage.OpenNIB(fileInfo, diskImageData, size))
	{
		// At the moment we cannot write out NIB files.
		diskImage.SetReadOnly(true);// readOnly);
		disks.push_back(diskImage);
		selectedIndex = disks.size() - 1;
		return true;
	}
	return false;
}

bool DiskCaddy::InsertNBZ(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly)
{
	DiskImage diskImage;
	if (diskImage.OpenNBZ(fileInfo, diskImageData, size))
	{
		// At the moment we cannot write out NIB files.
		diskImage.SetReadOnly(true);// readOnly);
		disks.push_back(diskImage);
		selectedIndex = disks.size() - 1;
		return true;
	}
	return false;
}

bool DiskCaddy::InsertD81(const FILINFO* fileInfo, unsigned char* diskImageData, unsigned size, bool readOnly)
{
	DiskImage diskImage;
	if (diskImage.OpenD81(fileInfo, diskImageData, size))
	{
		diskImage.SetReadOnly(readOnly);
		disks.push_back(diskImage);
		selectedIndex = disks.size() - 1;
		return true;
	}
	return false;
}

void DiskCaddy::Display()
{
	unsigned numberOfImages = GetNumberOfImages();
	unsigned caddyIndex;
	int x;
	int y;
	if (screen)
	{
		x = screen->ScaleX(screenPosXCaddySelections);
		y = screen->ScaleY(screenPosYCaddySelections);

		snprintf(buffer, 256, "Emulating\r\n");
		screen->PrintText(false, x, y, buffer, RGBA(0xff, 0xff, 0xff, 0xff), red);
		y += 16;

		for (caddyIndex = 0; caddyIndex < numberOfImages; ++caddyIndex)
		{
			DiskImage* image = GetImage(caddyIndex);
			const char* name = image->GetName();
			if (name)
			{
				snprintf(buffer, 256, "%d %s\r\n", caddyIndex + 1, name);
				screen->PrintText(false, x, y, buffer, RGBA(0xff, 0xff, 0xff, 0xff), red);
				y += 16;
			}
		}
	}

	if (screenLCD)
	{
		RGBA BkColour = RGBA(0, 0, 0, 0xFF);
		screenLCD->Clear(BkColour);
	}
	ShowSelectedImage(0);
}

void DiskCaddy::ShowSelectedImage(u32 index)
{
	u32 x;
	u32 y;
	if (screen)
	{
		x = screen->ScaleX(screenPosXCaddySelections) - 16;
		y = screen->ScaleY(screenPosYCaddySelections) + 16 + 16 * index;
		snprintf(buffer, 256, "*");
		screen->PrintText(false, x, y, buffer, white, red);
	}
	if (screenLCD)
	{
		unsigned numberOfImages = GetNumberOfImages();
		unsigned numberOfDisplayedImages = (screenLCD->Height()/screenLCD->GetFontHeight())-1;
		unsigned caddyIndex;

		RGBA BkColour = RGBA(0, 0, 0, 0xFF);
		//screenLCD->Clear(BkColour);
		x = 0;
		y = 0;

		snprintf(buffer, 256, "        D %d/%d %c        "
			, index + 1
			, numberOfImages
			, GetImage(index)->GetReadOnly() ? 'R' : ' '
			);
		screenLCD->PrintText(false, x, y, buffer, 0, RGBA(0xff, 0xff, 0xff, 0xff));
		y += screenLCD->GetFontHeight();

		if (numberOfImages > numberOfDisplayedImages && index > numberOfDisplayedImages-1)
		{
			if (numberOfImages - index < numberOfDisplayedImages)
				caddyIndex = numberOfImages - numberOfDisplayedImages;
			else
				caddyIndex = index;
		}
		else
		{
			caddyIndex = 0;
		}

		for (; caddyIndex < numberOfImages; ++caddyIndex)
		{
			DiskImage* image = GetImage(caddyIndex);
			const char* name = image->GetName();
			if (name)
			{
				memset(buffer, ' ',  screenLCD->Width()/screenLCD->GetFontWidth());
				screenLCD->PrintText(false, x, y, buffer, BkColour, BkColour);
				snprintf(buffer, 256, "%d %s", caddyIndex + 1, name);
				screenLCD->PrintText(false, x, y, buffer, 0, caddyIndex == index ? RGBA(0xff, 0xff, 0xff, 0xff) : BkColour);
				y += screenLCD->GetFontHeight();
			}
			if (y >= screenLCD->Height())
				break;
		}
		screenLCD->SwapBuffers();
	}
}

bool DiskCaddy::Update()
{
	u32 y;
	u32 x;
	u32 caddyIndex = GetSelectedIndex();
	if (caddyIndex != oldCaddyIndex)
	{
		if (screen)
		{
			x = screen->ScaleX(screenPosXCaddySelections) - 16;
			y = screen->ScaleY(screenPosYCaddySelections) + 16 + 16 * oldCaddyIndex;
			snprintf(buffer, 256, " ");
			screen->PrintText(false, x, y, buffer, red, red);
			oldCaddyIndex = caddyIndex;
			ShowSelectedImage(oldCaddyIndex);
		}

		if (screenLCD)
		{
			
		}

		return true;
	}
	return false;
}
