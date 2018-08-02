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

#ifndef InputMappings_H
#define InputMappings_H
#include "Singleton.h"
#include "Keyboard.h"

#define ESC_FLAG		(1 << 0)
#define NEXT_FLAG		(1 << 1)
#define PREV_FLAG		(1 << 2)
#define ENTER_FLAG		(1 << 3)
#define UP_FLAG			(1 << 4)
#define PAGEUP_FLAG		(1 << 5)
#define DOWN_FLAG		(1 << 6)
#define PAGEDOWN_FLAG		(1 << 7)
#define SPACE_FLAG		(1 << 8)
#define BACK_FLAG		(1 << 9)
#define INSERT_FLAG		(1 << 10)
#define NUMBER_FLAG		(1 << 11)

#define PAGEDOWN_LCD_FLAG	(1 << 12)
#define PAGEUP_LCD_FLAG		(1 << 13)

#define NEWD64_FLAG		(1 << 14)
#define AUTOLOAD_FLAG		(1 << 15)
#define FAKERESET_FLAG		(1 << 16)
#define WRITEPROTECT_FLAG	(1 << 17)
#define LETTER_FLAG		(1 << 18)
#define HOME_FLAG		(1 << 19)
#define END_FLAG		(1 << 20)
#define FUNCTION_FLAG		(1 << 21)
// dont exceed 32!!

const unsigned NumberKeys[33] =
{
	KEY_F1, KEY_KP1, KEY_1,
	KEY_F2, KEY_KP2, KEY_2,
	KEY_F3, KEY_KP3, KEY_3,
	KEY_F4, KEY_KP4, KEY_4,
	KEY_F5, KEY_KP5, KEY_5,
	KEY_F6, KEY_KP6, KEY_6,
	KEY_F7, KEY_KP7, KEY_7,
	KEY_F8, KEY_KP8, KEY_8,
	KEY_F9, KEY_KP9, KEY_9,
	KEY_F10, KEY_KP0, KEY_0,
	KEY_F11, KEY_KPMINUS, KEY_MINUS
};

class InputMappings : public Singleton<InputMappings>
{
protected:
	friend Singleton<InputMappings>;

	unsigned keyboardFlags;
	unsigned buttonFlags;

	bool keyboardBrowseLCDScreen;

	bool insertButtonPressedPrev;
	bool insertButtonPressed;

	bool enterButtonPressedPrev;
	bool enterButtonPressed;

	unsigned keyboardNumber;
	unsigned keyboardLetter;
	unsigned keyboardFunction;

	//inline void SetUartFlag(unsigned flag) { uartFlags |= flag;	}
	//inline bool UartFlag(unsigned flag) { return (uartFlags & flag) != 0; }
	inline void SetKeyboardFlag(unsigned flag) { keyboardFlags |= flag; }
	inline bool KeyboardFlag(unsigned flag) { return (keyboardFlags & flag) != 0; }
	inline void SetButtonFlag(unsigned flag) { buttonFlags |= flag; }
	inline bool ButtonFlag(unsigned flag) { return (buttonFlags & flag) != 0; }

public:
	InputMappings();

	//void CheckUart();		// One core will call this
	bool CheckKeyboardBrowseMode();
	void CheckKeyboardEmulationMode(unsigned numberOfImages, unsigned numberOfImagesMax);	// The other core will call this
	bool CheckButtonsBrowseMode();
	void CheckButtonsEmulationMode();

	void Reset()
	{
		keyboardFlags = 0;
		buttonFlags = 0;
	}

	void SetKeyboardBrowseLCDScreen(bool value)
	{
		keyboardBrowseLCDScreen = value;
	}

	inline bool Exit()
	{
		return KeyboardFlag(ESC_FLAG)/* | UartFlag(ESC_FLAG)*/ | ButtonFlag(ESC_FLAG);
	}

	inline bool NextDisk()
	{
		return KeyboardFlag(NEXT_FLAG)/* | UartFlag(NEXT_FLAG)*/ | ButtonFlag(NEXT_FLAG);
	}

	inline bool PrevDisk()
	{
		return KeyboardFlag(PREV_FLAG)/* | UartFlag(PREV_FLAG)*/ | ButtonFlag(PREV_FLAG);
	}

	inline bool AutoLoad() { return KeyboardFlag(AUTOLOAD_FLAG); }

	inline bool FakeReset() { return KeyboardFlag(FAKERESET_FLAG); }

	inline bool BrowseSelect()
	{
		return KeyboardFlag(ENTER_FLAG)/* | UartFlag(ENTER_FLAG)*/ | ButtonFlag(ENTER_FLAG);
	}

	inline bool BrowseDone()
	{
		return KeyboardFlag(SPACE_FLAG)/* | UartFlag(SPACE_FLAG)*/;
	}

	inline bool BrowseBack()
	{
		return KeyboardFlag(BACK_FLAG)/* | UartFlag(BACK_FLAG)*/ | ButtonFlag(BACK_FLAG);
	}

	inline bool BrowseUp()
	{
		return KeyboardFlag(UP_FLAG)/* | UartFlag(UP_FLAG)*/ | ButtonFlag(UP_FLAG);
	}

	inline bool BrowsePageUp()
	{
		return KeyboardFlag(PAGEUP_FLAG)/* | UartFlag(PAGEUP_FLAG)*/;
	}
	inline bool BrowsePageUpLCD() { return KeyboardFlag(PAGEUP_LCD_FLAG); }

	inline bool BrowseDown()
	{
		return KeyboardFlag(DOWN_FLAG)/* | UartFlag(DOWN_FLAG)*/ | ButtonFlag(DOWN_FLAG);
	}

	inline bool BrowsePageDown()
	{
		return KeyboardFlag(PAGEDOWN_FLAG)/* | UartFlag(PAGEDOWN_FLAG)*/;
	}
	inline bool BrowsePageDownLCD() { return KeyboardFlag(PAGEDOWN_LCD_FLAG); }

	inline bool BrowseInsert()
	{
		return KeyboardFlag(INSERT_FLAG)/* | UartFlag(INSERT_FLAG)*/ | ButtonFlag(INSERT_FLAG);
	}

	inline bool BrowseNewD64() { return KeyboardFlag(NEWD64_FLAG); }

	inline bool BrowseAutoLoad() { return KeyboardFlag(AUTOLOAD_FLAG); }

	inline bool BrowseFakeReset() { return KeyboardFlag(FAKERESET_FLAG); }

	inline bool BrowseWriteProtect() { return KeyboardFlag(WRITEPROTECT_FLAG); }

	inline bool BrowseNumber() { return KeyboardFlag(NUMBER_FLAG); }
	inline bool BrowseLetter() { return KeyboardFlag(LETTER_FLAG); }
	inline bool BrowseFunction() { return KeyboardFlag(FUNCTION_FLAG); }

	inline bool BrowseHome() { return KeyboardFlag(HOME_FLAG); }

	inline bool BrowseEnd() { return KeyboardFlag(END_FLAG); }

	inline char getKeyboardNumber() { return keyboardNumber; }
	inline char getKeyboardLetter() { return (char) keyboardLetter; }
	inline unsigned getKeyboardFunction() { return (char) keyboardFunction; }

	// Used by the 2 cores so need to be volatile
	//volatile static unsigned directDiskSwapRequest;
	static unsigned directDiskSwapRequest;
	//volatile static unsigned uartFlags;	// WARNING uncached volatile accessed across cores can be very expensive and may cause the emulation to exceed the 1us time frame and a realtime cycle will not be emulated correctly!
//private:
//	static unsigned escapeSequenceIndex;
};
#endif

