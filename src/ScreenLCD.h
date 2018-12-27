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

#ifndef SCREENLCD_H
#define SCREENLCD_H

#include "ScreenBase.h"
#include "SSD1306.h"
#include "options.h"

class ScreenLCD : public ScreenBase
{

public:
	ScreenLCD()
		: ScreenBase()
		, ssd1306(0)
	{
	}

	void Open(u32 width, u32 height, u32 colourDepth, int BSCMaster, int LCDAddress, int LCDFlip, LCD_MODEL LCDType, bool luseCBMFont);

	void DrawRectangle(u32 x1, u32 y1, u32 x2, u32 y2, RGBA colour);
	void Clear(RGBA colour);
	void ClearInit(RGBA colour);

	void SetContrast(u8 value);

	void ScrollArea(u32 x1, u32 y1, u32 x2, u32 y2);

	void WriteChar(bool petscii, u32 x, u32 y, unsigned char c, RGBA colour);
	u32 PrintText(bool petscii, u32 xPos, u32 yPos, char *ptr, RGBA TxtColour = RGBA(0xff, 0xff, 0xff, 0xff), RGBA BkColour = RGBA(0, 0, 0, 0xFF), bool measureOnly = false, u32* width = 0, u32* height = 0);
	u32 MeasureText(bool petscii, char *ptr, u32* width = 0, u32* height = 0);

	void PlotPixel(u32 x, u32 y, RGBA colour);

	void PlotImage(u32* image, int x, int y, int w, int h);

	void PlotRawImage(const u8* image, int x, int y, int w, int h);

	u32 GetFontHeight();

	void SwapBuffers();
	void RefreshScreen();

	void RefreshRows(u32 start, u32 amountOfRows);
	bool IsLCD();
	bool UseCBMFont();
private:
	SSD1306* ssd1306 = 0;
	bool useCBMFont;
};

#endif
