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

#ifndef IOPort_H
#define IOPort_H
#include <assert.h>

typedef void(*PortOutFn)(void*, unsigned char status);

class IOPort
{
public:
	IOPort() : stateOut(0), stateIn(0), direction(0), portOutFn(0) {}

	inline void SetInput(unsigned char pin, bool state)
	{
		if (state) stateIn |= pin;
		else stateIn &= ~pin;
	}
	inline unsigned char GetInput() { return stateIn; }
	inline void SetInput(unsigned char value) { stateIn = value; }
	inline unsigned char GetOutput() { return stateOut; }
	inline void SetOutput(unsigned char value) { stateOut = value; if (portOutFn) (portOutFn)(portOutFnThis, stateOut & direction); }
	inline unsigned char GetDirection() { return direction; }
	inline void SetDirection(unsigned char value) { direction = value; if (portOutFn) (portOutFn)(portOutFnThis, stateOut & direction); }
	inline void SetPortOut(void* data, PortOutFn fn) { portOutFnThis = data; portOutFn = fn; }
private:
	unsigned char stateOut;
	unsigned char stateIn;
	unsigned char direction;
	PortOutFn portOutFn;
	void* portOutFnThis;
};
#endif
