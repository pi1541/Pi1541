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

	inline unsigned int GetDeviceID() const { return deviceID; }
	inline unsigned int GetOnResetChangeToStartingFolder() const { return onResetChangeToStartingFolder; }
	inline const char* GetAutoMountImageName() const { return autoMountImageName; }
	inline const char* GetRomFontName() const { return ROMFontName; }
	const char* GetRomName(int index) const;
	inline const char* GetStarFileName() const { return starFileName; }
	inline unsigned int GetExtraRAM() const { return extraRAM; }
	inline unsigned int GetRAMBOard() const { return RAMBOard; }
	inline unsigned int GetDisableSD2IECCommands() const { return disableSD2IECCommands; }
	inline unsigned int GetSupportUARTInput() const { return supportUARTInput; }

	inline unsigned int GraphIEC() const { return graphIEC; }
	inline unsigned int QuickBoot() const { return quickBoot; }
	inline unsigned int ShowOptions() const { return showOptions; }
	inline unsigned int DisplayPNGIcons() const { return displayPNGIcons; }
	inline unsigned int SoundOnGPIO() const { return soundOnGPIO; }
	inline unsigned int SoundOnGPIODuration() const { return soundOnGPIODuration; }
	inline unsigned int SoundOnGPIOFreq() const { return soundOnGPIOFreq; }
	inline unsigned int SplitIECLines() const { return splitIECLines; }
	inline unsigned int InvertIECInputs() const { return invertIECInputs; }
	inline unsigned int InvertIECOutputs() const { return invertIECOutputs; }
	inline unsigned int IgnoreReset() const { return ignoreReset; }

	inline unsigned int ScreenWidth() const { return screenWidth; }
	inline unsigned int ScreenHeight() const { return screenHeight; }

	inline unsigned int I2CBusMaster() const { return i2cBusMaster; }
	inline unsigned int I2CLcdAddress() const { return i2cLcdAddress; }
	inline unsigned int I2CLcdFlip() const { return i2cLcdFlip; }
	inline unsigned int I2CLcdOnContrast() const { return i2cLcdOnContrast; }
	inline unsigned int I2CLcdDimContrast() const { return i2cLcdDimContrast; }
	inline unsigned int I2CLcdDimTime() const { return i2cLcdDimTime; }
	inline unsigned int I2CLcdModel() const { return i2cLcdModel; }
	inline const char* GetLcdLogoName() const { return LcdLogoName; }

	// Page up and down will jump a different amount based on the maximum number rows displayed.
	// Perhaps we should use some keyboard modifier to the the other screen?
	inline unsigned int KeyboardBrowseLCDScreen() const { return keyboardBrowseLCDScreen; }

	const char* GetLCDName() const { return LCDName; }

	static unsigned GetDecimal(char* pString);
	static float GetFloat(char* pString);

private:
	unsigned int deviceID;
	unsigned int onResetChangeToStartingFolder;
	unsigned int extraRAM;
	unsigned int RAMBOard;
	unsigned int disableSD2IECCommands;
	unsigned int supportUARTInput;
	unsigned int graphIEC;
	unsigned int quickBoot;
	unsigned int showOptions;
	unsigned int displayPNGIcons;
	unsigned int soundOnGPIO;
	unsigned int soundOnGPIODuration;
	unsigned int soundOnGPIOFreq;
	unsigned int invertIECInputs;
	unsigned int invertIECOutputs;
	unsigned int splitIECLines;
	unsigned int ignoreReset;

	unsigned int screenWidth;
	unsigned int screenHeight;

	unsigned int i2cBusMaster;
	unsigned int i2cLcdAddress;
	unsigned int i2cLcdFlip;
	unsigned int i2cLcdOnContrast;
	unsigned int i2cLcdDimContrast;
	unsigned int i2cLcdDimTime;
	unsigned int i2cLcdModel;

	unsigned int keyboardBrowseLCDScreen;

	char starFileName[256];
	char LCDName[256];
	char LcdLogoName[256];

	char autoMountImageName[256];
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
