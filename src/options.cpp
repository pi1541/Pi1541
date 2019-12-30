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

#include "options.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

#define INVALID_VALUE	((unsigned) -1)

char* TextParser::GetToken(bool includeSpace)
{
	bool isSpace;

	do
	{
	} while (ParseComment());

	while (*data != '\0')
	{
		isSpace = isspace(*data);

		if (!isSpace || (includeSpace && isSpace))
			break;

		data++;
	}

	if (*data == '\0')
		return 0;

	char* pToken = data;

	while (*data != '\0')
	{
		isSpace = isspace(*data);

		if ((!includeSpace && isSpace) || (*data == '\n') || (*data == '\r'))
		{
			*data++ = '\0';
			break;
		}
		data++;
	}

	return pToken;
}

void TextParser::SkipWhiteSpace()
{
	while (*data != '\0')
	{
		if (!isspace(*data))
			break;

		data++;
	}
}

bool TextParser::ParseComment()
{
	SkipWhiteSpace();

	if (*data == '\0')
		return 0;

	if (data[0] != '/')
		return false;

	if (data[1] == '/')
	{
		// One line comment
		data += 2;

		while (*data)
		{
			if (*data == '\n')
				break;

			data++;
		}
		SkipWhiteSpace();
		return true;
	}
	else if (data[1] == '*')
	{
		// Multiline comment
		data += 2;

		while (*data)
		{
			if (*data++ == '*' && *data && *data == '/')
			{
				data++;
				break;
			}
		}
		SkipWhiteSpace();
		return true;
	}
	return false;
}


Options::Options(void)
	: TextParser()
	, deviceID(8)
	, onResetChangeToStartingFolder(0)
	, extraRAM(0)
	, RAMBOard(0)
	, disableSD2IECCommands(0)
	, supportUARTInput(0)
	, graphIEC(0)
	, displayTracks(0)
	, quickBoot(0)
	, showOptions(0)
	, displayPNGIcons(0)
	, soundOnGPIO(0)
	, soundOnGPIODuration(1000)
	, soundOnGPIOFreq(1200)
	, invertIECInputs(0)
	, invertIECOutputs(1)
	, splitIECLines(0)
	, ignoreReset(0)
	, autoBootFB128(0)
	, displayTemperature(0)
	, lowercaseBrowseModeFilenames(0)
	, screenWidth(1024)
	, screenHeight(768)
	, i2cBusMaster(1)
	, i2cLcdAddress(0x3C)
	, i2cScan(0)
	, i2cLcdFlip(0)
	, i2cLcdOnContrast(127)
	, i2cLcdUseCBMChar(0)
	, i2cLcdModel(LCD_UNKNOWN)
	, scrollHighlightRate(0.125f)
	, keyboardBrowseLCDScreen(0)
        , buttonEnter(1)
        , buttonUp(2)
        , buttonDown(3)
        , buttonBack(4)
        , buttonInsert(5)
	, rotaryEncoderEnable(0) //ROTARY:

{
	autoMountImageName[0] = 0;
	strcpy(ROMFontName, "chargen");
	strcpy(LcdLogoName, "1541ii");
	strcpy(autoBaseName, "autoname");
	C128BootSectorName[0] = 0;
	starFileName[0] = 0;
	ROMName[0] = 0;
	ROMNameSlot2[0] = 0;
	ROMNameSlot3[0] = 0;
	ROMNameSlot4[0] = 0;
	ROMNameSlot5[0] = 0;
	ROMNameSlot6[0] = 0;
	ROMNameSlot7[0] = 0;
	ROMNameSlot8[0] = 0;
	ROMName1581[0] = 0;
	newDiskType[0] = 0;
}

#define ELSE_CHECK_DECIMAL_OPTION(Name) \
	else if (strcasecmp(pOption, #Name) == 0) \
	{ \
		unsigned nValue = 0; \
		if ((nValue = GetDecimal(pValue)) != INVALID_VALUE) \
			Name = nValue; \
	}
#define ELSE_CHECK_FLOAT_OPTION(Name) \
	else if (strcasecmp(pOption, #Name) == 0) \
	{ \
		float value = 0; \
		if ((value = GetFloat(pValue)) != INVALID_VALUE) \
			Name = value; \
	}

void Options::Process(char* buffer)
{
	SetData(buffer);

	char* pOption;
	while ((pOption = GetToken()) != 0)
	{
		/*char* equals = */GetToken();
		char* pValue = GetToken();

		if ((strcasecmp(pOption, "Font") == 0) || (strcasecmp(pOption, "ChargenFont") == 0))
		{
			strncpy(ROMFontName, pValue, 255);
		}
		else if ((strcasecmp(pOption, "AutoMountImage") == 0))
		{
			strncpy(autoMountImageName, pValue, 255);
		}
		ELSE_CHECK_DECIMAL_OPTION(deviceID)
		ELSE_CHECK_DECIMAL_OPTION(onResetChangeToStartingFolder)
		ELSE_CHECK_DECIMAL_OPTION(extraRAM)
		ELSE_CHECK_DECIMAL_OPTION(RAMBOard)
		ELSE_CHECK_DECIMAL_OPTION(disableSD2IECCommands)
		ELSE_CHECK_DECIMAL_OPTION(supportUARTInput)
		ELSE_CHECK_DECIMAL_OPTION(graphIEC)
		ELSE_CHECK_DECIMAL_OPTION(displayTracks)
		ELSE_CHECK_DECIMAL_OPTION(quickBoot)
		ELSE_CHECK_DECIMAL_OPTION(showOptions)
		ELSE_CHECK_DECIMAL_OPTION(displayPNGIcons)
		ELSE_CHECK_DECIMAL_OPTION(soundOnGPIO)
		ELSE_CHECK_DECIMAL_OPTION(soundOnGPIODuration)
		ELSE_CHECK_DECIMAL_OPTION(soundOnGPIOFreq)
		ELSE_CHECK_DECIMAL_OPTION(invertIECInputs)
		ELSE_CHECK_DECIMAL_OPTION(invertIECOutputs)
		ELSE_CHECK_DECIMAL_OPTION(splitIECLines)
		ELSE_CHECK_DECIMAL_OPTION(ignoreReset)
		ELSE_CHECK_DECIMAL_OPTION(lowercaseBrowseModeFilenames)
		ELSE_CHECK_DECIMAL_OPTION(autoBootFB128)
		ELSE_CHECK_DECIMAL_OPTION(displayTemperature)
		ELSE_CHECK_DECIMAL_OPTION(screenWidth)
		ELSE_CHECK_DECIMAL_OPTION(screenHeight)
		ELSE_CHECK_DECIMAL_OPTION(i2cBusMaster)
		ELSE_CHECK_DECIMAL_OPTION(i2cLcdAddress)
		ELSE_CHECK_DECIMAL_OPTION(i2cScan)
		ELSE_CHECK_DECIMAL_OPTION(i2cLcdFlip)
		ELSE_CHECK_DECIMAL_OPTION(i2cLcdOnContrast)
		ELSE_CHECK_DECIMAL_OPTION(i2cLcdDimContrast)
		ELSE_CHECK_DECIMAL_OPTION(i2cLcdDimTime)
		ELSE_CHECK_DECIMAL_OPTION(i2cLcdUseCBMChar)
		ELSE_CHECK_FLOAT_OPTION(scrollHighlightRate)
		ELSE_CHECK_DECIMAL_OPTION(keyboardBrowseLCDScreen)
		ELSE_CHECK_DECIMAL_OPTION(buttonEnter)
		ELSE_CHECK_DECIMAL_OPTION(buttonUp)
		ELSE_CHECK_DECIMAL_OPTION(buttonDown)
		ELSE_CHECK_DECIMAL_OPTION(buttonBack)
		ELSE_CHECK_DECIMAL_OPTION(buttonInsert)
		ELSE_CHECK_DECIMAL_OPTION(rotaryEncoderEnable) //ROTARY:
		else if ((strcasecmp(pOption, "AutoBaseName") == 0))
		{
			strncpy(autoBaseName, pValue, 255);
		}
		else if ((strcasecmp(pOption, "128BootSectorName") == 0))
		{
			strncpy(C128BootSectorName, pValue, 255);
		}
		else if ((strcasecmp(pOption, "StarFileName") == 0))
		{
			strncpy(starFileName, pValue, 255);
		}
		else if ((strcasecmp(pOption, "LCDLogoName") == 0))
		{
			strncpy(LcdLogoName, pValue, 255);
		}
		else if ((strcasecmp(pOption, "LCDName") == 0))
		{
			strncpy(LCDName, pValue, 255);
			if (strcasecmp(pValue, "ssd1306_128x64") == 0)
				i2cLcdModel = LCD_1306_128x64;
			else if (strcasecmp(pValue, "ssd1306_128x32") == 0)
				i2cLcdModel = LCD_1306_128x32;
			else if (strcasecmp(pValue, "sh1106_128x64") == 0)
				i2cLcdModel = LCD_1106_128x64;
		}
		else if ((strcasecmp(pOption, "ROM1581") == 0))
		{
			strncpy(ROMName1581, pValue, 255);
		}
		else if ((strcasecmp(pOption, "ROM") == 0) || (strcasecmp(pOption, "ROM1") == 0))
		{
			strncpy(ROMName, pValue, 255);
		}
		else if (strcasecmp(pOption, "ROM2") == 0)
		{
			strncpy(ROMNameSlot2, pValue, 255);
		}
		else if (strcasecmp(pOption, "ROM3") == 0)
		{
			strncpy(ROMNameSlot3, pValue, 255);
		}
		else if (strcasecmp(pOption, "ROM4") == 0)
		{
			strncpy(ROMNameSlot4, pValue, 255);
		}
		else if (strcasecmp(pOption, "ROM5") == 0)
		{
			strncpy(ROMNameSlot5, pValue, 255);
		}
		else if (strcasecmp(pOption, "ROM6") == 0)
		{
			strncpy(ROMNameSlot6, pValue, 255);
		}
		else if (strcasecmp(pOption, "ROM7") == 0)
		{
			strncpy(ROMNameSlot7, pValue, 255);
		}
		else if (strcasecmp(pOption, "ROM8") == 0)
		{
			strncpy(ROMNameSlot8, pValue, 255);
		}
		else if ((strcasecmp(pOption, "NewDiskType") == 0))
		{
			strncpy(newDiskType, pValue, 31);
		}
	}

	if (!SplitIECLines())
	{
		invertIECInputs = false;
		// If using non split lines then only the 1st bus master can be used (as ATN is using the 2nd)
		i2cBusMaster = 0;
	}
}

unsigned Options::GetDecimal(char* pString)
{
	if (pString == 0 || *pString == '\0')
		return 0;

	return strtol(pString, NULL, 0);
}

float Options::GetFloat(char* pString)
{
	if (pString == 0 || *pString == '\0')
		return 0;

	return atof(pString);
}

const char* Options::GetRomName(int index) const
{
	switch (index)
	{
	case 1:
		return ROMNameSlot2;
	case 2:
		return ROMNameSlot3;
	case 3:
		return ROMNameSlot4;
	case 4:
		return ROMNameSlot5;
	case 5:
		return ROMNameSlot6;
	case 6:
		return ROMNameSlot7;
	case 7:
		return ROMNameSlot8;
	}
	return ROMName;
}

const char* Options::GetRomName1581() const
{
	if (ROMName1581[0] == 0)
		return "1581-rom.318045-02.bin";
	else
		return ROMName1581;
}

DiskImage::DiskType Options::GetNewDiskType() const
{
	if (strcasecmp(newDiskType, "g64") == 0)
		return DiskImage::G64;

	return DiskImage::D64;
}

