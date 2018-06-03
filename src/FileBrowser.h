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

#ifndef FileBrowser_H
#define FileBrowser_H
#include <assert.h>
#include "ff.h"
#include <vector>
#include "types.h"
#include "DiskImage.h"
#include "DiskCaddy.h"
#include "ROMs.h"
#include "ScreenBase.h"

#define VIC2_COLOUR_INDEX_BLACK		0
#define VIC2_COLOUR_INDEX_WHITE		1
#define VIC2_COLOUR_INDEX_RED		2
#define VIC2_COLOUR_INDEX_CYAN		3
#define VIC2_COLOUR_INDEX_MAGENTA	4
#define VIC2_COLOUR_INDEX_GREEN		5
#define VIC2_COLOUR_INDEX_BLUE		6
#define VIC2_COLOUR_INDEX_YELLOW	7
#define VIC2_COLOUR_INDEX_ORANGE	8
#define VIC2_COLOUR_INDEX_BROWN		9
#define VIC2_COLOUR_INDEX_PINK		10
#define VIC2_COLOUR_INDEX_DGREY		11
#define VIC2_COLOUR_INDEX_GREY		12
#define VIC2_COLOUR_INDEX_LGREEN	13
#define VIC2_COLOUR_INDEX_LBLUE		14
#define VIC2_COLOUR_INDEX_LGREY		15

#define FILEBROWSER_MAX_PNG_SIZE	0x10000

#define STATUS_BAR_POSITION_Y (40 * 16 + 10)

class FileBrowser
{
public:

	class BrowsableList;

	class BrowsableListView
	{
	public:
		BrowsableListView(BrowsableList* list, ScreenBase* screen, u32 columns, u32 rows, u32 positionX, u32 positionY, bool lcdPgUpDown)
			: list(list)
			, screen(screen)
			, columns(columns)
			, rows(rows)
			, positionX(positionX)
			, positionY(positionY)
			, lcdPgUpDown(lcdPgUpDown)
		{
		}

		void Refresh();
		bool CheckBrowseNavigation(bool pageOnly);

		BrowsableList* list;
		u32 offset;

		ScreenBase* screen;
		u32 columns;
		u32 rows;
		u32 positionX;
		u32 positionY;
		bool lcdPgUpDown;
	};

	class BrowsableList
	{
	public:
		BrowsableList()
			: current(0)
			, currentIndex(0)
		{
		}

		void Clear()
		{
			u32 index;
			entries.clear();
			current = 0;
			currentIndex = 0;
			for (index = 0; index < views.size(); ++index)
			{
				views[index].offset = 0;
			}
		}

		void AddView(ScreenBase* screen, u32 columns, u32 rows, u32 positionX, u32 positionY, bool lcdPgUpDown)
		{
			BrowsableListView view(this, screen, columns, rows, positionX, positionY, lcdPgUpDown);
			views.push_back(view);
		}

		void ClearSelections();

		struct Entry
		{
			Entry() : caddyIndex(-1)
			{
			}
			FILINFO filImage;
			FILINFO filIcon;
			int caddyIndex;
		};

		Entry* FindEntry(const char* name);

		void RefreshViews();
		bool CheckBrowseNavigation();

		std::vector<Entry> entries;
		Entry* current;
		u32 currentIndex;

		std::vector<BrowsableListView> views;
	};

	FileBrowser(DiskCaddy* diskCaddy, ROMs* roms, unsigned deviceID, bool displayPNGIcons, ScreenBase* screenMain, ScreenBase* screenLCD);

	void AutoSelectTestImage();
	void DisplayRoot();
	void UpdateInput();

	void RefeshDisplay();
	void DisplayDiskInfo(DiskImage* diskImage, const char* filenameForIcon);

	void DisplayStatusBar();

	void FolderChanged();
	void PopFolder();

	bool SelectionsMade() { return selectionsMade; }
	const char* LastSelectionName() { return lastSelectionName; }
	void ClearSelections();

	void ShowDeviceAndROM();

	void ClearScreen();

	void SetDeviceID(u8 id) { deviceID = id; }

	static const long int LSTBuffer_size = 1024 * 8;
	static unsigned char LSTBuffer[];

	static const unsigned SwapKeys[];

	static u32 Colour(int index);


	bool SelectLST(const char* filenameLST);

private:
	void DisplayPNG(FILINFO& filIcon, int x, int y);
	void RefreshFolderEntries();

	void UpdateInputFolders();
	void UpdateInputDiskCaddy();

	//void RefeshDisplayForBrowsableList(FileBrowser::BrowsableList* browsableList, int xOffset, bool showSelected = true);

	bool FillCaddyWithSelections();

	bool AddToCaddy(FileBrowser::BrowsableList::Entry* current);

	bool CheckForPNG(const char* filename, FILINFO& filIcon);
	void DisplayPNG();

	enum State
	{
		State_Folders,
		State_DiskCaddy
	} state;

	BrowsableList folder;
	DiskCaddy* diskCaddy;
	bool selectionsMade;
	const char* lastSelectionName;
	ROMs* roms;
	unsigned deviceID;
	bool displayPNGIcons;

	BrowsableList caddySelections;

	ScreenBase* screenMain;
	ScreenBase* screenLCD;

	char PNG[FILEBROWSER_MAX_PNG_SIZE];
};
#endif
