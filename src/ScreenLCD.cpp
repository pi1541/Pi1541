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

#include "ScreenLCD.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "debug.h"
#include "ssd_logo.h"

void ScreenLCD::Open(u32 widthDesired, u32 heightDesired, u32 colourDepth, int BSCMaster, int LCDAddress)
{
	bpp = 1;

	if (widthDesired < 128)
		widthDesired = 128;
	if (heightDesired < 64)
		heightDesired = 64;
	if (widthDesired > 128)
		widthDesired = 128;
	if (heightDesired > 64)
		heightDesired = 64;

	width = widthDesired;
	height = heightDesired;

	ssd1306 = new SSD1306(BSCMaster, LCDAddress);
	ssd1306->DisplayOn();

	ssd1306->PlotImage(logo_ssd);
	ssd1306->Plottext(5, 0, "Pi1541", false);

	ssd1306->RefreshScreen();

	opened = true;
}

void ScreenLCD::ClearArea(u32 x1, u32 y1, u32 x2, u32 y2, RGBA colour)
{
	ClipRect(x1, y1, x2, y2);
}

void ScreenLCD::ScrollArea(u32 x1, u32 y1, u32 x2, u32 y2)
{
}

void ScreenLCD::Clear(RGBA colour)
{
	ssd1306->ClearScreen();
}

void ScreenLCD::WriteChar(bool petscii, u32 x, u32 y, unsigned char c, RGBA colour)
{
	if (opened)
	{
	}
}

void ScreenLCD::PlotPixel(u32 x, u32 y, RGBA colour)
{
}

void ScreenLCD::PlotImage(u32* image, int x, int y, int w, int h)
{
}

u32 ScreenLCD::PrintText(bool petscii, u32 x, u32 y, char *ptr, RGBA TxtColour, RGBA BkColour, bool measureOnly, u32* width, u32* height)
{
	int len = 0;
	ssd1306->Plottext(x >> 3, y >> 4, ptr, (BkColour & 0xffffff) != 0);
	return len;
}

u32 ScreenLCD::MeasureText(bool petscii, char *ptr, u32* width, u32* height)
{
	return PrintText(petscii, 0, 0, ptr, 0, 0, true, width, height);
}

u32 ScreenLCD::GetFontHeight()
{
	return 16;
}

void ScreenLCD::SwapBuffers()
{
	ssd1306->RefreshScreen();
}

void ScreenLCD::RefreshRows(u8 start, u8 amountOfRows)
{
	ssd1306->RefreshRows(start, amountOfRows);
}
