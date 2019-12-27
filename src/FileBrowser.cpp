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

#include "FileBrowser.h"
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <algorithm>
#include <ctype.h>
#include "debug.h"
#include "options.h"
#include "InputMappings.h"
#include "stb_image.h"
#include "Petscii.h"
extern "C"
{
#include "rpi-gpio.h"
}

#include "iec_commands.h"
extern IEC_Commands m_IEC_Commands;
extern Options options;


#define PNG_WIDTH 320
#define PNG_HEIGHT 200

extern void GlobalSetDeviceID(u8 id);
extern void CheckAutoMountImage(EXIT_TYPE reset_reason , FileBrowser* fileBrowser);

extern bool SwitchDrive(const char* drive);
extern int numberOfUSBMassStorageDevices;

unsigned char FileBrowser::LSTBuffer[FileBrowser::LSTBuffer_size];

static const u32 palette[] = 
{
	RGBA(0x00, 0x00, 0x00, 0xFF),
	RGBA(0xFF, 0xFF, 0xFF, 0xFF),
	RGBA(0x88, 0x39, 0x32, 0xFF),
	RGBA(0x67, 0xB6, 0xBD, 0xFF),
	RGBA(0x8B, 0x4F, 0x96, 0xFF),
	RGBA(0x55, 0xA0, 0x49, 0xFF),
	RGBA(0x40, 0x31, 0x8D, 0xFF),
	RGBA(0xBF, 0xCE, 0x72, 0xFF),
	RGBA(0x8B, 0x54, 0x29, 0xFF),
	RGBA(0x57, 0x42, 0x00, 0xFF),
	RGBA(0xB8, 0x69, 0x62, 0xFF),
	RGBA(0x50, 0x50, 0x50, 0xFF),
	RGBA(0x78, 0x78, 0x78, 0xFF),
	RGBA(0x94, 0xE0, 0x89, 0xFF),
	RGBA(0x78, 0x69, 0xC4, 0xFF),
	RGBA(0x9F, 0x9F, 0x9F, 0xFF)
};

void FileBrowser::BrowsableListView::RefreshLine(u32 entryIndex, u32 x, u32 y, bool selected)
{
	char buffer1[128] = { 0 };
	char buffer2[256] = { 0 };
	u32 colour;
	RGBA BkColour = RGBA(0, 0, 0, 0xFF); //palette[VIC2_COLOUR_INDEX_BLUE];
	u32 columnsMax = columns;

	if (columnsMax > sizeof(buffer1)-1)
		columnsMax = sizeof(buffer1)-1;

	if (entryIndex < list->entries.size())
	{
		FileBrowser::BrowsableList::Entry* entry = &list->entries[entryIndex];
		if (screen->IsLCD())
		{
			// pre-clear line on OLED
			memset(buffer1, ' ', columnsMax);
			screen->PrintText(false, x, y, buffer1, BkColour, BkColour);

			if (entry->filImage.fattrib & AM_DIR)
			{
				snprintf(buffer2, 256, "[%s]", entry->filImage.fname);
			}
			else
			{
				char ROstring[8] = { 0 };
				if (entry->filImage.fattrib & AM_RDO)
					strncpy (ROstring, "<", 8);
				if (entry->caddyIndex != -1)
					snprintf(buffer2, 256, "%d>%s%s"
						, entry->caddyIndex
						, entry->filImage.fname
						, ROstring
						);
				else
					snprintf(buffer2, 256, "%s%s", entry->filImage.fname, ROstring);
			}
		}
		else
		{
			snprintf(buffer2, 256, "%s", entry->filImage.fname);
		}
		int len = strlen(buffer2 + highlightScrollOffset);
		strncpy(buffer1, buffer2 + highlightScrollOffset, sizeof(buffer1));
		if (!screen->IsLCD())
		{
			// space pad the remainder of the line (but not on OLED)
			while (len < (int)columnsMax)
				buffer1[len++] = ' ';
			buffer1[columnsMax] = 0;
		}
		if (selected)
		{
			if (entry->filImage.fattrib & AM_DIR)
			{
				screen->PrintText(false, x, y, buffer1, palette[VIC2_COLOUR_INDEX_LBLUE], RGBA(0xff, 0xff, 0xff, 0xff));
			}
			else
			{
				colour = RGBA(0xff, 0, 0, 0xff);
				if (entry->filImage.fattrib & AM_RDO)
					colour = palette[VIC2_COLOUR_INDEX_RED];
				screen->PrintText(false, x, y, buffer1, colour, RGBA(0xff, 0xff, 0xff, 0xff));
			}
		}
		else
		{
			if (entry->filImage.fattrib & AM_DIR)
			{
				screen->PrintText(false, x, y, buffer1, palette[VIC2_COLOUR_INDEX_LBLUE], BkColour);
			}
			else
			{
				colour = palette[VIC2_COLOUR_INDEX_LGREY];
				if (entry->filImage.fattrib & AM_RDO)
					colour = palette[VIC2_COLOUR_INDEX_PINK];
				screen->PrintText(false, x, y, buffer1, colour, BkColour);
			}
		}
	}
	else // line is blank, write spaces
	{
		memset(buffer1, ' ', columnsMax);
		screen->PrintText(false, x, y, buffer1, BkColour, BkColour);
	}
}

void FileBrowser::BrowsableListView::Refresh()
{
	u32 index;
	u32 entryIndex;
	u32 x = positionX;
	u32 y = positionY;

	highlightScrollOffset = 0;

	// Ensure the current selection is visible
	if (list->currentIndex - offset >= rows)
	{
		//DEBUG_LOG("CI= %d O = %d R = %d\r\n", list->currentIndex, offset, rows);
		offset = list->currentIndex - rows + 1;
		if ((int)offset < 0) offset = 0;
	}

	for (index = 0; index < rows; ++index)
	{
		entryIndex = offset + index;

		RefreshLine(entryIndex, x, y, /*showSelected && */list->currentIndex == entryIndex);
		y += screen->GetFontHeight ();
	}

	screen->SwapBuffers();
}

void FileBrowser::BrowsableListView::RefreshHighlightScroll()
{
	char buffer2[256] = { 0 };

	FileBrowser::BrowsableList::Entry* entry = list->current;
	if (screen->IsMonocrome())
	{
		if (entry->filImage.fattrib & AM_DIR)
		{
			snprintf(buffer2, 256, "[%s]", entry->filImage.fname);
		}
		else
		{
			char ROstring[8] = { 0 };
			if (entry->filImage.fattrib & AM_RDO)
				strncpy (ROstring, "<", 8);
			if (entry->caddyIndex != -1)
				snprintf(buffer2, 256, "%d>%s%s"
					, entry->caddyIndex
					, entry->filImage.fname
					, ROstring
					);
			else
				snprintf(buffer2, 256, "%s%s", entry->filImage.fname, ROstring);
		}
	}
	else
	{
		snprintf(buffer2, 256, "%s", entry->filImage.fname);
	}


	int len = strlen(buffer2);
	if (len > (int)columns)
	{
		if (highlightScrollOffset == 0)
		{
			highlightScrollStartCount++;
			if (highlightScrollStartCount > 10)
			{
				highlightScrollStartCount = 0;
				highlightScrollOffset = 1;
			}
		}
		else if (len - (int)(highlightScrollOffset + 1) <= (int)(columns - 1))
		{
			highlightScrollEndCount++;
			if (highlightScrollEndCount > 10)
			{
				highlightScrollOffset = 0;
				highlightScrollEndCount = 0;
			}
		}
		else
		{
			highlightScrollOffset++;
		}

		int rowIndex = list->currentIndex - offset;
		
		u32 y = positionY;
		y += rowIndex * screen->GetFontHeight ();

		RefreshLine(list->currentIndex, 0, y, true);

		screen->RefreshRows(rowIndex, 1);
	}
}

bool FileBrowser::BrowsableListView::CheckBrowseNavigation(bool pageOnly)
{
	bool dirty = false;
	u32 numberOfEntriesMinus1 = list->entries.size() - 1;

	if (inputMappings->BrowseDown())
	{
		if (list->currentIndex < numberOfEntriesMinus1)
		{
			if (!pageOnly)
			{
				list->currentIndex++;
				list->SetCurrent();
			}
			if (list->currentIndex >= (offset + rows) && (list->currentIndex < list->entries.size()))
				offset++;
			dirty = true;
		}
		else
		{
			if (!pageOnly)
			{
				list->currentIndex = 0;
				list->SetCurrent();
				dirty = true;
			}
		}
	}
	if (inputMappings->BrowseUp())
	{
		if (list->currentIndex > 0)
		{
			if (!pageOnly)
			{
				list->currentIndex--;
				list->SetCurrent();
			}
			if ((offset > 0) && (list->currentIndex < offset))
				offset--;
			dirty = true;
		}
		else
		{
			if (!pageOnly)
			{
				list->currentIndex = list->entries.size() - 1;
				list->SetCurrent();
				dirty = true;
			}
		}
	}
	if ((lcdPgUpDown && inputMappings->BrowsePageDownLCD()) || (!lcdPgUpDown && inputMappings->BrowsePageDown()))
	{
		u32 rowsMinus1 = rows - 1;

		if (list->currentIndex == offset + rowsMinus1)
		{
			// Need to move the screen window down so that the currentIndex is now at the top of the screen
			offset = list->currentIndex;

			// Current index now becomes the bottom one
			if (offset + rowsMinus1 > numberOfEntriesMinus1)
				list->currentIndex = numberOfEntriesMinus1; // Not enough entries to move another page just move to the last entry
			else // Move the window down a page
				list->currentIndex = offset + rowsMinus1;
		}
		else
		{
			// Need to move to list->offset + rowsMinus1
			if (offset + rowsMinus1 > numberOfEntriesMinus1)
				list->currentIndex = numberOfEntriesMinus1; // Run out of entries before we hit the bottom. Just move to the bottom.
			else
				list->currentIndex = offset + rowsMinus1; // Move the bottom of the screen
		}
		list->SetCurrent();
		dirty = true;
	}
	if ((lcdPgUpDown && inputMappings->BrowsePageUpLCD()) || (!lcdPgUpDown && inputMappings->BrowsePageUp()))
	{
		if (list->currentIndex == offset)
		{
			// If the cursor is already at the top of the window then page up
			int offsetInWindow = (int)list->currentIndex - (int)rows;
			if (offsetInWindow < 0) offset = 0;
			else offset = (u32)offsetInWindow;
			list->currentIndex = offset;
		}
		else
		{
			list->currentIndex = offset; // Move the cursor to the top of the window
		}
		list->SetCurrent();
		dirty = true;
	}

	return dirty;
}

FileBrowser::BrowsableList::BrowsableList()
	: inputMappings(0)
	, current(0)
	, currentIndex(0)
	, currentHighlightTime(0)
	, scrollHighlightRate(0)
	, searchPrefixIndex(0)
	, searchLastKeystrokeTime(0)
{
	lastUpdateTime = read32(ARM_SYSTIMER_CLO);
	searchPrefix[0] = 0;
}

void FileBrowser::BrowsableList::ClearSelections()
{
	u32 entryIndex;

	for (entryIndex = 0; entryIndex < entries.size(); ++entryIndex)
	{
		entries[entryIndex].caddyIndex = -1;
	}
}

void FileBrowser::BrowsableList::RefreshViews()
{
	u32 index;
	for (index = 0; index < views.size(); ++index)
	{
		views[index].Refresh();
	}
}

void FileBrowser::BrowsableList::RefreshViewsHighlightScroll()
{
	u32 index;
	for (index = 0; index < views.size(); ++index)
	{
		views[index].RefreshHighlightScroll();
	}
}

bool FileBrowser::BrowsableList::CheckBrowseNavigation()
{
	u32 numberOfEntriesMinus1 = entries.size() - 1;

	bool dirty = false;
	u32 index;

	// Calculate the number of micro seconds since we were last called.
	u32 updateTime = read32(ARM_SYSTIMER_CLO);
	u32 timeDelta;
	if (updateTime < lastUpdateTime)
		timeDelta = updateTime + (0xffffffff - lastUpdateTime);	// wrapped
	else
		timeDelta = updateTime - lastUpdateTime;
	lastUpdateTime = updateTime;

	if (searchPrefixIndex != 0)	// Are they typing?
	{
		searchLastKeystrokeTime += timeDelta;
	}

	for (index = 0; index < views.size(); ++index)
	{
		dirty |= views[index].CheckBrowseNavigation(index != 0);
	}

#if not defined(EXPERIMENTALZERO)
	// check for keys a-z and 0-9
	char searchChar = inputMappings->getKeyboardNumLetter();
	if (searchChar)
	{
		unsigned found=0;
		u32 i=0;

		searchLastKeystrokeTime = 0;

		searchPrefix[searchPrefixIndex] = searchChar;
		if (searchPrefixIndex < KEYBOARD_SEARCH_BUFFER_SIZE - 1)
		{
			searchPrefixIndex++;
			searchPrefix[searchPrefixIndex] = 0;
			dirty |= 1;
		}

		// first look from next to last
		for (i=1+currentIndex; i <= numberOfEntriesMinus1 ; i++)
		{
			FileBrowser::BrowsableList::Entry* entry = &entries[i];
			if (strncasecmp(searchPrefix, entry->filImage.fname, searchPrefixIndex) == 0)
			{
				found=i;
				break;
			}
		}
		if (!found)
		{
			// look from first to previous
			for (i=0; i< 1+currentIndex ; i++)
			{
				FileBrowser::BrowsableList::Entry* entry = &entries[i];
				if (strncasecmp(searchPrefix, entry->filImage.fname, searchPrefixIndex) == 0)
				{
					found=i;
					break;
				}
			}
		}

		if (found)
		{
			currentIndex=found;
			SetCurrent();
			dirty |= 1;
		}
	}
	else
	{
		if (searchLastKeystrokeTime > 500000)
		{
			searchPrefixIndex = 0;
			searchPrefix[0] = 0;
			searchLastKeystrokeTime = 0;
			dirty |= 1;
		}

		if (inputMappings->BrowseHome())
		{
			currentIndex = 0;
			SetCurrent();
			dirty |= 1;
		}
		else if (inputMappings->BrowseEnd())
		{
			currentIndex = numberOfEntriesMinus1;
			SetCurrent();
			dirty |= 1;
		}
	}
#endif
	return dirty;
}

FileBrowser::BrowsableList::Entry* FileBrowser::BrowsableList::FindEntry(const char* name)
{
	int index;
	int len = (int)entries.size();

	for (index = 0; index < len; ++index)
	{
		Entry* entry = &entries[index];
		if (!(entry->filImage.fattrib & AM_DIR) && strcasecmp(name, entry->filImage.fname) == 0)
			return entry;
	}
	return 0;
}

FileBrowser::FileBrowser(InputMappings* inputMappings, DiskCaddy* diskCaddy, ROMs* roms, u8* deviceID, bool displayPNGIcons, ScreenBase* screenMain, ScreenBase* screenLCD, float scrollHighlightRate)
	: inputMappings(inputMappings)
	, state(State_Folders)
	, diskCaddy(diskCaddy)
	, selectionsMade(false)
	, roms(roms)
	, deviceID(deviceID)
	, displayPNGIcons(displayPNGIcons)
#if not defined(EXPERIMENTALZERO)
	, screenMain(screenMain)
#endif
	, screenLCD(screenLCD)
	, scrollHighlightRate(scrollHighlightRate)
	, displayingDevices(false)
{

	folder.scrollHighlightRate = scrollHighlightRate;

#if not defined(EXPERIMENTALZERO)
	u32 columns = screenMain->ScaleX(80);
	u32 rows = (int)(38.0f * screenMain->GetScaleY());
	u32 positionX = 0;
	u32 positionY = 17;

	if (rows < 1)
		rows = 1;

	folder.AddView(screenMain, inputMappings, columns, rows, positionX, positionY, false);

	positionX = screenMain->ScaleX(1024 - 320);
	columns = screenMain->ScaleX(40);
	caddySelections.AddView(screenMain, inputMappings, columns, rows, positionX, positionY, false);


#endif

	if (screenLCD)
	{
		u32 columns = screenLCD->Width() / 8;
		u32 rows = screenLCD->Height() / screenLCD->GetFontHeight();
		u32 positionX = 0;
		u32 positionY = 0;

		folder.AddView(screenLCD, inputMappings, columns, rows, positionX, positionY, true);
	}

	f_chdir("/1541");
	RefreshFolderEntries();
}

u32 FileBrowser::Colour(int index)
{
	return palette[index & 0xf];
}

struct greater
{
	bool operator()(const FileBrowser::BrowsableList::Entry& lhs, const FileBrowser::BrowsableList::Entry& rhs) const
	{
		if (strcasecmp(lhs.filImage.fname, "..") == 0)
			return true;
		else if (strcasecmp(rhs.filImage.fname, "..") == 0)
			return false;
		else if (((lhs.filImage.fattrib & AM_DIR) && (rhs.filImage.fattrib & AM_DIR)) || (!(lhs.filImage.fattrib & AM_DIR) && !(rhs.filImage.fattrib & AM_DIR)))
			return strcasecmp(lhs.filImage.fname, rhs.filImage.fname) < 0;
		else if ((lhs.filImage.fattrib & AM_DIR) && !(rhs.filImage.fattrib & AM_DIR))
			return true;
		else
			return false;
	}
};

void FileBrowser::RefreshDevicesEntries(std::vector<FileBrowser::BrowsableList::Entry>& entries, bool toLower)
{
	FileBrowser::BrowsableList::Entry entry;
	char label[1024];
	DWORD vsn;
	f_getlabel("SD:", label, &vsn);

	if (strlen(label) > 0)
		sprintf(entry.filImage.fname, "SD: %s", label);
	else
		sprintf(entry.filImage.fname, "SD:");
	if (toLower)
	{
		for (int i = 0; entry.filImage.fname[i]; i++)
		{
			entry.filImage.fname[i] = tolower(entry.filImage.fname[i]);
		}
	}
	entry.filImage.fattrib |= AM_DIR;
	entry.filIcon.fname[0] = 0;
	entries.push_back(entry);

	for (int USBDriveIndex = 0; USBDriveIndex < numberOfUSBMassStorageDevices; ++USBDriveIndex)
	{
		char USBDriveId[16];
		sprintf(USBDriveId, "USB%02d:", USBDriveIndex + 1);

		f_getlabel(USBDriveId, label, &vsn);

		if (strlen(label) > 0)
			sprintf(entry.filImage.fname, "%s %s", USBDriveId, label);
		else
			strcpy(entry.filImage.fname, USBDriveId);

		if (toLower)
		{
			for (int i = 0; entry.filImage.fname[i]; i++)
			{
				entry.filImage.fname[i] = tolower(entry.filImage.fname[i]);
			}
		}
		entry.filImage.fattrib |= AM_DIR;
		entry.filIcon.fname[0] = 0;
		entries.push_back(entry);
	}
}

void FileBrowser::RefreshFolderEntries()
{
	DIR dir;
	FileBrowser::BrowsableList::Entry entry;
	FRESULT res;
	char* ext;

	folder.Clear();
	if (displayingDevices)
	{
		FileBrowser::RefreshDevicesEntries(folder.entries, false);
	}
	else
	{
		res = f_opendir(&dir, ".");
		if (res == FR_OK)
		{
			do
			{
				res = f_readdir(&dir, &entry.filImage);
				ext = strrchr(entry.filImage.fname, '.');
				if (res == FR_OK && entry.filImage.fname[0] != 0 && !(ext && strcasecmp(ext, ".png") == 0) && (entry.filImage.fname[0] != '.'))
					folder.entries.push_back(entry);
			} while (res == FR_OK && entry.filImage.fname[0] != 0);
			f_closedir(&dir);

			// Now check for icons
			res = f_opendir(&dir, ".");
			if (res == FR_OK)
			{
				do
				{
					res = f_readdir(&dir, &entry.filIcon);
					ext = strrchr(entry.filIcon.fname, '.');
					if (ext)
					{
						int length = ext - entry.filIcon.fname;
						if (res == FR_OK && entry.filIcon.fname[0] != 0 && strcasecmp(ext, ".png") == 0)
						{
							for (unsigned index = 0; index < folder.entries.size(); ++index)
							{
								FileBrowser::BrowsableList::Entry* entryAtIndex = &folder.entries[index];
								if (strncasecmp(entry.filIcon.fname, entryAtIndex->filImage.fname, length) == 0)
									entryAtIndex->filIcon = entry.filIcon;
							}
						}
					}
				} while (res == FR_OK && entry.filIcon.fname[0] != 0);
			}
			f_closedir(&dir);

			strcpy(entry.filImage.fname, "..");
			entry.filImage.fattrib |= AM_DIR;
			entry.filIcon.fname[0] = 0;
			folder.entries.push_back(entry);

			std::sort(folder.entries.begin(), folder.entries.end(), greater());

			folder.currentIndex = 0;
			folder.SetCurrent();
		}
		else
		{
			//DEBUG_LOG("Cannot open dir");
		}
	}

	// incase they deleted something selected in the caddy
	caddySelections.Clear();
}

void FileBrowser::FolderChanged()
{
	RefreshFolderEntries();
	RefeshDisplay();
}

void FileBrowser::DisplayRoot()
{
	f_chdir("/1541");
	FolderChanged();
}

void FileBrowser::DeviceSwitched()
{
	displayingDevices = false;
	m_IEC_Commands.SetDisplayingDevices(displayingDevices);
	FolderChanged();
}
/*
void FileBrowser::RefeshDisplayForBrowsableList(FileBrowser::BrowsableList* browsableList, int xOffset, bool showSelected)
{
	char buffer1[128] = { 0 };
	char buffer2[128] = { 0 };
	u32 index;
	u32 entryIndex;
	u32 x = xOffset;
	u32 y = 17;
	u32 colour;
	bool terminal = false;
	RGBA BkColour = RGBA(0, 0, 0, 0xFF); //palette[VIC2_COLOUR_INDEX_BLUE];

	if (terminal)
		printf("\E[2J\E[f");

	u32 maxCharacters = screenMain->ScaleX(80);

	for (index = 0; index < maxOnScreen; ++index)
	{
		entryIndex = browsableList->offset + index;

		if (entryIndex < browsableList->entries.size())
		{
			FileBrowser::BrowsableList::Entry* entry = &browsableList->entries[entryIndex];
			snprintf(buffer2, maxCharacters + 1, "%s", entry->filImage.fname);
			memset(buffer1, ' ', maxCharacters);
			buffer1[127] = 0;
			strncpy(buffer1, buffer2, strlen(buffer2));
			if (showSelected && browsableList->currentIndex == entryIndex)
			{
				if (entry->filImage.fattrib & AM_DIR)
				{
					if (terminal)
						printf("\E[34;47m%s\E[0m\r\n", buffer1);
					screenMain->PrintText(false, x, y, buffer1, palette[VIC2_COLOUR_INDEX_LBLUE], RGBA(0xff, 0xff, 0xff, 0xff));
				}
				else
				{
					colour = RGBA(0xff, 0, 0, 0xff);
					if (entry->filImage.fattrib & AM_RDO)
						colour = palette[VIC2_COLOUR_INDEX_RED];

					screenMain->PrintText(false, x, y, buffer1, colour, RGBA(0xff, 0xff, 0xff, 0xff));
					if (terminal)
						printf("\E[31;47m%s\E[0m\r\n", buffer1);
				}
			}
			else
			{
				if (entry->filImage.fattrib & AM_DIR)
				{
					screenMain->PrintText(false, x, y, buffer1, palette[VIC2_COLOUR_INDEX_LBLUE], BkColour);
					if (terminal)
						printf("\E[34m%s\E[0m\r\n", buffer1);
				}
				else
				{
					colour = palette[VIC2_COLOUR_INDEX_LGREY];
					if (entry->filImage.fattrib & AM_RDO)
						colour = palette[VIC2_COLOUR_INDEX_PINK];
					screenMain->PrintText(false, x, y, buffer1, colour, BkColour);
					if (terminal)
						printf("\E[0;m%s\E[0m\r\n", buffer1);
				}
			}
		}
		else
		{
			memset(buffer1, ' ', 80);
			screenMain->PrintText(false, x, y, buffer1, BkColour, BkColour);
			if (terminal)
				printf("%s\r\n", buffer1);
		}
		y += 16;
	}
}
*/
void FileBrowser::RefeshDisplay()
{
#if not defined(EXPERIMENTALZERO)
	u32 textColour = Colour(VIC2_COLOUR_INDEX_LGREEN);
	u32 bgColour = Colour(VIC2_COLOUR_INDEX_GREY);
	char buffer[1024];
	if (f_getcwd(buffer, 1024) == FR_OK)
	{
		screenMain->DrawRectangle(0, 0, (int)screenMain->Width(), 17, bgColour);
		screenMain->PrintText(false, 0, 0, buffer, textColour, bgColour);
	}
	//u32 offsetX = screenMain->ScaleX(1024 - 320);
	//RefeshDisplayForBrowsableList(&folder, 0);
	//RefeshDisplayForBrowsableList(&caddySelections, offsetX, false);
	folder.RefreshViews();
	caddySelections.RefreshViews();

	DisplayPNG();
	DisplayStatusBar();

	if (folder.searchPrefixIndex > 0)
	{
		u32 y = screenMain->ScaleY(STATUS_BAR_POSITION_Y);
		screenMain->PrintText(false, 0, y, folder.searchPrefix, textColour, bgColour);
	}
#else
	folder.RefreshViews();
	caddySelections.RefreshViews();
#endif
}

bool FileBrowser::CheckForPNG(const char* filename, FILINFO& filIcon)
{
	bool foundValid = false;

	filIcon.fname[0] = 0;

	if (DiskImage::IsDiskImageExtention(filename))
	{
		char fileName[256];
		char* ptr = strrchr(filename, '.');
		if (ptr)
		{
			int len = ptr - filename;
			strncpy(fileName, filename, len);
			fileName[len] = 0;

			strcat(fileName, ".png");

			if (f_stat(fileName, &filIcon) == FR_OK)
				foundValid = true;
		}
	}
	return foundValid;
}

void FileBrowser::DisplayPNG(FILINFO& filIcon, int x, int y)
{
	if (filIcon.fname[0] != 0)
	{
		FIL fp;
		FRESULT res;

		res = f_open(&fp, filIcon.fname, FA_READ);
		if (res == FR_OK)
		{
			char* PNG = (char*)malloc(filIcon.fsize);
			if (PNG)
			{
				u32 bytesRead;
				SetACTLed(true);
				f_read(&fp, PNG, filIcon.fsize, &bytesRead);
				SetACTLed(false);
				f_close(&fp);

				int w;
				int h;
				int channels_in_file;
				stbi_uc* image = stbi_load_from_memory((stbi_uc const*)PNG, bytesRead, &w, &h, &channels_in_file, 4);
#if not defined(EXPERIMENTALZERO)

				if (image && (w == PNG_WIDTH && h == PNG_HEIGHT))
				{
					//DEBUG_LOG("Opened PNG %s w = %d h = %d cif = %d\r\n", fileName, w, h, channels_in_file);
					screenMain->PlotImage((u32*)image, x, y, w, h);
				}
				else
				{
					//DEBUG_LOG("Invalid PNG size %d x %d\r\n", w, h);
				}
#endif
				free(PNG);
			}
		}
	}
	else
	{
		//DEBUG_LOG("Cannot find PNG %s\r\n", fileName);
	}
}

void FileBrowser::DisplayPNG()
{
#if not defined(EXPERIMENTALZERO)
	if (displayPNGIcons && folder.current)
	{
		FileBrowser::BrowsableList::Entry* current = folder.current;
		u32 x = screenMain->ScaleX(1024) - PNG_WIDTH;
		u32 y = screenMain->ScaleY(616) - PNG_HEIGHT;
		DisplayPNG(current->filIcon, x, y);
	}
#endif
}

int FileBrowser::IsAtRootOfDevice()
{
	char buffer[1024];
	if (f_getcwd(buffer, 1024) == FR_OK)
	{
		if (strcmp("SD:/", buffer) == 0)
			return 0;
		for (int USBDriveIndex = 0; USBDriveIndex < numberOfUSBMassStorageDevices; ++USBDriveIndex)
		{
			char USBDriveId[16];
			sprintf(USBDriveId, "USB%02d:/", USBDriveIndex + 1);
			if (strcmp(USBDriveId, buffer) == 0)
				return USBDriveIndex + 1;
		}
	}
	return -1;
}

void FileBrowser::PopFolder()
{
	char buffer[1024];
	if (f_getcwd(buffer, 1024) == FR_OK)
	{
		int deviceRoot = IsAtRootOfDevice();
		DEBUG_LOG("deviceRoot = %d\r\n", deviceRoot);
		if (deviceRoot >= 0)
		{
			displayingDevices = true;
			m_IEC_Commands.SetDisplayingDevices(displayingDevices);
			RefreshFolderEntries();
			folder.currentIndex = deviceRoot;
			folder.SetCurrent();
		}
		else
		{
			// find the last '/' of the current dir
			char *last_ptr = 0;
			char *ptr = strtok(buffer, "/");
			while (ptr != NULL)
			{
				last_ptr = ptr;
				ptr = strtok(NULL, "/");
			}
			f_chdir("..");
			RefreshFolderEntries();

			unsigned found = 0;
			if (last_ptr)
			{
				u32 numberOfEntriesMinus1 = folder.entries.size() - 1;
				for (unsigned i = 0; i <= numberOfEntriesMinus1; i++)
				{
					FileBrowser::BrowsableList::Entry* entry = &folder.entries[i];
					if (strcmp(last_ptr, entry->filImage.fname) == 0)
					{
						found = i;
						break;
					}
				}
			}
			if (found)
			{
				folder.currentIndex = found;
				folder.SetCurrent();
			}
		}
		RefeshDisplay();
	}
}

void FileBrowser::UpdateCurrentHighlight()
{
	if (folder.entries.size() > 0)
	{
		FileBrowser::BrowsableList::Entry* current = folder.current;
		if (current && folder.currentHighlightTime > 0)
		{
			folder.currentHighlightTime -= 0.000001f;

			if (folder.currentHighlightTime <= 0)
			{
				folder.RefreshViewsHighlightScroll();
			}

			if (folder.currentHighlightTime <= 0)
			{
				folder.currentHighlightTime = scrollHighlightRate;
			}

		}
	}

	if (folder.entries.size() > 0)
	{
		FileBrowser::BrowsableList::Entry* current = caddySelections.current;
		
		if (current && caddySelections.currentHighlightTime > 0)
		{
			caddySelections.currentHighlightTime -= 0.000001f;

			if (caddySelections.currentHighlightTime <= 0)
			{
				caddySelections.RefreshViewsHighlightScroll();
			}

			if (caddySelections.currentHighlightTime <= 0)
			{
				caddySelections.currentHighlightTime = scrollHighlightRate;
			}

		}
	}
}

void FileBrowser::Update()
{
	if ( inputMappings->CheckKeyboardBrowseMode() || inputMappings->CheckButtonsBrowseMode() || (folder.searchPrefixIndex != 0) )
		UpdateInputFolders();

	UpdateCurrentHighlight();
}

bool FileBrowser::FillCaddyWithSelections()
{
	if (caddySelections.entries.size())
	{
		for (auto it = caddySelections.entries.begin(); it != caddySelections.entries.end();)
		{
			bool readOnly = ((*it).filImage.fattrib & AM_RDO) != 0;
			if (diskCaddy->Insert(&(*it).filImage, readOnly) == false)
				caddySelections.entries.erase(it);
			else
				it++;
		}

		return true;
	}
	return false;
}

bool FileBrowser::AddToCaddy(FileBrowser::BrowsableList::Entry* current)
{
	if (!current) return false;

	else if (!(current->filImage.fattrib & AM_DIR) && DiskImage::IsDiskImageExtention(current->filImage.fname))
	{
		return AddImageToCaddy(current);
	}

	else if ( (current->filImage.fattrib & AM_DIR) && ( strcmp(current->filImage.fname, "..") != 0) )
	{
		bool ret = false;
		f_chdir(current->filImage.fname);
		RefreshFolderEntries();
		RefeshDisplay();

		for (unsigned i = 0; i < folder.entries.size(); ++i)
			ret |= AddImageToCaddy(&folder.entries[i]);

		folder.currentIndex = folder.entries.size() - 1;
		folder.SetCurrent();

		RefeshDisplay();
		return ret;
	}
	return false;

}

bool FileBrowser::AddImageToCaddy(FileBrowser::BrowsableList::Entry* current)
{
	bool added = false;

	if (current && !(current->filImage.fattrib & AM_DIR) && DiskImage::IsDiskImageExtention(current->filImage.fname))
	{
		bool canAdd = true;
		unsigned i;
		for (i = 0; i < caddySelections.entries.size(); ++i)
		{
			if (strcmp(current->filImage.fname, caddySelections.entries[i].filImage.fname) == 0)
			{
				canAdd = false;
				break;
			}
		}
		if (canAdd)
		{
			current->caddyIndex = caddySelections.entries.size();
			caddySelections.entries.push_back(*current);
			added = true;
		}
	}
	return added;
}

void FileBrowser::UpdateInputFolders()
{
	bool dirty = false;

	if (inputMappings->BrowseFunction())
	{
		// check for ROM and Drive Number changes
		unsigned ROMOrDevice = inputMappings->getROMOrDevice();
		if ( ROMOrDevice >= 1 && ROMOrDevice <= 11 )
			SelectROMOrDevice(ROMOrDevice);
	}
	else if (inputMappings->BrowseSelect())
	{
		FileBrowser::BrowsableList::Entry* current = folder.current;
		if (current)
		{
			if (displayingDevices)
			{
				if (strncmp(current->filImage.fname, "SD", 2) == 0)
				{
					SwitchDrive("SD:");
					displayingDevices = false;
					m_IEC_Commands.SetDisplayingDevices(displayingDevices);
					RefreshFolderEntries();
				}
				else
				{
					for (int USBDriveIndex = 0; USBDriveIndex < numberOfUSBMassStorageDevices; ++USBDriveIndex)
					{
						char USBDriveId[16];
						sprintf(USBDriveId, "USB%02d:", USBDriveIndex + 1);

						if (strncmp(current->filImage.fname, USBDriveId, 5) == 0)
						{
							SwitchDrive(USBDriveId);
							displayingDevices = false;
							m_IEC_Commands.SetDisplayingDevices(displayingDevices);
							RefreshFolderEntries();
						}
					}
				}
				dirty = true;
			}
			else if (current->filImage.fattrib & AM_DIR)
			{
				if (strcmp(current->filImage.fname, "..") == 0)
				{
					PopFolder();
				}
				else if (strcmp(current->filImage.fname, ".") != 0)
				{
					f_chdir(current->filImage.fname);
					RefreshFolderEntries();
				}
				dirty = true;
			}
			else // not a directory
			{
				if (DiskImage::IsDiskImageExtention(current->filImage.fname))
				{
					DiskImage::DiskType diskType = DiskImage::GetDiskImageTypeViaExtention(current->filImage.fname);

					// Should also be able to create a LST file from all the images currently selected in the caddy
					if (diskType == DiskImage::LST)
					{
						selectionsMade = SelectLST(current->filImage.fname);
					}
					else
					{
						// Add the current selected
						AddToCaddy(current);
						selectionsMade = FillCaddyWithSelections();
					}

					if (selectionsMade)
						lastSelectionName = current->filImage.fname;

					dirty = true;
				}
			}
		}
	}
	else if (inputMappings->BrowseDone())
	{
		selectionsMade = FillCaddyWithSelections();
	}
	else if (inputMappings->BrowseBack())
	{
		PopFolder();
		dirty = true;
	}
	else if (inputMappings->Exit())
	{
		ClearSelections();
		dirty = true;
	}
	else if (inputMappings->BrowseInsert())
	{
		FileBrowser::BrowsableList::Entry* current = folder.current;
		if (current)
		{
			dirty = AddToCaddy(current);
		}
	}
	else if (inputMappings->BrowseNewD64())
	{
		char newFileName[64];
		strncpy (newFileName, options.GetAutoBaseName(), 63);
		int num = folder.FindNextAutoName( newFileName );
		m_IEC_Commands.CreateNewDisk(newFileName, "42", true);
		FolderChanged();
	}
	else if (inputMappings->BrowseWriteProtect())
	{
		FileBrowser::BrowsableList::Entry* current = folder.current;
		if (current)
		{
			if (current->filImage.fattrib & AM_RDO)
			{
				current->filImage.fattrib &= ~AM_RDO;
				f_chmod(current->filImage.fname, 0, AM_RDO);
			}
			else
			{
				current->filImage.fattrib |= AM_RDO;
				f_chmod(current->filImage.fname, AM_RDO, AM_RDO);
			}
			dirty = true;
		}
	}
	else if (inputMappings->BrowseAutoLoad())
	{
		CheckAutoMountImage(EXIT_RESET, this);
	}
	else if (inputMappings->MakeLSTFile())
	{
		MakeLST("autoswap.lst");
		FolderChanged();
		FileBrowser::BrowsableList::Entry* current = 0;
		for (unsigned index = 0; index < folder.entries.size(); ++index)
		{
			current = &folder.entries[index];
			if (strcasecmp(current->filImage.fname, "autoswap.lst") == 0)
			{
				folder.currentIndex = index;
				folder.SetCurrent();
				dirty=true;
				break;
			}
		}
	}
	else
	{
		dirty = folder.CheckBrowseNavigation();
	}

	if (dirty) RefeshDisplay();
}

bool FileBrowser::SelectROMOrDevice(u32 index)
// 1-7 select ROM image
// 8-11 change deviceID to  8-11
{
	if ( (index >= 8) && (index <= 11 ) )
	{
		GlobalSetDeviceID( index );
		ShowDeviceAndROM();
		return true;
	}
	index--;
	if ((index < ROMs::MAX_ROMS) && (roms->ROMValid[index]))
	{
		roms->currentROMIndex = index;
		roms->lastManualSelectedROMIndex = index;
		DEBUG_LOG("Swap ROM %d %s\r\n", index, roms->ROMNames[index]);
		ShowDeviceAndROM();
		return true;
	}
	return false;
}


bool FileBrowser::MakeLST(const char* filenameLST)
{
	bool retcode=true;
	FIL fp;
	FRESULT res;
	res = f_open(&fp, filenameLST,  FA_CREATE_ALWAYS | FA_WRITE);
	if (res == FR_OK)
	{
		FileBrowser::BrowsableList::Entry* entry = 0;
		u32 bytes;

		BrowsableList& list = caddySelections.entries.size() > 1 ? caddySelections : folder;

		for (unsigned index = 0; index < list.entries.size(); ++index)
		{
			entry = &list.entries[index];
			if (entry->filImage.fattrib & AM_DIR)
				continue;	// skip dirs

			if ( DiskImage::IsDiskImageExtention(entry->filImage.fname)
				&& !DiskImage::IsLSTExtention(entry->filImage.fname) )
			{
				f_write(&fp,
					entry->filImage.fname,
					strlen(entry->filImage.fname),
					&bytes);
				f_write(&fp, "\r\n", 2, &bytes);
			}
		}

		f_write(&fp
			, roms->ROMNames[roms->currentROMIndex]
			, strlen(roms->ROMNames[roms->currentROMIndex])
			, &bytes);
		f_write(&fp, "\r\n", 2, &bytes);

		f_close(&fp);
	}
	else
		retcode=false;

	return retcode;
}

bool FileBrowser::SelectLST(const char* filenameLST)
{
	bool validImage = false;
	//DEBUG_LOG("Selected %s\r\n", filenameLST);
	if (DiskImage::IsLSTExtention(filenameLST))
	{
		DiskImage::DiskType diskType;
		FIL fp;
		FRESULT res;
		res = f_open(&fp, filenameLST, FA_READ);
		if (res == FR_OK)
		{
			u32 bytesRead;
			SetACTLed(true);
			f_read(&fp, FileBrowser::LSTBuffer, FileBrowser::LSTBuffer_size, &bytesRead);
			SetACTLed(false);
			f_close(&fp);

			TextParser textParser;

			textParser.SetData((char*)FileBrowser::LSTBuffer);
			char* token = textParser.GetToken(true);
			while (token)
			{
				//DEBUG_LOG("LST token = %s\r\n", token);
				diskType = DiskImage::GetDiskImageTypeViaExtention(token);
				if (diskType == DiskImage::D64 || diskType == DiskImage::G64 || diskType == DiskImage::NIB || diskType == DiskImage::NBZ || diskType == DiskImage::T64)
				{
					FileBrowser::BrowsableList::Entry* entry = folder.FindEntry(token);
					if (entry && !(entry->filImage.fattrib & AM_DIR))
					{
						bool readOnly = (entry->filImage.fattrib & AM_RDO) != 0;
						if (diskCaddy->Insert(&entry->filImage, readOnly))
							validImage = true;
					}
				}
				else
				{
					roms->SelectROM(token);
				}
				token = textParser.GetToken(true);
			}
		}
	}
	return validImage;
}

/*
// Not used
void FileBrowser::UpdateInputDiskCaddy()
{
	bool dirty = false;

	if (keyboard->KeyPressed(KEY_DELETE))
	{
	}
	else if (keyboard->KeyPressed(KEY_TAB))
	{
		state = State_Folders;
		dirty = true;
	}
	else
	{
		if (keyboard->KeyHeld(KEY_DOWN))
		{
		}
		if (keyboard->KeyHeld(KEY_UP))
		{
		}
	}

	if (dirty) RefeshDisplay();
}
*/

void FileBrowser::DisplayStatusBar()
{
#if not defined(EXPERIMENTALZERO)
	u32 x = 0;
	u32 y = screenMain->ScaleY(STATUS_BAR_POSITION_Y);

	char bufferOut[128];
	if (options.DisplayTemperature())
		snprintf(bufferOut, 128, "LED 0 Motor 0 Track 18.0 ATN 0 DAT 0 CLK 0 00%cC", 248);
	else
		snprintf(bufferOut, 128, "LED 0 Motor 0 Track 18.0 ATN 0 DAT 0 CLK 0");

	screenMain->PrintText(false, x, y, bufferOut, RGBA(0, 0, 0, 0xff), RGBA(0xff, 0xff, 0xff, 0xff));
#endif
}

void FileBrowser::ClearScreen()
{
#if not defined(EXPERIMENTALZERO)
	u32 bgColour = palette[VIC2_COLOUR_INDEX_BLUE];
	screenMain->Clear(bgColour);
#endif
}

void FileBrowser::ClearSelections()
{
	selectionsMade = false;
	caddySelections.Clear();

	folder.ClearSelections();
}

void FileBrowser::ShowDeviceAndROM()
{
	ShowDeviceAndROM(roms->GetSelectedROMName());
}

void FileBrowser::ShowDeviceAndROM( const char* ROMName )
{
	char buffer[256];
	u32 textColour = RGBA(0, 0, 0, 0xff);
	u32 bgColour = RGBA(0xff, 0xff, 0xff, 0xff);
	u32 x = 0; // 43 * 8
	u32 y;

#if not defined(EXPERIMENTALZERO)
	y = screenMain->ScaleY(STATUS_BAR_POSITION_Y) - 20;

	snprintf(buffer, 256, "Device %2d %*s"
		, *deviceID
		, roms->GetLongestRomNameLen()
		, ROMName
		);

	screenMain->PrintText(false, x, y, buffer, textColour, bgColour);
#endif
	if (screenLCD)
	{
		x = 0;
		y = 0;

		snprintf(buffer, 256, "D%2d %s"
			, *deviceID
			, ROMName
			);
		screenLCD->PrintText(false, x, y, buffer, textColour, bgColour);
		screenLCD->SwapBuffers();
	}
}

void FileBrowser::DisplayDiskInfo(DiskImage* diskImage, const char* filenameForIcon)
{
#if not defined(EXPERIMENTALZERO)
	// Ideally we should not have to load the entire disk to read the directory.
	static const char* fileTypes[]=
	{
		"DEL", "SEQ", "PRG", "USR", "REL", "UKN", "UKN", "UKN"
	};
	// Decode the BAM
	unsigned track = 18;
	unsigned sectorNo = 0;
	char name[17] = { 0 };
	unsigned char buffer[260] = { 0 };
	int charIndex;
	u32 fontHeight = screenMain->GetFontHeightDirectoryDisplay();
	u32 x = 0;
	u32 y = 0;
	char bufferOut[128] = { 0 };
	u32 textColour = palette[VIC2_COLOUR_INDEX_LBLUE];
	u32 bgColour = palette[VIC2_COLOUR_INDEX_BLUE];

	u32 usedColour = palette[VIC2_COLOUR_INDEX_RED];
	u32 freeColour = palette[VIC2_COLOUR_INDEX_LGREEN];
	u32 thisColour = 0;
	u32 BAMOffsetX = screenMain->ScaleX(400);

	u32 bmBAMOffsetX = screenMain->ScaleX(1024) - PNG_WIDTH;
	u32 x_px = 0;
	u32 y_px = 0;

	ClearScreen();

	if (options.DisplayTracks())
	{
		for (track = 0; track < HALF_TRACK_COUNT; track += 2)
		{
			int yoffset = screenMain->ScaleY(400);
			unsigned index;
			unsigned length = diskImage->TrackLength(track);
			unsigned countSync = 0;

			u8 shiftReg = 0;
			for (index = 0; index < length / 8; ++index)
			{
				RGBA colour;
				unsigned count1s = 0;
				bool sync = false;

				int bit;

				for (bit = 0; bit < 64; ++bit)
				{
					if (bit % 8 == 0)
						shiftReg = diskImage->GetNextByte(track, index * 8 + bit / 8);

					if (shiftReg & 0x80)
					{
						count1s++;
						countSync++;
						if (countSync == 10)
							sync = true;
					}
					else
					{
						countSync = 0;
					}
					shiftReg <<= 1;
				}

				if (sync)
					colour = RGBA(0xff, 0x00, 0x00, 0xFF);
				else
					colour = RGBA(0x00, (unsigned char)((float)count1s / 64.0f * 255.0f), 0x00, 0xFF);

				screenMain->DrawRectangle(index, (track >> 1) * 4 + yoffset, index + 1, (track >> 1) * 4 + 4 + yoffset, colour);
			}
		}
	}

	track = 18;

	if (diskImage->GetDecodedSector(track, sectorNo, buffer))
	{
		track = buffer[0];
		sectorNo = buffer[1];

		//144-161 ($90-Al) Name of the disk (padded with "shift space") 
		//162,163 ($A2,$A3) Disk ID marker 
		//164 ($A4) $A0 Shift Space
		//165,166 ($A5,$A6) $32,$41 ASCII chars "2A" DOS indicator
		//167-170 ($A7-$AA) $A0 Shift Space
		//171-255 ($AB-$FF) $00 Not used, filled with zero (The bytes 180 to 191 can have the contents "blocks free" on many disks.)
		//AB-FF: Normally unused ($00), except for 40 track extended format,
		//	see the following two entries:
		//AC-BF: DOLPHIN DOS track 36-40 BAM entries (only for 40 track)
		//C0-D3: SPEED DOS track 36-40 BAM entries (only for 40 track)

		strncpy(name, (char*)&buffer[144], 16);

		int blocksFree = 0;
		int bamTrack;
		int lastTrackUsed = (int)diskImage->LastTrackUsed() >> 1;	// 0..34 (or 39)
		int bamOffset = BAM_OFFSET;
		int guess40 = 0;

		x_px = bmBAMOffsetX;
		int x_size = PNG_WIDTH/lastTrackUsed;
		int y_size = PNG_HEIGHT/21;

// try to guess the 40 track format
		if (lastTrackUsed == 39)
		{
			int dolphin_sum = 0;
			int speeddos_sum = 0;
			for (int i=0; i<20; i++)
			{
				dolphin_sum += buffer[0xac+i];
				speeddos_sum += buffer[0xc0+i];
			}
			if ( dolphin_sum == 0 && speeddos_sum != 0)
				guess40 = 0xc0;
			if ( dolphin_sum != 0 && speeddos_sum == 0)
				guess40 = 0xac;

// debugging
//snprintf(bufferOut, 128, "LTU %d dd %d sd %d g40 %d", lastTrackUsed, dolphin_sum, speeddos_sum, guess40);
//screenMain->PrintText(false, x_px, PNG_HEIGHT+30, bufferOut, textColour, bgColour);
		}


		for (bamTrack = 0; bamTrack <= lastTrackUsed; ++bamTrack)
		{
			if (bamTrack >= 35)
				bamOffset = guess40;
			else
				bamOffset = BAM_OFFSET;

			if (bamOffset && (bamTrack + 1) != 18)
				blocksFree += buffer[bamOffset + bamTrack * BAM_ENTRY_SIZE];

			y_px = 0;
			for (int bit = 0; bit < DiskImage::SectorsPerTrack[bamTrack]; bit++)
			{
				u32 bits = buffer[bamOffset + 1 + (bit >> 3) + bamTrack * BAM_ENTRY_SIZE];

				if (!guess40 && bamTrack>= 35)
					thisColour = 0;
				else if (bits & (1 << (bit & 0x7)))
					thisColour = freeColour;
				else
					thisColour = usedColour;

// highight track 18
				if ((bamTrack + 1) == 18)
					screenMain->DrawRectangle(x_px, y_px, x_px+x_size, y_px+y_size, textColour);

				screenMain->DrawRectangle(x_px+1, y_px+1, x_px+x_size-1, y_px+y_size-1, thisColour);

				y_px += y_size;
				bits <<= 1;
			}
			x_px += x_size;
		}

		x = 0;
		y = 0;
		snprintf(bufferOut, 128, "0");
		screenMain->PrintText(true, x, y, bufferOut, textColour, bgColour);
		x = 16;
		snprintf(bufferOut, 128, "\"%s\" %c%c%c%c%c%c", name, buffer[162], buffer[163], buffer[164], buffer[165], buffer[166], buffer[167]);
		screenMain->PrintText(true, x, y, bufferOut, bgColour, textColour);
		x = 0;
		y += fontHeight;

		if (track != 0)
		{
			unsigned trackPrev = 0xff;
			unsigned sectorPrev = 0xff;
			bool complete = false;
			// Blocks 1 through 19 on track 18 contain the file entries. The first two bytes of a block point to the next directory block with file entries. If no more directory blocks follow, these bytes contain $00 and $FF, respectively.
			while (!complete)
			{
				//DEBUG_LOG("track %d sector %d\r\n", track, sectorNo);
				if (diskImage->GetDecodedSector(track, sectorNo, buffer))
				{
					unsigned trackNext = buffer[0];
					unsigned sectorNoNext = buffer[1];

					complete = (track == trackNext) && (sectorNo == sectorNoNext);	// Detect looping directory entries (raid over moscow ntsc)
					complete |= (trackNext == trackPrev) && (sectorNoNext == sectorPrev);	// Detect looping directory entries (IndustrialBreakdown)
					complete |= (trackNext == 00) || (sectorNoNext == 0xff);
					complete |= (trackNext == 18) && (sectorNoNext == 1);
					trackPrev = track;
					sectorPrev = sectorNo;
					track = trackNext;
					sectorNo = sectorNoNext;

					int entry;
					int entryOffset = 2;
					for (entry = 0; entry < 8; ++entry)
					{
						bool done = true;
						for (int i = 0; i < 0x1d; ++i)
						{
							if (buffer[i + entryOffset])
								done = false;
						}

						if (!done)
						{
							u8 fileType = buffer[DIR_ENTRY_OFFSET_TYPE + entryOffset];
							u16 blocks = (buffer[DIR_ENTRY_OFFSET_BLOCKS + entryOffset + 1] << 8) | buffer[DIR_ENTRY_OFFSET_BLOCKS + entryOffset];

							if (fileType != 0) { // hide scratched files
								x = 0;
								for (charIndex = 0; charIndex < DIR_ENTRY_NAME_LENGTH; ++charIndex)
								{
									char c = buffer[DIR_ENTRY_OFFSET_NAME + entryOffset + charIndex];
									if (c == 0xa0) c = 0x20;
									name[charIndex] = c;
								}
								name[charIndex] = 0;

								//DEBUG_LOG("%d name = %s %x\r\n", blocks, name, fileType);
								snprintf(bufferOut, 128, "%d", blocks);
								screenMain->PrintText(true, x, y, bufferOut, textColour, bgColour);
								x += 5 * 8;
								snprintf(bufferOut, 128, "\"%s\"", name);
								screenMain->PrintText(true, x, y, bufferOut, textColour, bgColour);
								x += 19 * 8;
								char modifier = 0x20;
								if ((fileType & 0x80) == 0)
									modifier = screen2petscii(42);
								else if (fileType & 0x40)
									modifier = screen2petscii(60);
								snprintf(bufferOut, 128, "%s%c", fileTypes[fileType & 7], modifier);
								screenMain->PrintText(true, x, y, bufferOut, textColour, bgColour);
								y += fontHeight;
							}
						}
						entryOffset += 32;
					}
				}
				else
				{
					// Error, just abort
					complete = true;
				}
			}
		}
		x = 0;
		//DEBUG_LOG("%d blocks free\r\n", blocksFree);
		snprintf(bufferOut, 128, "%d BLOCKS FREE.\r\n", blocksFree);
		screenMain->PrintText(true, x, y, bufferOut, textColour, bgColour);
		y += fontHeight;
	}

	DisplayStatusBar();

	if (filenameForIcon)
	{
		FILINFO filIcon;
		if (CheckForPNG(filenameForIcon, filIcon))
		{
			x = screenMain->ScaleX(1024) - 320;
			y = screenMain->ScaleY(0);
			DisplayPNG(filIcon, x, y);
		}
	}
#endif
}

void FileBrowser::SelectAutoMountImage(const char* image)
{
	f_chdir("/1541");
	RefreshFolderEntries();

	if (SelectLST(image))
	{
		selectionsMade = true;
	}
	else
	{
		FileBrowser::BrowsableList::Entry* current = 0;
		int index;
		int maxEntries = folder.entries.size();

		for (index = 0; index < maxEntries; ++index)
		{
			current = &folder.entries[index];
			if (strcasecmp(current->filImage.fname, image) == 0)
			{
				break;
			}
		}

		if (index != maxEntries)
		{
			ClearSelections();
			caddySelections.entries.push_back(*current);
			selectionsMade = FillCaddyWithSelections();
		}
	}
}

int FileBrowser::BrowsableList::FindNextAutoName(char* filename)
{
	int index;
	int len = (int)entries.size();

	int inputlen = strlen(filename);
	int lastNumber = 0;

	char scanfname[64];
	strncpy (scanfname, filename, 54);
	strncat (scanfname, "%d",2);

	int foundnumber;

	for (index = 0; index < len; ++index)
	{
		Entry* entry = &entries[index];
		if (	!(entry->filImage.fattrib & AM_DIR) 
			&& strncasecmp(filename, entry->filImage.fname, inputlen) == 0
			&& sscanf(entry->filImage.fname, scanfname, &foundnumber) == 1
			)
		{
			if (foundnumber > lastNumber)
				lastNumber = foundnumber;
		}
	}
	snprintf(filename + inputlen, 54, "%03d.d64", lastNumber+1);
	return lastNumber+1;
}
