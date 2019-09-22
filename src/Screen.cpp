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

#include "Screen.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "debug.h"
#include "Petscii.h"
#include "stb_image_config.h"

extern "C"
{
	#include "rpi-mailbox-interface.h"
	#include "xga_font_data.h"
}

extern u32 RPi_CpuId;

extern unsigned char* CBMFont;

static const int BitFontHt = 16;
static const int BitFontWth = 8;

void Screen::Open(u32 widthDesired, u32 heightDesired, u32 colourDepth)
{
	if (widthDesired < 320)
		widthDesired = 320;
	if (heightDesired < 240)
		heightDesired = 240;
	if (widthDesired > 1024)
		widthDesired = 1024;
	if (heightDesired > 720)
		heightDesired = 720;

	rpi_mailbox_property_t* mp;
	//int width = 0;
	//int height = 0;
	//int depth = 0;

	scaleX = (float)widthDesired / 1024.0f;
	scaleY = (float)heightDesired / 768.0f;

	RPI_PropertyInit();
	RPI_PropertyAddTag(TAG_GET_PHYSICAL_SIZE);
	RPI_PropertyAddTag(TAG_GET_VIRTUAL_SIZE);
	RPI_PropertyAddTag(TAG_GET_DEPTH);
	RPI_PropertyProcess();

	//if ((mp = RPI_PropertyGet(TAG_GET_PHYSICAL_SIZE)))
	//{
	//	width = mp->data.buffer_32[0];
	//	height = mp->data.buffer_32[1];
	//}

	//if ((mp = RPI_PropertyGet(TAG_GET_DEPTH)))
	//	depth = mp->data.buffer_32[0];

	//DEBUG_LOG("width = %d height = %d depth = %d\r\n", width, height, depth);

	do
	{
		RPI_PropertyInit();
		RPI_PropertyAddTag(TAG_ALLOCATE_BUFFER);
		RPI_PropertyAddTag(TAG_SET_PHYSICAL_SIZE, widthDesired, heightDesired);
		RPI_PropertyAddTag(TAG_SET_VIRTUAL_SIZE, widthDesired, heightDesired);	// Don't need to double buffer (yet).
		RPI_PropertyAddTag(TAG_SET_DEPTH, colourDepth);
		RPI_PropertyAddTag(TAG_GET_PITCH);
		RPI_PropertyAddTag(TAG_GET_PHYSICAL_SIZE);
		RPI_PropertyAddTag(TAG_GET_DEPTH);
		RPI_PropertyProcess();

		if ((mp = RPI_PropertyGet(TAG_GET_PHYSICAL_SIZE)))
		{
			width = mp->data.buffer_32[0];
			height = mp->data.buffer_32[1];
		}

		if ((mp = RPI_PropertyGet(TAG_GET_DEPTH)))
			bpp = mp->data.buffer_32[0];

		if ((mp = RPI_PropertyGet(TAG_GET_PITCH)))
			pitch = mp->data.buffer_32[0];

		if ((mp = RPI_PropertyGet(TAG_ALLOCATE_BUFFER)))
			framebuffer = (unsigned char*)(mp->data.buffer_32[0] & 0x3FFFFFFF);
	}
	while (framebuffer == 0);


	//RPI_PropertyInit();
	//RPI_PropertyAddTag(TAG_SET_PALETTE, palette);
	//RPI_PropertyProcess();

	switch (bpp)
	{
		case 32:
			plotPixelFn = &Screen::PlotPixel32;
		break;
		case 24:
			plotPixelFn = &Screen::PlotPixel24;
		break;
		default:
		case 16:
			plotPixelFn = &Screen::PlotPixel16;
		break;
		case 8:
			plotPixelFn = &Screen::PlotPixel8;
		break;
	}

	opened = true;
}

void Screen::PlotPixel32(u32 pixel_offset, RGBA Colour)
{
#if not defined(EXPERIMENTALZERO)
	*((volatile RGBA*)&framebuffer[pixel_offset]) = Colour;
#endif
}
void Screen::PlotPixel24(u32 pixel_offset, RGBA Colour)
{
#if not defined(EXPERIMENTALZERO)
	framebuffer[pixel_offset++] = BLUE(Colour);
	framebuffer[pixel_offset++] = GREEN(Colour);
	framebuffer[pixel_offset++] = RED(Colour);
#endif
}
void Screen::PlotPixel16(u32 pixel_offset, RGBA Colour)
{
#if not defined(EXPERIMENTALZERO)
	*(unsigned short*)&framebuffer[pixel_offset] = ((RED(Colour) >> 3) << 11) | ((GREEN(Colour) >> 2) << 5) | (BLUE(Colour) >> 3);
#endif
}
void Screen::PlotPixel8(u32 pixel_offset, RGBA Colour)
{
#if not defined(EXPERIMENTALZERO)
	framebuffer[pixel_offset++] = RED(Colour);
#endif
}

void Screen::DrawRectangle(u32 x1, u32 y1, u32 x2, u32 y2, RGBA colour)
{
	ClipRect(x1, y1, x2, y2);

	for (u32 y = y1; y < y2; y++)
	{
		u32 line = y * pitch;
		for (u32 x = x1; x < x2; x++)
		{
			u32 pixel_offset = (x * (bpp >> 3)) + line;
			(this->*Screen::plotPixelFn)(pixel_offset, colour);
		}
	}
}

void Screen::ScrollArea(u32 x1, u32 y1, u32 x2, u32 y2)
{
	ClipRect(x1, y1, x2, y2);

	if (x2 - 1 <= x1)
		return;

	for (u32 y = y1; y < y2; y++)
	{
		u32 line = y * pitch;
		for (u32 x = x1; x < (x2 - 1); x++)
		{
			u32 pixel_offset = ((x + 1) * (bpp >> 3)) + line;
			u32 pixel_offsetDest = (x * (bpp >> 3)) + line;
			*(unsigned short*)&framebuffer[pixel_offsetDest] = *(unsigned short*)&framebuffer[pixel_offset];
		}
	}
}

void Screen::Clear(RGBA colour)
{
	DrawRectangle(0, 0, width, height, colour);
}

// HACK: I have a better fix for this coming when I commit support for other LCDs and screens (each screen can use its own character set/font)
u32 Screen::GetFontHeight()
{
	return 16;
}

u32 Screen::GetFontHeightDirectoryDisplay()
{
	if (CBMFont)
		return 8;
	else
		return BitFontHt;
}

static char vga2screen(char c)
{
	if ((u8)c == 160)
		c = ' ';
	else if ((u8)c == 209)
		c = 'X';
	else if ((u8)c == 215)
		c = 'O';
	return c;
}

void Screen::WriteChar(bool petscii, u32 x, u32 y, unsigned char c, RGBA colour)
{
	if (opened)
	{
		u32 fontHeight;
		const unsigned char* fontBitMap;
		if (petscii && CBMFont)
		{
			fontBitMap = CBMFont;
			fontHeight = 8;
			c = petscii2screen(c);
		}
		else
		{
			if (petscii)
				c = vga2screen(c);
			fontBitMap = avpriv_vga16_font;
			fontHeight = BitFontHt;
		}
		for (u32 py = 0; py < fontHeight; ++py)
		{
			if (y + py > height)
				return;

			unsigned char b = fontBitMap[c * fontHeight + py];
			int yoffs = (y + py) * pitch;
			for (int px = 0; px < 8; ++px)
			{
				if (x + px >= width)
					continue;

				int pixel_offset = ((px + x) * (bpp >> 3)) + yoffs;
				if ((b & 0x80) == 0x80)
					(this->*Screen::plotPixelFn)(pixel_offset, colour);
				b = b << 1;
			}
		}
	}
}

void Screen::PlotPixel(u32 x, u32 y, RGBA colour)
{
	if (x < 0 || y < 0 || x >= width || y >= height)
		return;
	int pixel_offset = (x * (bpp >> 3)) + (y * pitch);
	(this->*Screen::plotPixelFn)(pixel_offset, colour);
}

void Screen::DrawLine(u32 x1, u32 y1, u32 x2, u32 y2, RGBA colour)
{
	ClipRect(x1, y1, x2, y2);

	int dx0, dy0, ox, oy, eulerMax;
	dx0 = (int)(x2 - x1);
	dy0 = (int)(y2 - y1);
	eulerMax = abs(dx0);
	if (abs(dy0) > eulerMax) eulerMax = abs(dy0);
	for (int i = 0; i <= eulerMax; i++)
	{
		ox = ((dx0 * i) / eulerMax) + x1;
		oy = ((dy0 * i) / eulerMax) + y1;
		int pixel_offset = (ox * (bpp >> 3)) + (oy * pitch);
		(this->*Screen::plotPixelFn)(pixel_offset, colour);
	}
}

void Screen::DrawLineV(u32 x, u32 y1, u32 y2, RGBA colour)
{
	//ClipRect(x, y1, x, y2);
	for (u32 y = y1; y <= y2; ++y)
	{
		int pixel_offset = (x * (bpp >> 3)) + (y * pitch);
		(this->*Screen::plotPixelFn)(pixel_offset, colour);
	}
}

u32 Screen::PrintText(bool petscii, u32 x, u32 y, char *ptr, RGBA TxtColour, RGBA BkColour, bool measureOnly, u32* width, u32* height)
{
	int xCursor = x;
	int yCursor = y;
	int len = 0;
	u32 fontHeight;

	if (petscii && CBMFont) fontHeight = 8;
	else fontHeight = BitFontHt;

	if (width) *width = 0;

	while (*ptr != 0)
	{
		char c = *ptr++;
		if ((c != '\r') && (c != '\n'))
		{
			if (!measureOnly)
			{
				DrawRectangle(xCursor, yCursor, xCursor + BitFontWth, yCursor + fontHeight, BkColour);
				WriteChar(petscii, xCursor, yCursor, c, TxtColour);
			}
			xCursor += BitFontWth;
			if (width) *width = MAX(*width, (u32)MAX(0, xCursor));
		}
		else
		{
			xCursor = x;
			yCursor += fontHeight;
		}
		len++;
	}
	if (height) *height = yCursor;

	return len;
}

u32 Screen::MeasureText(bool petscii, char *ptr, u32* width, u32* height)
{
	return PrintText(petscii, 0, 0, ptr, 0, 0, true, width, height);
}

void Screen::PlotImage(u32* image, int x, int y, int w, int h)
{
	int px;
	int py;
	int i = 0;
	for (py = 0; py < h; ++py)
	{
		for (px = 0; px < w; ++px)
		{
			PlotPixel(x + px, y + py, image[i++]);
		}
	}
}

