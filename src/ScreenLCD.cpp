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

extern unsigned char* CBMFont;

void ScreenLCD::Open(u32 widthDesired, u32 heightDesired, u32 colourDepth, int BSCMaster, int LCDAddress, int LCDFlip, LCD_MODEL LCDType, bool luseCBMFont)
{
	bpp = 1;

	if (widthDesired < 128)
		widthDesired = 128;
	if (heightDesired < 32)
		heightDesired = 32;
	if (widthDesired > 128)
		widthDesired = 128;
	if (heightDesired > 64)
		heightDesired = 64;

	width = widthDesired;
	height = heightDesired;
	useCBMFont = luseCBMFont;
 
	ssd1306 = new SSD1306(BSCMaster, LCDAddress, width, height, LCDFlip, LCDType);
	ssd1306->ClearScreen();
	ssd1306->RefreshScreen();
	ssd1306->DisplayOn();

	opened = true;
}

void ScreenLCD::DrawRectangle(u32 x1, u32 y1, u32 x2, u32 y2, RGBA colour)
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

void ScreenLCD::ClearInit(RGBA colour)
{
	ssd1306->InitHardware();
	ssd1306->ClearScreen();
	ssd1306->SetContrast(ssd1306->GetContrast());
	ssd1306->DisplayOn();
}

void ScreenLCD::SetContrast(u8 value)
{
	ssd1306->SetContrast(value);
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

void ScreenLCD::PlotRawImage(const u8* image, int x, int y, int w, int h)
{
	if (x==0 && y==0 && w==128 && h==64)
	{
		ssd1306->PlotImage(image);
	}
}

u32 ScreenLCD::PrintText(bool petscii, u32 x, u32 y, char *ptr, RGBA TxtColour, RGBA BkColour, bool measureOnly, u32* width, u32* height)
{
	int len = 0;
	ssd1306->PlotText(UseCBMFont(), petscii, x >> 3, y / GetFontHeight (), ptr, (BkColour & 0xffffff) != 0);
	return len;
}

u32 ScreenLCD::MeasureText(bool petscii, char *ptr, u32* width, u32* height)
{
	return PrintText(petscii, 0, 0, ptr, 0, 0, true, width, height);
}

u32 ScreenLCD::GetFontHeight()
{
	if (CBMFont && useCBMFont)
		return 8;
	else
		return 16;
}

void ScreenLCD::RefreshScreen()
{
	ssd1306->RefreshScreen();
}

void ScreenLCD::SwapBuffers()
{
	ssd1306->RefreshScreen();
}

void ScreenLCD::RefreshRows(u32 start, u32 amountOfRows)
{
	if (ssd1306)
	{
	if (UseCBMFont())
		ssd1306->RefreshTextRows(start, amountOfRows);
	else
		ssd1306->RefreshTextRows(start*2, amountOfRows*2);   
	}
}

bool ScreenLCD::IsLCD()
{
	return true;
}

bool ScreenLCD::UseCBMFont()
{
	return (CBMFont && useCBMFont);
}