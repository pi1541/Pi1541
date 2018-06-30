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
#include "debug.h"
#include "Keyboard.h"
#include "options.h"
#include "InputMappings.h"
#include "stb_image.h"
#include "Petscii.h"
extern "C"
{
#include "rpi-gpio.h"
}

#define PNG_WIDTH 320
#define PNG_HEIGHT 200


unsigned char FileBrowser::LSTBuffer[FileBrowser::LSTBuffer_size];

const unsigned FileBrowser::SwapKeys[30] =
{
	KEY_F1, KEY_KP1, KEY_1,
	KEY_F2, KEY_KP2, KEY_2,
	KEY_F3, KEY_KP3, KEY_3,
	KEY_F4, KEY_KP4, KEY_4,
	KEY_F5, KEY_KP5, KEY_5,
	KEY_F6, KEY_KP6, KEY_6,
	KEY_F7, KEY_KP7, KEY_7,
	KEY_F8, KEY_KP8, KEY_8,
	KEY_F9, KEY_KP9, KEY_9,
	KEY_F10, KEY_KP0, KEY_0
};

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

void FileBrowser::BrowsableListView::Refresh()
{
	char buffer1[128] = { 0 };
	char buffer2[128] = { 0 };
	u32 index;
	u32 entryIndex;
	u32 x = positionX;
	u32 y = positionY;
	u32 colour;
	RGBA BkColour = RGBA(0, 0, 0, 0xFF); //palette[VIC2_COLOUR_INDEX_BLUE];

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

		if (entryIndex < list->entries.size())
		{
			FileBrowser::BrowsableList::Entry* entry = &list->entries[entryIndex];
			if (screen->IsMonocrome())
			{
				if (entry->filImage.fattrib & AM_DIR)
				{
					snprintf(buffer2, columns + 1, "[%s]", entry->filImage.fname);
				}
				else
				{
					if (entry->caddyIndex != -1)
						snprintf(buffer2, columns + 1, "%d>%s", entry->caddyIndex, entry->filImage.fname);
					else
						snprintf(buffer2, columns + 1, "%s", entry->filImage.fname);
				}
			}
			else
			{
				snprintf(buffer2, columns + 1, "%s", entry->filImage.fname);
			}
			memset(buffer1, ' ', columns);
			buffer1[127] = 0;
			strncpy(buffer1, buffer2, strlen(buffer2));
			if (/*showSelected && */list->currentIndex == entryIndex)
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
		else
		{
			memset(buffer1, ' ', 80);
			screen->PrintText(false, x, y, buffer1, BkColour, BkColour);
		}
		y += 16;
	}

	screen->SwapBuffers();
}

bool FileBrowser::BrowsableListView::CheckBrowseNavigation(bool pageOnly)
{
	InputMappings* inputMappings = InputMappings::Instance();
	bool dirty = false;
	u32 numberOfEntriesMinus1 = list->entries.size() - 1;

	if (inputMappings->BrowseDown())
	{
		if (list->currentIndex < numberOfEntriesMinus1)
		{
			if (!pageOnly)
			{
				list->currentIndex++;
				list->current = &list->entries[list->currentIndex];
			}
			if (list->currentIndex >= (offset + rows) && (list->currentIndex < list->entries.size()))
				offset++;
			dirty = true;
		}
	}
	if (inputMappings->BrowseUp())
	{
		if (list->currentIndex > 0)
		{
			if (!pageOnly)
			{
				list->currentIndex--;
				list->current = &list->entries[list->currentIndex];
			}
			if ((offset > 0) && (list->currentIndex < offset))
				offset--;
			dirty = true;
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
		list->current = &list->entries[list->currentIndex];
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
		list->current = &list->entries[list->currentIndex];
		dirty = true;
	}

	return dirty;
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

bool FileBrowser::BrowsableList::CheckBrowseNavigation()
{
	bool dirty = false;
	u32 index;
	for (index = 0; index < views.size(); ++index)
	{
		dirty |= views[index].CheckBrowseNavigation(index != 0);
	}
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

FileBrowser::FileBrowser(DiskCaddy* diskCaddy, ROMs* roms, unsigned deviceID, bool displayPNGIcons, ScreenBase* screenMain, ScreenBase* screenLCD)
	: state(State_Folders)
	, diskCaddy(diskCaddy)
	, selectionsMade(false)
	, roms(roms)
	, deviceID(deviceID)
	, displayPNGIcons(displayPNGIcons)
	, screenMain(screenMain)
	, screenLCD(screenLCD)
{
	u32 columns = screenMain->ScaleX(80);
	u32 rows = (int)(38.0f * screenMain->GetScaleY());
	u32 positionX = 0;
	u32 positionY = 17;

	if (rows < 1)
		rows = 1;

	folder.AddView(screenMain, columns, rows, positionX, positionY, false);

	positionX = screenMain->ScaleX(1024 - 320);
	caddySelections.AddView(screenMain, columns, rows, positionX, positionY, false);



	columns = 128 / 8;
	rows = 4;
	positionX = 0;
	positionY = 0;

	if (screenLCD)
		folder.AddView(screenLCD, columns, rows, positionX, positionY, true);
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

void FileBrowser::RefreshFolderEntries()
{
	DIR dir;
	FileBrowser::BrowsableList::Entry entry;
	FRESULT res;
	char* ext;

	folder.Clear();
	res = f_opendir(&dir, ".");
	if (res == FR_OK)
	{
		do 
		{
			res = f_readdir(&dir, &entry.filImage);
			ext = strrchr(entry.filImage.fname, '.');
			if (res == FR_OK && entry.filImage.fname[0] != 0 && !(ext && strcasecmp(ext, ".png") == 0))
				folder.entries.push_back(entry);
		}
		while (res == FR_OK && entry.filImage.fname[0] != 0);
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
			}
			while (res == FR_OK && entry.filIcon.fname[0] != 0);
		}
		f_closedir(&dir);

		strcpy(entry.filImage.fname, "..");
		entry.filIcon.fname[0] = 0;
		folder.entries.push_back(entry);

		std::sort(folder.entries.begin(), folder.entries.end(), greater());

		if (folder.entries.size() > 0) folder.current = &folder.entries[0];
		else folder.current = 0;

		folder.currentIndex = 0;
	}
	else
	{
		//DEBUG_LOG("Cannot open dir");
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
	f_chdir("\\1541");
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
	char buffer[1024];
	if (f_getcwd(buffer, 1024) == FR_OK)
	{
		u32 textColour = Colour(VIC2_COLOUR_INDEX_LGREEN);
		u32 bgColour = Colour(VIC2_COLOUR_INDEX_GREY);
			
		screenMain->ClearArea(0, 0, (int)screenMain->Width(), 17, bgColour);
		screenMain->PrintText(false, 0, 0, buffer, textColour, bgColour);
	}

	//u32 offsetX = screenMain->ScaleX(1024 - 320);
	//RefeshDisplayForBrowsableList(&folder, 0);
	//RefeshDisplayForBrowsableList(&caddySelections, offsetX, false);

	folder.RefreshViews();
	caddySelections.RefreshViews();

	DisplayPNG();
	DisplayStatusBar();
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

			if (f_stat(fileName, &filIcon) == FR_OK && filIcon.fsize < FILEBROWSER_MAX_PNG_SIZE)
				foundValid = true;
		}
	}
	return foundValid;
}

void FileBrowser::DisplayPNG(FILINFO& filIcon, int x, int y)
{
	if (filIcon.fname[0] != 0 && filIcon.fsize < FILEBROWSER_MAX_PNG_SIZE)
	{
		FIL fp;
		FRESULT res;

		res = f_open(&fp, filIcon.fname, FA_READ);
		if (res == FR_OK)
		{
			u32 bytesRead;
			SetACTLed(true);
			f_read(&fp, PNG, FILEBROWSER_MAX_PNG_SIZE, &bytesRead);
			SetACTLed(false);
			f_close(&fp);

			int w;
			int h;
			int channels_in_file;
			stbi_uc* image = stbi_load_from_memory((stbi_uc const*)PNG, bytesRead, &w, &h, &channels_in_file, 4);
			if (image && (w == PNG_WIDTH && h == PNG_HEIGHT))
			{
				//DEBUG_LOG("Opened PNG %s w = %d h = %d cif = %d\r\n", fileName, w, h, channels_in_file);
				screenMain->PlotImage((u32*)image, x, y, w, h);
			}
			else
			{
				//DEBUG_LOG("Invalid PNG size %d x %d\r\n", w, h);
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
	if (displayPNGIcons && folder.current)
	{
		FileBrowser::BrowsableList::Entry* current = folder.current;
		u32 x = screenMain->ScaleX(1024 - 320);
		u32 y = screenMain->ScaleY(666) - 240;
		DisplayPNG(current->filIcon, x, y);
	}
}

void FileBrowser::PopFolder()
{
	f_chdir("..");
	//{
	//	char buffer[1024];
	//	if (f_getcwd(buffer, 1024) == FR_OK)
	//	{
	//		DEBUG_LOG("CWD = %s\r\n", buffer);
	//	}
	//}
	RefreshFolderEntries();
	caddySelections.Clear();
	RefeshDisplay();
}

void FileBrowser::UpdateInput()
{
	InputMappings* inputMappings = InputMappings::Instance();
	Keyboard* keyboard = Keyboard::Instance();
	bool dirty = false;

	if (keyboard->CheckChanged())
		dirty = inputMappings->CheckKeyboardBrowseMode();
	else
		dirty = inputMappings->CheckButtonsBrowseMode();

	if (dirty)
	{
		//if (state == State_Folders)
			UpdateInputFolders();
		//else
		//	UpdateInputDiskCaddy();
	}
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
	Keyboard* keyboard = Keyboard::Instance();
	InputMappings* inputMappings = InputMappings::Instance();

	if (folder.entries.size() > 0)
	{
		//u32 numberOfEntriesMinus1 = folder.entries.size() - 1;
		bool dirty = false;

		if (inputMappings->BrowseSelect())
		{
			FileBrowser::BrowsableList::Entry* current = folder.current;
			if (current)
			{
				if (current->filImage.fattrib & AM_DIR)
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
				else
				{
					if (strcmp(current->filImage.fname, "..") == 0)
					{
						PopFolder();
					}
					else if (DiskImage::IsDiskImageExtention(current->filImage.fname))
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
		//else if (keyboard->KeyPressed(KEY_TAB))
		//{
		//	state = State_DiskCaddy;
		//	dirty = true;
		//}
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
		else
		{
			unsigned keySetIndex;
			for (keySetIndex = 0; keySetIndex < ROMs::MAX_ROMS; ++keySetIndex)
			{
				unsigned keySetIndexBase = keySetIndex * 3;
				if (keyboard->KeyPressed(FileBrowser::SwapKeys[keySetIndexBase]) || keyboard->KeyPressed(FileBrowser::SwapKeys[keySetIndexBase + 1]) || keyboard->KeyPressed(FileBrowser::SwapKeys[keySetIndexBase + 2]))
				{
					if (roms->ROMValid[keySetIndex])
					{
						roms->currentROMIndex = keySetIndex;
						roms->lastManualSelectedROMIndex = keySetIndex;
						DEBUG_LOG("Swap ROM %d %s\r\n", keySetIndex, roms->ROMNames[keySetIndex]);
						ShowDeviceAndROM();
					}
				}
			}

			dirty = folder.CheckBrowseNavigation();
		}

		if (dirty) RefeshDisplay();
	}
	else
	{
		if (inputMappings->BrowseBack())
			PopFolder();
	}
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
				if (diskType == DiskImage::D64 || diskType == DiskImage::G64 || diskType == DiskImage::NIB || diskType == DiskImage::NBZ)
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

// Not used
void FileBrowser::UpdateInputDiskCaddy()
{
	bool dirty = false;
	Keyboard* keyboard = Keyboard::Instance();

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

void FileBrowser::DisplayStatusBar()
{
	u32 x = 0;
	u32 y = screenMain->ScaleY(STATUS_BAR_POSITION_Y);

	char bufferOut[128];
	snprintf(bufferOut, 256, "LED 0 Motor 0 Track 00.0 ATN 0 DAT 0 CLK 0");
	screenMain->PrintText(false, x, y, bufferOut, RGBA(0, 0, 0, 0xff), RGBA(0xff, 0xff, 0xff, 0xff));
}

void FileBrowser::ClearScreen()
{
	u32 bgColour = palette[VIC2_COLOUR_INDEX_BLUE];
	screenMain->Clear(bgColour);
}

void FileBrowser::ClearSelections()
{
	selectionsMade = false;
	caddySelections.Clear();

	folder.ClearSelections();
}

void FileBrowser::ShowDeviceAndROM()
{
	char buffer[256];
	u32 textColour = RGBA(0, 0, 0, 0xff);
	u32 bgColour = RGBA(0xff, 0xff, 0xff, 0xff);
	u32 x = 0; // 43 * 8
	u32 y = screenMain->ScaleY(STATUS_BAR_POSITION_Y) - 20;

	snprintf(buffer, 256, "Device %d %s\r\n", deviceID, roms->ROMNames[roms->currentROMIndex]);
	screenMain->PrintText(false, x, y, buffer, textColour, bgColour);
}

void FileBrowser::DisplayDiskInfo(DiskImage* diskImage, const char* filenameForIcon)
{
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
	u32 fontHeight = screenMain->GetFontHeight();
	u32 x = 0;
	u32 y = 0;
	char bufferOut[128] = { 0 };
	u32 textColour = palette[VIC2_COLOUR_INDEX_LBLUE];
	u32 bgColour = palette[VIC2_COLOUR_INDEX_BLUE];

	u32 usedColour = palette[VIC2_COLOUR_INDEX_RED];
	u32 freeColour = palette[VIC2_COLOUR_INDEX_LGREEN];
	u32 BAMOffsetX = screenMain->ScaleX(400);

	ClearScreen();

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

		strncpy(name, (char*)&buffer[144], 16);

		int blocksFree = 0;
		int bamTrack;
		int lastTrackUsed = (int)diskImage->LastTrackUsed() >> 1;
		for (bamTrack = 0; bamTrack < 35; ++bamTrack)
		{
			if ((bamTrack + 1) != 18)
				blocksFree += buffer[BAM_OFFSET + bamTrack * BAM_ENTRY_SIZE];

			x = BAMOffsetX;
			for (int bit = 0; bit < DiskImage::SectorsPerTrack[bamTrack]; bit++)
			{
				u32 bits = buffer[BAM_OFFSET + 1 + (bit >> 3) + bamTrack * BAM_ENTRY_SIZE];
				bool used = (bits & (1 << (bit & 0x7))) == 0;

				if (!used)
				{
					snprintf(bufferOut, 128, "%c", screen2petscii(87));
					screenMain->PrintText(true, x, y, bufferOut, usedColour, bgColour);
				}
				else
				{
					snprintf(bufferOut, 128, "%c", screen2petscii(81));
					screenMain->PrintText(true, x, y, bufferOut, freeColour, bgColour);
				}
				x += 8;
				bits <<= 1;
			}
			y += fontHeight;
		}
		for (; bamTrack < lastTrackUsed; ++bamTrack)
		{
			x = BAMOffsetX;
			for (int bit = 0; bit < DiskImage::SectorsPerTrack[bamTrack]; bit++)
			{
				snprintf(bufferOut, 128, "%c", screen2petscii(87));
				screenMain->PrintText(true, x, y, bufferOut, usedColour, bgColour);
				x += 8;
			}
			y += fontHeight;
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
					complete |= (trackNext == 00) || (sectorNoNext == 0xff);
					complete |= (trackNext == 18) && (sectorNoNext == 1);
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
}

void FileBrowser::AutoSelectImage(const char* image)
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
