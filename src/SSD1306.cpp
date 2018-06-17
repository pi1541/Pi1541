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

#include "SSD1306.h"
#include "debug.h"
#include <string.h>

extern "C"
{
#include "xga_font_data.h"
}


#define SSD1306_CMD_SET_MEMORY_ADDRESSING_MODE 0x20

#define SSD1306_CMD_SET_PAGE_ADDRESS 0x22
#define SSD1306_CMD_DEACTIVATE_SCROLL 0x2E
#define SSD1306_CMD_ACTIVATE_SCROLL 0x2F

#define SSD1306_CMD_SET_CONTRAST_CONTROL 0x81	//  Set Contrast Control for BANK0 
#define SSD1306_ENABLE_CHARGE_PUMP 0x8D

#define SSD1306_CMD_ENTIRE_DISPLAY_ON 0xA4
#define SSD1306_CMD_ENTIRE_DISPLAY_OFF 0xA5
#define SSD1306_CMD_NORMAL_DISPLAY 0xA6	// 1 = on pixel
#define SSD1306_CMD_INVERT_DISPLAY 0xA7	// 0 = on pixel

#define SSD1306_CMD_DISPLAY_OFF 0xAE
#define SSD1306_CMD_DISPLAY_ON 0xAF
#define SSD1306_CMD_MULTIPLEX_RATIO 0xA8

#define SSD1306_CMD_SET_START_LINE 0x40

#define SSD1306_CMD_SET_DISPLAY_OFFSET 0xD3
#define SSD1306_CMD_SET_DISPLAY_CLOCK_DIVIDE_RATIO 0xD5
#define SSD1306_CMD_SET_PRE_CHARGE_PERIOD 0xD9
#define SSD1306_CMD_SET_COM_PINS 0xDA
#define SSD1306_CMD_SET_VCOMH_DESELECT_LEVEL 0xDB

#define SSD1306_CONTROL_REG 0x00
#define SSD1306_DATA_REG 0x40

unsigned char frame[SSD1306_128x64_BYTES];

SSD1306::SSD1306(int BSCMaster, u8 address)
	: BSCMaster(BSCMaster)
	, address(address)
{
	RPI_I2CInit(BSCMaster, 1);

	// SSD1306 data sheet configuration flow
	SendCommand(SSD1306_CMD_DISPLAY_OFF);
	SendCommand(SSD1306_CMD_MULTIPLEX_RATIO);
	SendCommand(0x3F);	// SSD1306_LCDHEIGHT - 1

	SendCommand(SSD1306_CMD_SET_DISPLAY_OFFSET);
	SendCommand(0x00);  // no Offset

	SendCommand(SSD1306_CMD_SET_START_LINE | 0x0);

	SendCommand(0xA1);  // Set Segment Re-Map

	SendCommand(0xC8);  // Set COM Output Scan Direction

	SendCommand(SSD1306_CMD_SET_COM_PINS);	// Layout and direction
	SendCommand(0x12);

	SendCommand(SSD1306_CMD_SET_CONTRAST_CONTROL);
	SendCommand(0x7F);

	SendCommand(SSD1306_CMD_ENTIRE_DISPLAY_ON);

	SendCommand(SSD1306_CMD_NORMAL_DISPLAY);
	SendCommand(0xD5);
	SendCommand(0x80);

	SendCommand(SSD1306_CMD_SET_PRE_CHARGE_PERIOD);
	SendCommand(0xF1);

	SendCommand(SSD1306_CMD_SET_VCOMH_DESELECT_LEVEL);
	SendCommand(0x40);

	SendCommand(SSD1306_CMD_SET_DISPLAY_CLOCK_DIVIDE_RATIO);
	SendCommand(0x80);	// upper nibble is rate, lower nibble is divisor

	SendCommand(SSD1306_ENABLE_CHARGE_PUMP);	// Enable charge pump regulator
	SendCommand(0x14);  // external = 0x10 internal = 0x14




	SendCommand(0x00);  // Set Lower Column Start Address

	SendCommand(0x10);  // Set Higher Column Start Address

	SendCommand(0xB0);  // Set Page Start Address for Page Addressing Mode

	SendCommand(SSD1306_CMD_SET_MEMORY_ADDRESSING_MODE);  // Set Memory Addressing Mode
	SendCommand(0x00);	// 00 - Horizontal Addressing Mode

	SendCommand(0x21);  // Set Column Address (only for horizontal or vertical mode)
	SendCommand(0x00);
	SendCommand(0x7F);

	SendCommand(SSD1306_CMD_SET_PAGE_ADDRESS);
	SendCommand(0x00);
	SendCommand(0x07);

	SendCommand(SSD1306_CMD_DEACTIVATE_SCROLL);
}

void SSD1306::SendCommand(u8 command)
{
	char buffer[2];

	buffer[0] = SSD1306_CONTROL_REG;
	buffer[1] = command;

	RPI_I2CWrite(BSCMaster, address, buffer, sizeof(buffer));
}

void SSD1306::SendData(u8 data)
{
	char buffer[2];

	buffer[0] = SSD1306_DATA_REG;
	buffer[1] = data;

	RPI_I2CWrite(BSCMaster, address, buffer, sizeof(buffer));
}

void SSD1306::Home()
{
	SendCommand(0x21); // column range
	SendCommand(0x00); // set start to 0
	SendCommand(0x7F); // set end to 0x7F
	SendCommand(0x22); // row range
	SendCommand(0x00); // set start to 0
	SendCommand(0x07); // set end to 0x07
}

void SSD1306::MoveCursorByte(u8 row, u8 col)
{
	if (col > 127) { col = 127; }
	if (row > 7) { row = 7; }

	SendCommand(0x21); // set column
	SendCommand(col);  // start = col
	SendCommand(0x7F); // end = col max
	SendCommand(0x22); // set row
	SendCommand(row);  // start = row
	SendCommand(0x07); // end = row max
}

void SSD1306::MoveCursorCharacter(u8 row, u8 col)
{
	if (col > 15) { col = 15; }
	if (row > 7) { row = 7; }

	MoveCursorByte(row, col << 3);
}

void SSD1306::RefreshScreen()
{
	int i;
	Home();
	for (i = 0; i < SSD1306_128x64_BYTES; i++)
	{
		SendData(frame[i]);
	}
}

void SSD1306::RefreshRows(u8 start, u8 amountOfRows)
{
	MoveCursorCharacter(start, 0);
	start *= 128;

	int i;
	int end = start + amountOfRows * 128;

	for (i = start * 128; i < end; i++)
	{
		SendData(frame[i]);
	}
}

void SSD1306::ClearScreen()
{
	memset(frame, 0, sizeof(frame));
	RefreshScreen();
}

void SSD1306::DisplayOn()
{
	SendCommand(SSD1306_CMD_DISPLAY_ON);
	ClearScreen();
}

void SSD1306::DisplayOff()
{
	ClearScreen();
	SendCommand(SSD1306_CMD_DISPLAY_OFF);
}

void SSD1306::Plottext(int x, int y, char* str, bool inverse)
{
	int i;
	i = 0;
	while (str[i] && x < 16)
	{
		PlotCharacter(x++, y, str[i++], inverse);
	}
}

// Pg 143 - 145. Transposing an 8x8 bit matrix. Hacker's Delight
void transpose8(unsigned char* B, const unsigned char* A, bool inverse)
{
	unsigned x, y, t;
	x = (A[7] << 24) | (A[6] << 16) | (A[5] << 8) | A[4];
	y = (A[3] << 24) | (A[2] << 16) | (A[1] << 8) | A[0];
	if (inverse)
	{
		x = ~x;
		y = ~y;
	}

	t = (x ^ (x >> 7)) & 0x00AA00AA;  x = x ^ t ^ (t << 7);
	t = (y ^ (y >> 7)) & 0x00AA00AA;  y = y ^ t ^ (t << 7);

	t = (x ^ (x >> 14)) & 0x0000CCCC;  x = x ^ t ^ (t << 14);
	t = (y ^ (y >> 14)) & 0x0000CCCC;  y = y ^ t ^ (t << 14);

	t = (x & 0xF0F0F0F0) | ((y >> 4) & 0x0F0F0F0F);
	y = ((x << 4) & 0xF0F0F0F0) | (y & 0x0F0F0F0F);
	x = t;


	B[0] = x >> 24; B[1] = x >> 16; B[2] = x >> 8; B[3] = x;
	B[4] = y >> 24; B[5] = y >> 16; B[6] = y >> 8; B[7] = y;
}

void SSD1306::PlotCharacter(int x, int y, char c, bool inverse)
{
	unsigned char a[8], b[8];
	transpose8(a, avpriv_vga16_font + (c * 16), inverse);
	transpose8(b, avpriv_vga16_font + (c * 16) + 8, inverse);
	memcpy(frame + (y * 256) + (x * 8), a, 8);
	memcpy(frame + (y * 256) + (x * 8) + 128, b, 8);
}

void SSD1306::PlotPixel(int x, int y, int c)
{
	switch (c)
	{
		case 1:   frame[x+ (y/8)*SSD1306_LCDWIDTH] |=  (1 << (y&7)); break;
		case 0:   frame[x+ (y/8)*SSD1306_LCDWIDTH] &= ~(1 << (y&7)); break;
		case -1:  frame[x+ (y/8)*SSD1306_LCDWIDTH] ^=  (1 << (y&7)); break;
	}
}

// expects source in ssd1306 native vertical byte format
void SSD1306::PlotImage(const unsigned char * source)
{
	memcpy (frame, source, SSD1306_128x64_BYTES);
}
