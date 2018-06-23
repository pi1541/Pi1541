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

#ifndef SSD1306_H
#define SSD1306_H
#include "types.h"
extern "C"
{
#include "rpi-i2c.h"
}

// 8 pages * (128 columns * 8 bits)
//0                      127 0                         127
//0                      127 0                         127
//0                      127 0                         127
//________________________________________________________
//7777777					|    						 7
//6666666					|							 6
//5555555					|							 5
//4444444	PG(ROW)0		|			PG(ROW)1		 4
//3333333					|							 3
//2222222					|							 2
//1111111					|							 1
//0000000					|							 0
//________________________________________________________
//7777777					|    						 7
//6666666					|							 6
//5555555					|							 5
//4444444	PG(ROW)2		|			PG(ROW)3		 4
//3333333					|							 3
//2222222					|							 2
//1111111					|							 1
//0000000					|							 0
//________________________________________________________
//7777777					|    						 7
//6666666					|							 6
//5555555					|							 5
//4444444	PG(ROW)4		|			PG(ROW)5		 4
//3333333					|							 3
//2222222					|							 2
//1111111					|							 1
//0000000					|							 0
//________________________________________________________
//7777777					|    						 7
//6666666					|							 6
//5555555					|							 5
//4444444	PG(ROW)6		|			PG(ROW)7		 4
//3333333					|							 3
//2222222					|							 2
//1111111					|							 1
//0000000					|							 0
//________________________________________________________
#define SSD1306_128x64_BYTES ((128 * 64) / 8)

class SSD1306
{
public:
	// 128x32 0x3C
	// 128x64 0x3D or 0x3C (if SA0 is grounded)
	SSD1306(int BSCMaster = 1, u8 address = 0x3C, int flip = 0, int type=1306);

	void PlotCharacter(int x, int y, char ascii, bool inverse);
	void Plottext(int x, int y, char* str, bool inverse);

	void DisplayOn();
	void DisplayOff();
	void SetContrast(u8 value);
	void SetVCOMDeselect(u8 value);

	void ClearScreen();
	void RefreshScreen();
	void RefreshRows(u8 start, u8 amountOfRows);

protected:
	void SendCommand(u8 command);
	void SendData(u8 data);

	void Home();
	void MoveCursorByte(u8 row, u8 col);
	void MoveCursorCharacter(u8 row, u8 col);

	unsigned char frame[SSD1306_128x64_BYTES];

	int BSCMaster;
	u8 address;
	int type;
};
#endif
