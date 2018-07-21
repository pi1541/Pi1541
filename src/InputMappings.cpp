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

#include "InputMappings.h"
#include "FileBrowser.h"
#include "iec_bus.h"
#include "debug.h"
extern "C"
{
#include "rpi-aux.h"
}

// If disk swaps can be done via multiple cores then directDiskSwapRequest needs to be volatile. WARNING: volatile acesses can be very expensive.
//volatile unsigned InputMappings::directDiskSwapRequest = 0;
unsigned InputMappings::directDiskSwapRequest = 0;
//volatile unsigned InputMappings::uartFlags = 0;
//unsigned InputMappings::escapeSequenceIndex = 0;

InputMappings::InputMappings()
	: keyboardBrowseLCDScreen(false)
{
}

bool InputMappings::CheckButtonsBrowseMode()
{
	buttonFlags = 0;
	if (IEC_Bus::GetInputButtonPressed(0))
		SetButtonFlag(ENTER_FLAG);
	else if (IEC_Bus::GetInputButtonRepeating(1))
		SetButtonFlag(UP_FLAG);
	else if (IEC_Bus::GetInputButtonRepeating(2))
		SetButtonFlag(DOWN_FLAG);
	else if (IEC_Bus::GetInputButtonPressed(3))
		SetButtonFlag(BACK_FLAG);
	else if (IEC_Bus::GetInputButtonPressed(4))
		SetButtonFlag(INSERT_FLAG);

	return buttonFlags != 0;
}

void InputMappings::CheckButtonsEmulationMode()
{
	buttonFlags = 0;

	if (IEC_Bus::GetInputButtonPressed(0))
		SetButtonFlag(ESC_FLAG);
	else if (IEC_Bus::GetInputButtonPressed(1))
		SetButtonFlag(NEXT_FLAG);
	else if (IEC_Bus::GetInputButtonPressed(2))
		SetButtonFlag(PREV_FLAG);
	//else if (IEC_Bus::GetInputButtonPressed(3))
	//	SetButtonFlag(BACK_FLAG);
}


//void InputMappings::CheckUart()
//{
//	char charReceived;
//
//	uartFlags = 0;
//
//	if (RPI_AuxMiniUartRead(&charReceived))
//	{
//		DEBUG_LOG("charReceived=%c %02x\r\n", charReceived, charReceived);
//		if (charReceived == '[')
//		{
//			escapeSequenceIndex++;
//		}
//		else
//		{
//			if (escapeSequenceIndex == 0)
//			{
//				if (charReceived == 27)
//					SetUartFlag(ESC_FLAG);
//				else if (charReceived == 13)
//					SetUartFlag(ENTER_FLAG);
//				else if (charReceived == ' ')
//					SetUartFlag(SPACE_FLAG);
//				else if (charReceived == 0x7f)
//					SetUartFlag(BACK_FLAG);
//				//else if (charReceived == 'u')
//				//	SetUartFlag(UP_FLAG);
//				//else if (charReceived == 'U')
//				//	SetUartFlag(PAGEUP_FLAG);
//				//else if (charReceived == 'd')
//				//	SetUartFlag(DOWN_FLAG);
//				//else if (charReceived == 'D')
//				//	SetUartFlag(PAGEDOWN_FLAG);
//				else
//				{
//					char number = charReceived - '0';
//					if (number >= 0 && number <= 9)
//					{
//						if (number == 0)
//							number = 10;
//						directDiskSwapRequest |= (1 << (number - 1));
//						printf("SWAP %d\r\n", number);
//					}
//				}
//			}
//			else if (escapeSequenceIndex == 1)
//			{
//				if (charReceived == 'A')
//					SetUartFlag(UP_FLAG);
//				else if (charReceived == 'B')
//					SetUartFlag(DOWN_FLAG);
//				else if (charReceived == 'C')
//					SetUartFlag(PAGEDOWN_FLAG);
//				else if (charReceived == 'D')
//					SetUartFlag(PAGEUP_FLAG);
//				else if (charReceived == '2')
//					SetUartFlag(INSERT_FLAG);
//				escapeSequenceIndex = 0;
//			}
//		}
//	}
//}

bool InputMappings::CheckKeyboardBrowseMode()
{
	Keyboard* keyboard = Keyboard::Instance();

	keyboardFlags = 0;

	// TODO: add KEY_HOME and KEY_END
	if (keyboard->KeyHeld(KEY_ESC))
		SetKeyboardFlag(ESC_FLAG);
	else if (keyboard->KeyHeld(KEY_ENTER))
		SetKeyboardFlag(ENTER_FLAG);
	else if (keyboard->KeyHeld(KEY_BACKSPACE))
		SetKeyboardFlag(BACK_FLAG);
	else if (keyboard->KeyHeld(KEY_SPACE))
		SetKeyboardFlag(SPACE_FLAG);
	else if (keyboard->KeyHeld(KEY_INSERT))
		SetKeyboardFlag(INSERT_FLAG);
	else if (keyboard->KeyHeld(KEY_UP))
		SetKeyboardFlag(UP_FLAG);
	else if (keyboard->KeyHeld(KEY_PAGEUP) || keyboard->KeyHeld(KEY_LEFT))
	{
		if (keyboardBrowseLCDScreen)
			SetKeyboardFlag(PAGEUP_LCD_FLAG);
		else
			SetKeyboardFlag(PAGEUP_FLAG);
	}
	else if (keyboard->KeyHeld(KEY_DOWN))
		SetKeyboardFlag(DOWN_FLAG);
	else if (keyboard->KeyHeld(KEY_PAGEDOWN) || keyboard->KeyHeld(KEY_RIGHT))
	{
		if (keyboardBrowseLCDScreen)
			SetKeyboardFlag(PAGEDOWN_LCD_FLAG);
		else
			SetKeyboardFlag(PAGEDOWN_FLAG);
	}
	//else if (keyboard->KeyHeld(KEY_HOME))
	//	SetKeyboardFlag(PAGEUP_LCD_FLAG);
	//else if (keyboard->KeyHeld(KEY_END))
	//	SetKeyboardFlag(PAGEDOWN_LCD_FLAG);
	else
	{
		unsigned index;
		for (index = 0; index < 11; ++index)
		{
			unsigned keySetIndexBase = index * 3;
			if (keyboard->KeyHeld(FileBrowser::SwapKeys[keySetIndexBase]) || keyboard->KeyHeld(FileBrowser::SwapKeys[keySetIndexBase + 1]) || keyboard->KeyHeld(FileBrowser::SwapKeys[keySetIndexBase + 2]))
				keyboardFlags |= NUMBER_FLAG;
		}
	}

	return keyboardFlags != 0;
}

void InputMappings::CheckKeyboardEmulationMode(unsigned numberOfImages, unsigned numberOfImagesMax)
{
	Keyboard* keyboard = Keyboard::Instance();

	keyboardFlags = 0;
	if (keyboard->CheckChanged())
	{
		if (keyboard->KeyHeld(KEY_ESC))
			SetKeyboardFlag(ESC_FLAG);
		else if (keyboard->KeyHeld(KEY_PAGEUP))
			SetKeyboardFlag(PREV_FLAG);
		else if (keyboard->KeyHeld(KEY_PAGEDOWN))
			SetKeyboardFlag(NEXT_FLAG);
		else if (numberOfImages > 1)
		{
			unsigned index;
			for (index = 0; index < numberOfImagesMax; ++index)
			{
				unsigned keySetIndexBase = index * 3;
				if (keyboard->KeyHeld(FileBrowser::SwapKeys[keySetIndexBase]) || keyboard->KeyHeld(FileBrowser::SwapKeys[keySetIndexBase + 1]) || keyboard->KeyHeld(FileBrowser::SwapKeys[keySetIndexBase + 2]))
					directDiskSwapRequest |= (1 << index);
			}
		}
	}
}
