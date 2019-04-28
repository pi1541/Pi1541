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

#ifndef SCREENBASE_H
#define SCREENBASE_H

#include "types.h"

typedef u32 RGBA;

#define RED(colour)   ( (u8)(((u32)colour) & 0xFF) )
#define GREEN(colour)  ( (u8)(((u32)colour >> 8) & 0xFF) )
#define BLUE(colour)    ( (u8)(((u32)colour >> 16) & 0xFF) )
#define ALPHA(colour)  ( (u8)(((u32)colour >> 24) & 0xFF) )

#define RGBA(r, g, b, a)  ( ((u32)((u8)(r))) | ((u32)((u8)(g)) << 8) | ((u32)((u8)(b)) << 16) | ((u32)((u8)(a)) << 24) )

#define MAX(x, y) (((x) > (y)) ? (x) : (y))

class ScreenBase
{

public:
	ScreenBase()
		: opened(false)
		, width(0)
		, height(0)
		, bpp(0)
		, pitch(0)
		, framebuffer(0)
	{
	}

	virtual void DrawRectangle(u32 x1, u32 y1, u32 x2, u32 y2, RGBA colour) = 0;
	virtual void Clear(RGBA colour) = 0;

	virtual void ScrollArea(u32 x1, u32 y1, u32 x2, u32 y2) = 0;

	virtual void WriteChar(bool petscii, u32 x, u32 y, unsigned char c, RGBA colour) = 0;
	virtual u32 PrintText(bool petscii, u32 xPos, u32 yPos, char *ptr, RGBA TxtColour = RGBA(0xff, 0xff, 0xff, 0xff), RGBA BkColour = RGBA(0, 0, 0, 0xFF), bool measureOnly = false, u32* width = 0, u32* height = 0) = 0;
	virtual u32 MeasureText(bool petscii, char *ptr, u32* width = 0, u32* height = 0) = 0;

	virtual void PlotPixel(u32 x, u32 y, RGBA colour) = 0;

	virtual void PlotImage(u32* image, int x, int y, int w, int h) = 0;

	u32 Width() const { return width; }
	u32 Height() const { return height; }

	virtual float GetScaleX() const { return 1; }
	virtual float GetScaleY() const { return 1; }

	virtual u32 ScaleX(u32 x) { return x; }
	virtual u32 ScaleY(u32 y) { return y; }

	virtual u32 GetFontWidth() { return 8; }
	virtual u32 GetFontHeight() = 0;
	virtual u32 GetFontHeightDirectoryDisplay() { return 16; }

	virtual void SwapBuffers() = 0;
	virtual void RefreshRows(u32 start, u32 amountOfRows) {}

	virtual bool IsLCD() { return false; };
	virtual bool UseCBMFont() { return false; };

	bool IsMonocrome() const { return bpp == 1; }

protected:

	//typedef void (ScreenBase::*PlotPixelFunction)(u32 pixel_offset, RGBA Colour);

	//PlotPixelFunction plotPixelFn;

	void ClipRect(u32& x1, u32& y1, u32& x2, u32& y2)
	{
		if (x1 > width) x1 = width;
		if (y1 > height) y1 = height;
		if (x2 > width) x2 = width;
		if (y2 > height) y2 = height;
	}

	bool opened;
	u32 width;
	u32 height;
	u32 bpp;
	u32 pitch;
	u8* framebuffer;
};

#endif
