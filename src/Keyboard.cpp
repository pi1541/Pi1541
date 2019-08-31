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
#if not defined(EXPERIMENTALZERO)
#include "Keyboard.h"
#include <string.h>
#include <uspi.h>
#include "Timer.h"
#include "debug.h"
extern "C"
{
#include "uspi/devicenameservice.h"
}

#define REPEAT_RATE		8
#define REPEAT_DELAY	3

Keyboard* Keyboard::instance;

void Keyboard::KeyPressedHandlerRaw(TUSBKeyboardDevice* device, unsigned char modifiers, const unsigned char RawKeys[6])
{
	// byte 0 - modifires
	//  bit 0: left control
	//  bit 1 : left shift
	//  bit 2 : left alt
	//  bit 3 : left GUI(Win / Apple / Meta key)
	//  bit 4 : right control
	//  bit 5 : right shift
	//  bit 6 : right alt
	//  bit 7 : right GUI
	// byte 1 - reserved
	// bytes 2-7 - represent the keys that are concurrently pressed (up to 6 in this case)
	//				ranges from 0x04 to 0xE7 
	//				If no keys are currently pressed then all 6 bytes should contain 0x00
	//				The key repeat delay and rate is purely a host function.
	//DEBUG_LOG("KeyPressedHandlerRaw: %x %d %d %d %d %d %d\r\n", modifiers, RawKeys[0], RawKeys[1], RawKeys[2], RawKeys[3], RawKeys[4], RawKeys[5]);

	Keyboard* keyboard = Keyboard::Instance();
	bool anyDown = false;

	keyboard->keyStatusPrev[0] = keyboard->keyStatus[0];
	keyboard->keyStatusPrev[1] = keyboard->keyStatus[1];
	keyboard->keyStatus[0] = 0;
	keyboard->keyStatus[1] = 0;

	keyboard->modifier = modifiers;

	int index;
	for (index = 0; index < 6; ++index)
	{
		u8 rawKey = RawKeys[index];
		if (rawKey >= 4)
		{
			int keyStatusIndex = (rawKey >= 64) ? 1 : 0;

			//DEBUG_LOG("%x %d\r\n", rawKey, keyStatusIndex);

			u64 keyBit = 1ULL << (u64)(rawKey & 0x3f);
			if (keyboard->keyStatusPrev[keyStatusIndex] & keyBit)
			{
			}
			else
			{
				keyboard->keyRepeatCount[rawKey] = 0;
			}
			keyboard->keyStatus[keyStatusIndex] |= keyBit;
			anyDown = true;
		}
	}

	if (anyDown)
	{
		// Only need the timer if a key was held down
		if (keyboard->timer == 0)
		{
			keyboard->timer = TimerStartKernelTimer(REPEAT_RATE, USBKeyboardDeviceTimerHandler, 0, device);
			//DEBUG_LOG("Timer started\r\n");
		}
	}
	else
	{
		if (keyboard->timer != 0)
		{
			TimerCancelKernelTimer(keyboard->timer);
			keyboard->timer = 0;
		}
	}
	keyboard->updateCount++;
}

void Keyboard::USBKeyboardDeviceTimerHandler(unsigned hTimer, void *pParam, void *pContext)
{
	Keyboard* keyboard = Keyboard::Instance();
	bool anyDown = false;

	int keyStatusIndex;
	int keyIndex;
	for (keyStatusIndex = 0; keyStatusIndex < 2; ++keyStatusIndex)
	{
		for (keyIndex = 0; keyIndex < 64; ++keyIndex)
		{
			int keyCodeIndex = keyStatusIndex * 64 + keyIndex;

			if (keyboard->keyStatus[keyStatusIndex] & (1ULL << keyIndex))
			{
				anyDown = true;

				keyboard->keyRepeatCount[keyCodeIndex]++;
				if (keyboard->keyRepeatCount[keyCodeIndex] > REPEAT_DELAY)
					keyboard->updateCount++;
			}
			else
			{
				keyboard->keyRepeatCount[keyCodeIndex] = 0;
			}
		}
	}

	keyboard->keyStatusPrev[0] = keyboard->keyStatus[0];
	keyboard->keyStatusPrev[1] = keyboard->keyStatus[1];

	if (keyboard->timer != 0)
	{
		TimerCancelKernelTimer(keyboard->timer);
		keyboard->timer = 0;
	}

	if (anyDown)	// Only need the timer if a key was held down
		keyboard->timer = TimerStartKernelTimer(REPEAT_RATE, USBKeyboardDeviceTimerHandler, 0, pContext);
}

Keyboard::Keyboard()
	: modifier(0)
	, timer(0)
	, updateCount(0)
	, updateCountLastRead(-1)
{
	instance = this;
	keyStatus[0] = 0;
	keyStatus[1] = 0;

	memset(keyRepeatCount, 0, sizeof(keyRepeatCount));
	USPiKeyboardRegisterKeyStatusHandlerRaw(KeyPressedHandlerRaw);
}
#endif
