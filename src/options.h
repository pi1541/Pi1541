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

#ifndef OPTIONS_H
#define OPTIONS_H

class TextParser
{
public:
	TextParser(void)
		: data(0)
	{
	}

	void SetData(char* buffer) { data = buffer; }

	char* GetToken(bool includeSpace = false);

protected:
	char* data;
	bool ParseComment();
	void SkipWhiteSpace();
};

class Options : public TextParser
{
public:
	Options(void);

	void Process(char* buffer);

	unsigned int GetDeviceID() const { return deviceID; }
	unsigned int GetOnResetChangeToStartingFolder() const { return onResetChangeToStartingFolder; }
	const char* GetRomFontName() const { return ROMFontName; }
	const char* GetRomName(int index) const;
	unsigned int GetExtraRAM() const { return extraRAM; }
	unsigned int GetRAMBOard() const { return enableRAMBOard; }
	unsigned int GetDisableSD2IECCommands() const { return disableSD2IECCommands; }
	unsigned int GetSupportUARTInput() const { return supportUARTInput; }

	unsigned int GraphIEC() const { return graphIEC; }
	unsigned int QuickBoot() const { return quickBoot; }
	unsigned int DisplayPNGIcons() const { return displayPNGIcons; }
	unsigned int SoundOnGPIO() const { return soundOnGPIO; }
	unsigned int SplitIECLines() const { return splitIECLines; }
	unsigned int InvertIECInputs() const { return invertIECInputs; }
	unsigned int InvertIECOutputs() const { return invertIECOutputs; }
	unsigned int IgnoreReset() const { return ignoreReset; }

	unsigned int ScreenWidth() const { return screenWidth; }
	unsigned int ScreenHeight() const { return screenHeight; }

	static unsigned GetDecimal(char* pString);

private:
	unsigned int deviceID;
	unsigned int onResetChangeToStartingFolder;
	unsigned int extraRAM;
	unsigned int enableRAMBOard;
	unsigned int disableSD2IECCommands;
	unsigned int supportUARTInput;
	unsigned int graphIEC;
	unsigned int quickBoot;
	unsigned int displayPNGIcons;
	unsigned int soundOnGPIO;
	unsigned int invertIECInputs;
	unsigned int invertIECOutputs;
	unsigned int splitIECLines;
	unsigned int ignoreReset;

	unsigned int screenWidth;
	unsigned int screenHeight;

	char ROMFontName[256];
	char ROMName[256];
	char ROMNameSlot2[256];
	char ROMNameSlot3[256];
	char ROMNameSlot4[256];
	char ROMNameSlot5[256];
	char ROMNameSlot6[256];
	char ROMNameSlot7[256];
	char ROMNameSlot8[256];
};
#endif
