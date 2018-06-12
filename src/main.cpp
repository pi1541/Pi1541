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

#include "defs.h"
#include <string.h>
#include <strings.h>
#include "Timer.h"
#include "ROMs.h"
#include "stb_image.h"
extern "C"
{
#include "rpi-aux.h"
#include "rpi-i2c.h"
#include "rpi-gpio.h"
#include "startup.h"
#include "cache.h"
#include "rpi-mailbox-interface.h"
#include "interrupt.h"
#include <uspi.h>
}
#include "InputMappings.h"
#include "options.h"
#include "iec_commands.h"
#include "diskio.h"
#include "Pi1541.h"
#include "FileBrowser.h"
#include "ScreenLCD.h"

#include "logo.h"
#include "sample.h"

unsigned versionMajor = 1;
unsigned versionMinor = 5;

// When the emulated CPU starts we execute the first million odd cycles in non-real-time (ie as fast as possible so the emulated 1541 becomes responsive to CBM-Browser asap)
// During these cycles the CPU is executing the ROM self test routines (these do not need to be cycle accurate)
#define FAST_BOOT_CYCLES 1003061

#define COLOUR_BLACK RGBA(0, 0, 0, 0xff)
#define COLOUR_WHITE RGBA(0xff, 0xff, 0xff, 0xff)
#define COLOUR_RED RGBA(0xff, 0, 0, 0xff)
#define COLOUR_GREEN RGBA(0, 0xff, 0, 0xff)
#define COLOUR_CYAN RGBA(0, 0xff, 0xff, 0xff)
#define COLOUR_YELLOW RGBA(0xff, 0xff, 0x00, 0xff)

// To exit a mounted disk image we need to watch(snoop) what the emulated CPU is doing when it executes code at some critical ROM addresses.
#define SNOOP_CD_CBM 0xEA2D
#define SNOOP_CD_JIFFY_BOTH 0xFC07
#define SNOOP_CD_JIFFY_DRIVEONLY 0xEA16
static const u8 snoopBackCommand[] = {
	'C', 'D', ':', '_'	//0x43, 0x44, 0x3a, 0x5f
};
static int snoopIndex = 0;
static int snoopPC = 0;

bool emulating; // When false we are in IEC command mode level simulation. When true we are in full cycle exact emulation mode.

typedef void(*func_ptr)();

const long int tempBufferSize = 1024;
char tempBuffer[tempBufferSize];
ROMs roms;

const long int CBMFont_size = 4096;
unsigned char CBMFontData[4096];
unsigned char* CBMFont = 0;

u8 s_u8Memory[0xc000];

DiskCaddy diskCaddy;
Pi1541 pi1541;
CEMMCDevice	m_EMMC;
Screen screen;
ScreenLCD* screenLCD = 0;
Options options;
const char* fileBrowserSelectedName;
u8 deviceID = 8;
IEC_Commands m_IEC_Commands;

unsigned int screenWidth = 1024;
unsigned int screenHeight = 768;

const char* termainalTextRed = "\E[31m";
const char* termainalTextNormal = "\E[0m";

// Hooks required for USPi library
extern "C"
{
	void LogWrite(const char *pSource, unsigned Severity, const char *pMessage, ...)
	{
		va_list args;
		va_start(args, pMessage);
		vprintf(pMessage, args);
		va_end(args);
	}

	int GetMACAddress(unsigned char Buffer[6])
	{
		rpi_mailbox_property_t* mp;

		RPI_PropertyInit();
		RPI_PropertyAddTag(TAG_GET_BOARD_MAC_ADDRESS);
		RPI_PropertyProcess();

		if ((mp = RPI_PropertyGet(TAG_GET_BOARD_MAC_ADDRESS)))
		{
			for (int i = 0; i < 6; ++i)
			{
				Buffer[i] = mp->data.buffer_8[i];
			}
			return 1;
		}

		return 0;
	}

	int SetPowerStateOn(unsigned id)
	{
		volatile u32* mailbox;
		u32 result;

		mailbox = (u32*)(PERIPHERAL_BASE | 0xB880);
		while (mailbox[6] & 0x80000000);
		mailbox[8] = 0x80;
		do {
			while (mailbox[6] & 0x40000000);
		} while (((result = mailbox[0]) & 0xf) != 0);
		return result == 0x80;
	}

	void usDelay(unsigned nMicroSeconds)
	{
		unsigned before;
		unsigned after;
		for (u32 count = 0; count < nMicroSeconds; ++count)
		{
			before = read32(ARM_SYSTIMER_CLO);
			do
			{
				after = read32(ARM_SYSTIMER_CLO);
			} while (after == before);
		}
	}

	void MsDelay(unsigned nMilliSeconds)
	{
		usDelay(nMilliSeconds * 1000);
	}

	unsigned StartKernelTimer(unsigned nHzDelay, TKernelTimerHandler* pHandler, void* pParam, void* pContext)
	{
		return TimerStartKernelTimer(nHzDelay, pHandler, pParam, pContext);
	}

	void CancelKernelTimer(unsigned hTimer)
	{
		TimerCancelKernelTimer(hTimer);
	}

	typedef void TInterruptHandler(void* pParam);

	// USPi uses USB IRQ 9
	void ConnectInterrupt(unsigned nIRQ, TInterruptHandler* pHandler, void *pParam)
	{
		InterruptSystemConnectIRQ(nIRQ, pHandler, pParam);
	}
}

// Hooks for FatFs
DWORD get_fattime() { return 0; }	// If you have hardware RTC return a correct value here. THis can then be reflected in file modification times/dates.

///////////////////////////////////////////////////////////////////////////////////////
// 6502 Address bus functions.
// Move here out of Pi1541 to increase performance.
///////////////////////////////////////////////////////////////////////////////////////
// In a 1541 address decoding and chip selects are performed by a 74LS42 ONE-OF-TEN DECODER
// 74LS42 Ouputs a low to the !CS based on the four inputs provided by address bits 10-13
// 1800 !cs2 on pin 9
// 1c00 !cs2 on pin 7
u8 read6502(u16 address)
{
	u8 value = 0;
	if (address & 0x8000)
	{
		switch (address & 0xe000) // keep bits 15,14,13
		{
			case 0x8000: // 0x8000-0x9fff
				if (options.GetRAMBOard()) {
					value = s_u8Memory[address]; // 74LS42 outputs low on pin 1 or pin 2
					break;
				}
			case 0xa000: // 0xa000-0xbfff
			case 0xc000: // 0xc000-0xdfff
			case 0xe000: // 0xe000-0xffff
				value = roms.Read(address);
				break;
		}
	}
	else
	{
		// Address lines 15, 12, 11 and 10 are fed into a 74LS42 for decoding
		u16 addressLines12_11_10 = (address & 0x1c00) >> 10;
		switch (addressLines12_11_10)
		{
			case 0:
			case 1:
				value = s_u8Memory[address & 0x7ff]; // 74LS42 outputs low on pin 1 or pin 2
				break;
			case 6:
				value = pi1541.VIA[0].Read(address);	// 74LS42 outputs low on pin 7
				break;
			case 7:
				value = pi1541.VIA[1].Read(address);	// 74LS42 outputs low on pin 9
				break;
			default:
				value = address >> 8;	// Empty address bus
				break;
		}
	}
	return value;
}

// Allows a mode where we have RAM at all addresses other than the ROM and the VIAs. (Maybe useful to someone?)
u8 read6502ExtraRAM(u16 address)
{
	if (address & 0x8000)
	{
		return roms.Read(address);
	}
	else
	{
		u16 addressLines11And12 = address & 0x1800;
		if (addressLines11And12 == 0x1800) return pi1541.VIA[(address & 0x400) != 0].Read(address);	// address line 10 indicates what VIA to index
		return s_u8Memory[address & 0x7fff];
	}
}

// Use for debugging (Reads VIA registers without the regular VIA read side effects)
u8 peek6502(u16 address)
{
	u8 value;
	if (address & 0x8000)	// address line 15 selects the ROM
	{
		value = roms.Read(address);
	}
	else
	{
		// Address lines 15, 12, 11 and 10 are fed into a 74LS42 for decoding
		u16 addressLines15_12_11_10 = (address & 0x1c00) >> 10;
		addressLines15_12_11_10 |= (address & 0x8000) >> (15 - 3);
		if (addressLines15_12_11_10 == 0 || addressLines15_12_11_10 == 1) value = s_u8Memory[address & 0x7ff]; // 74LS42 outputs low on pin 1 or pin 2
		else if (addressLines15_12_11_10 == 6) value = pi1541.VIA[0].Peek(address);	// 74LS42 outputs low on pin 7
		else if (addressLines15_12_11_10 == 7) value = pi1541.VIA[1].Peek(address);	// 74LS42 outputs low on pin 9
		else value = address >> 8;	// Empty address bus
	}
	return value;
}

void write6502(u16 address, const u8 value)
{
	if (address & 0x8000)
	{
		switch (address & 0xe000) // keep bits 15,14,13
		{
			case 0x8000: // 0x8000-0x9fff
				if (options.GetRAMBOard()) {
					s_u8Memory[address] = value; // 74LS42 outputs low on pin 1 or pin 2
					break;
				}
			case 0xa000: // 0xa000-0xbfff
			case 0xc000: // 0xc000-0xdfff
			case 0xe000: // 0xe000-0xffff
				return;
		}
	}
	else
	{
		// Address lines 15, 12, 11 and 10 are fed into a 74LS42 for decoding
		u16 addressLines12_11_10 = (address & 0x1c00) >> 10;
		switch (addressLines12_11_10)
		{
			case 0:
			case 1:
				s_u8Memory[address & 0x7ff] = value; // 74LS42 outputs low on pin 1 or pin 2
				break;
			case 6:
				pi1541.VIA[0].Write(address, value);	// 74LS42 outputs low on pin 7
				break;
			case 7:
				pi1541.VIA[1].Write(address, value);	// 74LS42 outputs low on pin 9
				break;
			default:
				break;
		}
	}
}

void write6502ExtraRAM(u16 address, const u8 value)
{
	if (address & 0x8000) return; // address line 15 selects the ROM
	u16 addressLines11And12 = address & 0x1800;
	if (addressLines11And12 == 0) s_u8Memory[address & 0x7fff] = value;
	else if (addressLines11And12 == 0x1800) pi1541.VIA[(address & 0x400) != 0].Write(address, value);	// address line 10 indicates what VIA to index
}

void InitialiseHardware()
{
#if defined(RPI3)
	RPI_GpioVirtInit();
	RPI_TouchInit();
#endif
	RPI_AuxMiniUartInit(115200, 8);

	screen.Open(screenWidth, screenHeight, 16);

	RPI_PropertyInit();
	RPI_PropertyAddTag(TAG_GET_MAX_CLOCK_RATE, ARM_CLK_ID);
	RPI_PropertyProcess();

	rpi_mailbox_property_t* mp;
	u32 MaxClk = 0;
	if ((mp = RPI_PropertyGet(TAG_GET_MAX_CLOCK_RATE)))
	{
		MaxClk = mp->data.buffer_32[1];
	}
	RPI_PropertyInit();
	RPI_PropertyAddTag(TAG_SET_CLOCK_RATE, ARM_CLK_ID, MaxClk);
	RPI_PropertyProcess();
}

//void UpdateUartControls(bool refreshStatusDisplay, bool LED, bool Motor, bool ATN, bool DATA, bool CLOCK, u32 Track, u32 romIndex)
//{
//	//InputMappings* inputMappings = InputMappings::Instance();
//
//	//if (emulating)
//	//	inputMappings->CheckUart();
//
//	if (refreshStatusDisplay && emulating)
//		printf("\E[1ALED %s%d\E[0m Motor %d Track %0d.%d ATN %d DAT %d CLK %d %s\r\n", LED ? termainalTextRed : termainalTextNormal, LED, Motor, Track >> 1, Track & 1 ? 5 : 0, ATN, DATA, CLOCK, roms.ROMNames[romIndex]);
//}

// This runs on core0 and frees up core1 to just run the emulator.
// Care must be taken not to crowd out the shared cache with core1 as this could slow down core1 so that it no longer can perform its duties in the 1us timings it requires.
void UpdateScreen()
{
	bool oldLED = false;
	bool oldMotor = false;
	bool oldATN = false;
	bool oldDATA = false;
	bool oldCLOCK = false;

	u32 oldTrack = 0;
	u32 textColour = COLOUR_BLACK;
	u32 bgColour = COLOUR_WHITE;

	RGBA dataColour = COLOUR_GREEN;
	RGBA clockColour = COLOUR_CYAN;
	RGBA atnColour = COLOUR_YELLOW;
	RGBA BkColour = FileBrowser::Colour(VIC2_COLOUR_INDEX_BLUE);

	int height = screen.ScaleY(60);
	int screenHeight = screen.Height();
	int screenWidthM1 = screen.Width() - 1;
	int top, top2, top3;
	int bottom;
	int graphX = 0;
	//bool refreshUartStatusDisplay;

	top = screenHeight - height / 2;
	bottom = screenHeight - 1;

	top2 = top - (bottom - top);
	top3 = top2 - (bottom - top);

	while (1)
	{
		bool value;
		u32 y = screen.ScaleY(STATUS_BAR_POSITION_Y);

		//RPI_UpdateTouch();
		//refreshUartStatusDisplay = false;

		value = pi1541.drive.IsLEDOn();
		if (value != oldLED)
		{
//			SetACTLed(value);
			oldLED = value;
			snprintf(tempBuffer, tempBufferSize, "%d", value);
			screen.PrintText(false, 4 * 8, y, tempBuffer, value ? COLOUR_RED : textColour, bgColour);
			//refreshUartStatusDisplay = true;
		}

		value = pi1541.drive.IsMotorOn();
		if (value != oldMotor)
		{
			oldMotor = value;
			snprintf(tempBuffer, tempBufferSize, "%d", value);
			screen.PrintText(false, 12 * 8, y, tempBuffer, textColour, bgColour);
			//refreshUartStatusDisplay = true;
		}

		value = IEC_Bus::GetPI_Atn();
		if (options.GraphIEC())
		{
			bottom = top2 - 2;
			if (value ^ oldATN)
			{
				screen.DrawLineV(graphX, top3, bottom, atnColour);
			}
			else
			{
				screen.DrawLineV(graphX, top3, bottom, BkColour);
				if (value) screen.PlotPixel(graphX, top3, atnColour);
				else screen.PlotPixel(graphX, bottom, atnColour);
			}
		}
		if (value != oldATN)
		{
			oldATN = value;
			snprintf(tempBuffer, tempBufferSize, "%d", value);
			screen.PrintText(false, 29 * 8, y, tempBuffer, textColour, bgColour);
			//refreshUartStatusDisplay = true;
		}

		value = IEC_Bus::GetPI_Data();
		if (options.GraphIEC())
		{
			bottom = top - 2;
			if (value ^ oldDATA)
			{
				screen.DrawLineV(graphX, top2, bottom, dataColour);
			}
			else
			{
				screen.DrawLineV(graphX, top2, bottom, BkColour);
				if (value) screen.PlotPixel(graphX, top2, dataColour);
				else screen.PlotPixel(graphX, bottom, dataColour);
			}
		}
		if (value != oldDATA)
		{
			oldDATA = value;
			snprintf(tempBuffer, tempBufferSize, "%d", value);
			screen.PrintText(false, 35 * 8, y, tempBuffer, textColour, bgColour);
			//refreshUartStatusDisplay = true;
		}

		value = IEC_Bus::GetPI_Clock();
		if (options.GraphIEC())
		{
			bottom = screenHeight - 1;
			if (value ^ oldCLOCK)
			{
				screen.DrawLineV(graphX, top, bottom, clockColour);
			}
			else
			{
				screen.DrawLineV(graphX, top, bottom, BkColour);
				if (value) screen.PlotPixel(graphX, top, clockColour);
				else screen.PlotPixel(graphX, bottom, clockColour);
			}
		}
		if (value != oldCLOCK)
		{
			oldCLOCK = value;
			snprintf(tempBuffer, tempBufferSize, "%d", value);
			screen.PrintText(false, 41 * 8, y, tempBuffer, textColour, bgColour);
			//refreshUartStatusDisplay = true;
		}

		if (graphX++ > screenWidthM1) graphX = 0;

		u32 track = pi1541.drive.Track();
		if (track != oldTrack)
		{
			oldTrack = track;
			snprintf(tempBuffer, tempBufferSize, "%02d.%d", (oldTrack >> 1) + 1, oldTrack & 1 ? 5 : 0);
			screen.PrintText(false, 20 * 8, y, tempBuffer, textColour, bgColour);
			//refreshUartStatusDisplay = true;

			if (screenLCD)
			{
				screenLCD->PrintText(false, 0, 0, tempBuffer, RGBA(0xff, 0xff, 0xff, 0xff), RGBA(0xff, 0xff, 0xff, 0xff));
				screenLCD->RefreshRows(0, 2); //SwapBuffers();
			}

		}

		if (emulating)
		{
			//refreshUartStatusDisplay =
				diskCaddy.Update();
		}

		//if (options.GetSupportUARTInput())
		//	UpdateUartControls(refreshUartStatusDisplay, oldLED, oldMotor, oldATN, oldDATA, oldCLOCK, oldTrack, romIndex);

		// Go back to sleep. The USB irq will wake us up again.
		__asm ("WFE");
	}
}

bool BeginEmulating(FileBrowser* fileBrowser, const char* filenameForIcon)
{
	DiskImage* diskImage = diskCaddy.SelectFirstImage();
	if (diskImage)
	{
		pi1541.drive.Insert(diskImage);
		fileBrowser->DisplayDiskInfo(diskImage, filenameForIcon);
		fileBrowser->ShowDeviceAndROM();
		return true;
	}
	return false;
}

static u32* dmaSound;

struct DMA_ControlBlock
{
	u32 transferInformation;
	u32* sourceAddress;
	u32 destinationAddress;
	u32 transferLength;
	u32 stride;
	DMA_ControlBlock* nextControlBlock;
	u32 res0, res1;
} __attribute__((aligned(32)));

DMA_ControlBlock dmaSoundCB =
{
	DMA_DEST_DREQ + DMA_PERMAP_5 + DMA_SRC_INC,
	0,
	//0x7E000000 + PWM_BASE + PWM_FIF1
	0x7E000000 + 0x20C000 + 0x18,
	(u32)(Sample_bin_size * 4),
	0,
	0,//&dmaSoundCB,
	0, 0
};

static void PlaySoundDMA()
{
	write32(PWM_DMAC, PWM_ENAB + 0x0001);
	write32(DMA_ENABLE, 1);	// DMA_EN0
	write32(DMA0_BASE + DMA_CONBLK_AD, (u32)&dmaSoundCB);
	write32(DMA0_BASE + DMA_CS, DMA_ACTIVE);
}

static void SetVIAsDeviceID(u8 id)
{
	if (id & 1) pi1541.VIA[0].GetPortB()->SetInput(VIAPORTPINS_DEVSEL0, true);
	if (id & 2) pi1541.VIA[0].GetPortB()->SetInput(VIAPORTPINS_DEVSEL1, true);
}

void emulator()
{
	bool oldLED = false;
	unsigned ctBefore = 0;
	unsigned ctAfter = 0;
	Keyboard* keyboard = Keyboard::Instance();
	bool resetWhileEmulating = false;
	bool selectedViaIECCommands = false;
	InputMappings* inputMappings = InputMappings::Instance();
	FileBrowser* fileBrowser;

	roms.lastManualSelectedROMIndex = 0;

	diskCaddy.SetScreen(&screen, screenLCD);
	fileBrowser = new FileBrowser(&diskCaddy, &roms, deviceID, options.DisplayPNGIcons(), &screen, screenLCD);
	fileBrowser->DisplayRoot();
	pi1541.Initialise();

	emulating = false;

	while (1)
	{
		if (!emulating)
		{
			IEC_Bus::VIA = 0;

			roms.ResetCurrentROMIndex();
			fileBrowser->ClearScreen();
			IEC_Bus::Reset();

			fileBrowserSelectedName = 0;
			fileBrowser->ClearSelections();

			// Go back to the root folder so you can load fb* again?
			if ((resetWhileEmulating && options.GetOnResetChangeToStartingFolder()) || selectedViaIECCommands) fileBrowser->DisplayRoot(); // Go back to the root folder and display it.
			else fileBrowser->RefeshDisplay(); // Just redisplay the current folder.

			resetWhileEmulating = false;
			selectedViaIECCommands = false;

			inputMappings->Reset();
			inputMappings->SetKeyboardBrowseLCDScreen(screenLCD && options.KeyboardBrowseLCDScreen());

			fileBrowser->ShowDeviceAndROM();

			if (!options.GetDisableSD2IECCommands())
			{
				m_IEC_Commands.SimulateIECBegin();

				while (!emulating)
				{
					IEC_Commands::UpdateAction updateAction = m_IEC_Commands.SimulateIECUpdate();

					switch (updateAction)
					{
						case IEC_Commands::RESET:
							if (options.GetOnResetChangeToStartingFolder())
								fileBrowser->DisplayRoot();
							IEC_Bus::Reset();
							m_IEC_Commands.SimulateIECBegin();
							break;
						case IEC_Commands::NONE:
							{
								//fileBrowser->AutoSelectTestImage();
								fileBrowser->UpdateInput();

								// Check selections made via FileBrowser
								if (fileBrowser->SelectionsMade())
									emulating = BeginEmulating(fileBrowser, fileBrowser->LastSelectionName());
							}
							break;
						case IEC_Commands::IMAGE_SELECTED:
						{
							// Check selections made via FileBrowser

							fileBrowserSelectedName = m_IEC_Commands.GetNameOfImageSelected();

							if (DiskImage::IsLSTExtention(fileBrowserSelectedName))
							{
								if (fileBrowser->SelectLST(fileBrowserSelectedName))
								{
									emulating = BeginEmulating(fileBrowser, fileBrowserSelectedName);
								}
								else
								{
									m_IEC_Commands.Reset();
									fileBrowserSelectedName = 0;
								}
							}
							else if (DiskImage::IsDiskImageExtention(fileBrowserSelectedName))
							{
								const FILINFO* filInfoSelected = m_IEC_Commands.GetImageSelected();
								DEBUG_LOG("IEC mounting %s\r\n", filInfoSelected->fname);
								bool readOnly = (filInfoSelected->fattrib & AM_RDO) != 0;

								if (diskCaddy.Insert(filInfoSelected, readOnly)) emulating = BeginEmulating(fileBrowser, filInfoSelected->fname);
								else fileBrowserSelectedName = 0;
							}
							else
							{
								fileBrowserSelectedName = 0;
							}

							if (fileBrowserSelectedName == 0)
								m_IEC_Commands.Reset();

							selectedViaIECCommands = true;
						}
						break;
						case IEC_Commands::DIR_PUSHED:
							fileBrowser->FolderChanged();
							break;
						case IEC_Commands::POP_DIR:
							fileBrowser->PopFolder();
							fileBrowser->RefeshDisplay();
							break;
						case IEC_Commands::POP_TO_ROOT:
							fileBrowser->DisplayRoot();
							break;
						case IEC_Commands::REFRESH:
							fileBrowser->FolderChanged();
							break;
						case IEC_Commands::DEVICEID_CHANGED:
							deviceID = m_IEC_Commands.GetDeviceId();
							fileBrowser->SetDeviceID(deviceID);
							fileBrowser->ShowDeviceAndROM();
							SetVIAsDeviceID(deviceID);	// Let the emilated VIA know
							break;
						default:
							break;
					}
				}
			}
			else
			{
				while (!emulating)
				{
					if (keyboard->CheckChanged())
					{
						fileBrowser->UpdateInput();
						if (fileBrowser->SelectionsMade())
							emulating = BeginEmulating(fileBrowser, fileBrowser->LastSelectionName());
					}
				}
			}
		}
		else
		{
			int cycleCount = 0;
			unsigned caddyIndex;
			int headSoundCounter = 0;
			int headSoundFreqCounter = 0;
			//const int headSoundFreq = 3333;	// 300Hz = 1/300 * 10^6;
			//const int headSoundFreq = 1666;	// 600Hz = 1/600 * 10^6;
//			const int headSoundFreq = 833;	// 1200Hz = 1/1200 * 10^6;
			const int headSoundFreq = options.SoundOnGPIOFreq();	// 1200Hz = 1/1200 * 10^6;
			unsigned char oldHeadDir;

			unsigned numberOfImages = diskCaddy.GetNumberOfImages();
			unsigned numberOfImagesMax = numberOfImages;
			if (numberOfImagesMax > 10)
				numberOfImagesMax = 10;

			diskCaddy.Display();

			inputMappings->directDiskSwapRequest = 0;
			// Force an update on all the buttons now before we start emulation mode. 
			IEC_Bus::ReadBrowseMode();

			bool extraRAM = options.GetExtraRAM();
			DataBusReadFn dataBusRead = extraRAM ? read6502ExtraRAM : read6502;
			DataBusWriteFn dataBusWrite = extraRAM ? write6502ExtraRAM : write6502;
			pi1541.m6502.SetBusFunctions(dataBusRead, dataBusWrite);

			IEC_Bus::VIA = &pi1541.VIA[0];
			pi1541.Reset();	// will call IEC_Bus::Reset();

			ctBefore = read32(ARM_SYSTIMER_CLO);

			while (1)
			{
				IEC_Bus::ReadEmulationMode();

				if (pi1541.m6502.SYNC())	// About to start a new instruction.
				{
					u16 pc = pi1541.m6502.GetPC();
					// See if the emulated cpu is executing CD:_ (ie back out of emulated image)
					if (snoopIndex == 0 && (pc == SNOOP_CD_CBM || pc == SNOOP_CD_JIFFY_BOTH || pc == SNOOP_CD_JIFFY_DRIVEONLY)) snoopPC = pc;

					if (pc == snoopPC)
					{
						u8 a = pi1541.m6502.GetA();
						if (a == snoopBackCommand[snoopIndex])
						{
							snoopIndex++;
							if (snoopIndex == sizeof(snoopBackCommand))
							{
								// Exit full emulation back to IEC commands level simulation.
								snoopIndex = 0;
								emulating = false;
								IEC_Bus::Reset(); // TO CHECK - remove this
								break;
							}
						}
						else
						{
							snoopIndex = 0;
							snoopPC = 0;
						}
					}
				}

				pi1541.m6502.Step();	// If the CPU reads or writes to the VIA then clk and data can change

				if (cycleCount >= FAST_BOOT_CYCLES)	// cycleCount is used so we can quickly get through 1541's self test code. This will make the emulated 1541 responsive to commands asap. During this time we don't need to set outputs.
				{
					//To artificialy delay the outputs later into the phi2's cycle (do this on future Pis that will be faster and perhaps too fast)
					//read32(ARM_SYSTIMER_CLO);	//Each one of these is > 100ns
					//read32(ARM_SYSTIMER_CLO);
					//read32(ARM_SYSTIMER_CLO);

					IEC_Bus::OutputLED = pi1541.drive.IsLEDOn();
					if (IEC_Bus::OutputLED ^ oldLED)
					{
						SetACTLed(IEC_Bus::OutputLED);
						oldLED = IEC_Bus::OutputLED;
					}

					// Do head moving sound
					unsigned char headDir = pi1541.drive.GetLastHeadDirection();
					if (headDir ^ oldHeadDir)	// Need to start a new sound?
					{
						oldHeadDir = headDir;
						if (options.SoundOnGPIO())
						{
							headSoundCounter = options.SoundOnGPIOCounter();
							headSoundFreqCounter = headSoundFreq;
						}
						else
						{
							PlaySoundDMA();
						}
					}

					if (options.SoundOnGPIO() && headSoundCounter > 0)
					{
						headSoundFreqCounter--;		// Continue updating a GPIO non DMA sound.
						if (headSoundFreqCounter <= 0)
						{
							headSoundFreqCounter = headSoundFreq;
							headSoundCounter -= headSoundFreq * 8;
							IEC_Bus::OutputSound = !IEC_Bus::OutputSound;
						}
					}

					IEC_Bus::RefreshOuts();	// Now output all outputs.
				}

				// We have now output so HERE is where the next phi2 cycle starts.
				pi1541.Update();

				// Other core will check the uart (as it is slow) (could enable uart irqs - will they execute on this core?)
				inputMappings->CheckKeyboardEmulationMode(numberOfImages, numberOfImagesMax);
				inputMappings->CheckButtonsEmulationMode();

				bool exitEmulation = inputMappings->Exit();
				bool nextDisk = inputMappings->NextDisk();
				bool prevDisk = inputMappings->PrevDisk();

				if (nextDisk)
				{
					pi1541.drive.Insert(diskCaddy.PrevDisk());
				}
				else if (prevDisk)
				{
					pi1541.drive.Insert(diskCaddy.NextDisk());
				}
				else if (numberOfImages > 1 && inputMappings->directDiskSwapRequest != 0)
				{
					for (caddyIndex = 0; caddyIndex < numberOfImagesMax; ++caddyIndex)
					{
						if (inputMappings->directDiskSwapRequest & (1 << caddyIndex))
						{
							DiskImage* diskImage = diskCaddy.SelectImage(caddyIndex);
							if (diskImage && diskImage != pi1541.drive.GetDiskImage())
							{
								pi1541.drive.Insert(diskImage);
								break;
							}
						}
					}
					inputMappings->directDiskSwapRequest = 0;
				}

				bool reset = IEC_Bus::IsReset();
				if (reset || exitEmulation)
				{
					// Clearing the caddy now
					//	- will write back all changed/dirty/written to disk images now
					//		- TDOO: need to display the image names as they write back
					//	- pass in a call back function?
					diskCaddy.Empty();
					IEC_Bus::WaitMicroSeconds(2 * 1000000);

					fileBrowser->ClearSelections();
					fileBrowser->RefeshDisplay(); // Just redisplay the current folder.

					IEC_Bus::WaitUntilReset();
					//DEBUG_LOG("6502 resetting\r\n");
					if (options.GetOnResetChangeToStartingFolder() || selectedViaIECCommands)
						fileBrowser->DisplayRoot();//m_IEC_Commands.ChangeToRoot(); // TO CHECK
					emulating = false;
					resetWhileEmulating = true;
					break;

				}

				if (cycleCount < FAST_BOOT_CYCLES)	// cycleCount is used so we can quickly get through 1541's self test code. This will make the emulated 1541 responsive to commands asap.
				{
					cycleCount++;
					ctAfter = read32(ARM_SYSTIMER_CLO);
				}
				else
				{
					do	// Sync to the 1MHz clock
					{
						ctAfter = read32(ARM_SYSTIMER_CLO);
						unsigned ct = ctAfter - ctBefore;
						if (ct > 1)
						{
							// If this ever occurs then we have taken too long (ie >1us) and lost a cycle.
							// Cycle accuracy is now in jeopardy. If this occurs during critical communication loops then emulation can fail!
							//DEBUG_LOG("!");
						}
					}
					while (ctAfter == ctBefore);
				}
				ctBefore = ctAfter;
			}
		}
	}
	delete fileBrowser;
}

//static void MouseHandler(unsigned nButtons,
//	int nDisplacementX,		// -127..127
//	int nDisplacementY)		// -127..127
//{
//	DEBUG_LOG("Mouse: %x %d %d\r\n", nButtons, nDisplacementX, nDisplacementY);
//}

#ifdef HAS_MULTICORE
extern "C" 
{
	void run_core() 
	{
		enable_MMU_and_IDCaches();
		_enable_unaligned_access();

		DEBUG_LOG("emulator running on core %d\r\n", _get_core());
		emulator();
	}
}
static void start_core(int core, func_ptr func)
{
	write32(0x4000008C + 0x10 * core, (unsigned int)func);
	__asm ("SEV");	// and wake it up.
}
#endif

static bool AttemptToLoadROM(char* ROMName)
{
	FIL fp;
	FRESULT res;

	res = f_open(&fp, ROMName, FA_READ);
	if (res == FR_OK)
	{
		u32 bytesRead;
		SetACTLed(true);
		f_read(&fp, roms.ROMImages[0], ROMs::ROM_SIZE, &bytesRead);
		strncpy(roms.ROMNames[0], ROMName, 255);
		roms.ROMValid[0] = true;
		SetACTLed(false);
		f_close(&fp);
		DEBUG_LOG("Opened ROM %s %d %d %d\r\n", ROMName, ROMs::ROM_SIZE, bytesRead, bytesRead == ROMs::ROM_SIZE);
		return true;
	}
	else
	{
		roms.ROMValid[0] = false;
		DEBUG_LOG("COULD NOT OPEN ROM FILE;- %s!\r\n", ROMName);
		return false;
	}
}

static void DisplayLogo()
{
	int w;
	int h;
	int channels_in_file;
	stbi_uc* image = stbi_load_from_memory((stbi_uc const*)I__logo_png, I__logo_png_size, &w, &h, &channels_in_file, 0);

	screen.PlotImage((u32*)image, 0, 0, w, h);

	if (versionMinor < 10) snprintf(tempBuffer, tempBufferSize, "V%d.0%d", versionMajor, versionMinor);
	else snprintf(tempBuffer, tempBufferSize, "V%d.%d", versionMajor, versionMinor);
	screen.PrintText(false, 20, 180, tempBuffer, FileBrowser::Colour(VIC2_COLOUR_INDEX_BLUE));
}

static void LoadOptions()
{
	FIL fp;
	FRESULT res;

	res = f_open(&fp, "options.txt", FA_READ);
	if (res == FR_OK)
	{
		u32 bytesRead;
		SetACTLed(true);
		f_read(&fp, s_u8Memory, sizeof(s_u8Memory), &bytesRead);
		SetACTLed(false);
		f_close(&fp);

		options.Process((char*)s_u8Memory);

		screenWidth = options.ScreenWidth();
		screenHeight = options.ScreenHeight();
	}
}

static void CheckOptions()
{
	FIL fp;
	FRESULT res;

	u32 widthText, heightText;
	u32 widthScreen = screen.Width();
	u32 heightScreen = screen.Height();
	u32 xpos, ypos;
	const char* ROMName;

	deviceID = (u8)options.GetDeviceID();
	DEBUG_LOG("DeviceID = %d\r\n", deviceID);


	// print confirmation of parsed options
	if (0) {
		screen.Clear(COLOUR_BLACK);
		int y_pos = 200;
		snprintf(tempBuffer, tempBufferSize, "ignoreReset = %d\r\n", options.IgnoreReset());
		screen.PrintText(false, 0, y_pos+=16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
		snprintf(tempBuffer, tempBufferSize, "RAMBOard = %d\r\n", options.GetRAMBOard());
		screen.PrintText(false, 0, y_pos+=16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
		snprintf(tempBuffer, tempBufferSize, "splitIECLines = %d\r\n", options.SplitIECLines());
		screen.PrintText(false, 0, y_pos+=16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
		snprintf(tempBuffer, tempBufferSize, "invertIECInputs = %d\r\n", options.InvertIECInputs());
		screen.PrintText(false, 0, y_pos+=16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
		snprintf(tempBuffer, tempBufferSize, "invertIECOutputs = %d\r\n", options.InvertIECOutputs());
		screen.PrintText(false, 0, y_pos+=16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
		snprintf(tempBuffer, tempBufferSize, "i2cBusAddress = %d\r\n", options.I2CLcdAddress());
		screen.PrintText(false, 0, y_pos+=16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
		IEC_Bus::WaitMicroSeconds(5 * 1000000);
	}

	ROMName = options.GetRomFontName();
	if (ROMName)
	{
		//DEBUG_LOG("%d Rom Name = %s\r\n", ROMIndex, ROMName);
		res = f_open(&fp, ROMName, FA_READ);
		if (res == FR_OK)
		{
			u32 bytesRead;

			screen.Clear(COLOUR_BLACK);
			snprintf(tempBuffer, tempBufferSize, "Loading ROM %s\r\n", ROMName);
			screen.MeasureText(false, tempBuffer, &widthText, &heightText);
			xpos = (widthScreen - widthText) >> 1;
			ypos = (heightScreen - heightText) >> 1;
			screen.PrintText(false, xpos, ypos, tempBuffer, COLOUR_WHITE, COLOUR_RED);

			SetACTLed(true);
			res = f_read(&fp, CBMFontData, CBMFont_size, &bytesRead);
			SetACTLed(false);
			f_close(&fp);
			if (res == FR_OK && bytesRead == CBMFont_size)
			{
				CBMFont = CBMFontData;
			}
			//DEBUG_LOG("Read ROM %s from options\r\n", ROMName);
		}
	}

	int ROMIndex;

	for (ROMIndex = ROMs::MAX_ROMS - 1; ROMIndex >= 0; --ROMIndex)
	{
		roms.ROMValid[ROMIndex] = false;
		const char* ROMName = options.GetRomName(ROMIndex);
		if (ROMName[0])
		{
			//DEBUG_LOG("%d Rom Name = %s\r\n", ROMIndex, ROMName);
			res = f_open(&fp, ROMName, FA_READ);
			if (res == FR_OK)
			{
				u32 bytesRead;

				screen.Clear(COLOUR_BLACK);
				snprintf(tempBuffer, tempBufferSize, "Loading ROM %s\r\n", ROMName);
				screen.MeasureText(false, tempBuffer, &widthText, &heightText);
				xpos = (widthScreen - widthText) >> 1;
				ypos = (heightScreen - heightText) >> 1;
				screen.PrintText(false, xpos, ypos, tempBuffer, COLOUR_WHITE, COLOUR_RED);

				SetACTLed(true);
				res = f_read(&fp, roms.ROMImages[ROMIndex], ROMs::ROM_SIZE, &bytesRead);
				SetACTLed(false);
				if (res == FR_OK && bytesRead == ROMs::ROM_SIZE)
				{
					strncpy(roms.ROMNames[ROMIndex], ROMName, 255);
					roms.ROMValid[ROMIndex] = true;
				}
				f_close(&fp);
				//DEBUG_LOG("Read ROM %s from options\r\n", ROMName);
			}
		}
	}

	if (roms.ROMValid[0] == false && !(AttemptToLoadROM("d1541.rom") || AttemptToLoadROM("dos1541") || AttemptToLoadROM("d1541II") || AttemptToLoadROM("Jiffy.bin")))
	{
		snprintf(tempBuffer, tempBufferSize, "No ROM file found!\r\nPlease copy a valid 1541 ROM file in the root folder of the SD card.\r\nThe file needs to be called 'dos1541'.");
		screen.MeasureText(false, tempBuffer, &widthText, &heightText);
		xpos = (widthScreen - widthText) >> 1;
		ypos = (heightScreen - heightText) >> 1;
		do
		{
			screen.Clear(COLOUR_RED);
			IEC_Bus::WaitMicroSeconds(20000);
			screen.PrintText(false, xpos, ypos, tempBuffer, COLOUR_WHITE, COLOUR_RED);
			IEC_Bus::WaitMicroSeconds(100000);
		}
		while (1);
	}
}

extern "C"
{
	void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags)
	{
		FATFS fileSystem;

		disk_setEMM(&m_EMMC);
		m_EMMC.Initialize();
		f_mount(&fileSystem, "", 1);

		LoadOptions();

		InitialiseHardware();
		enable_MMU_and_IDCaches();
		_enable_unaligned_access();

		write32(ARM_GPIO_GPCLR0, 0xFFFFFFFF);

		DisplayLogo();
		int y_pos = 184;
		snprintf(tempBuffer, tempBufferSize, "Copyright(C) 2018 Stephen White");
		screen.PrintText(false, 0, y_pos+=16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
		snprintf(tempBuffer, tempBufferSize, "This program comes with ABSOLUTELY NO WARRANTY.");
		screen.PrintText(false, 0, y_pos+=16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
		snprintf(tempBuffer, tempBufferSize, "This is free software, and you are welcome to redistribute it.");
		screen.PrintText(false, 0, y_pos+=16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);


		if (!options.QuickBoot())
			IEC_Bus::WaitMicroSeconds(3 * 1000000);

		InterruptSystemInitialize();
		TimerSystemInitialize();

		USPiInitialize();

		if (!USPiKeyboardAvailable())
			DEBUG_LOG("Keyboard not found\r\n");
		else
			DEBUG_LOG("Keyboard found\r\n");

		//if (!USPiMouseAvailable())
		//	DEBUG_LOG("Mouse not found\r\n");
		//else
		//	DEBUG_LOG("Mouse found\r\n");

		Keyboard::Instance();
		InputMappings::Instance();
		//USPiMouseRegisterStatusHandler(MouseHandler);


		CheckOptions();


		int i2cBusMaster = options.I2CBusMaster();
		int i2cLcdAddress = options.I2CLcdAddress();
		if (strcasecmp(options.GetLCDName(), "ssd1306_128x64") == 0)
		{
			screenLCD = new ScreenLCD();
			screenLCD->Open(128, 64, 1, i2cBusMaster, i2cLcdAddress);
		}
		else
		{
		}

		IEC_Bus::SetSplitIECLines(options.SplitIECLines());
		IEC_Bus::SetInvertIECInputs(options.InvertIECInputs());
		IEC_Bus::SetInvertIECOutputs(options.InvertIECOutputs());
		IEC_Bus::SetIgnoreReset(options.IgnoreReset());

		if (!options.SoundOnGPIO())
		{
			dmaSound = (u32*)malloc(Sample_bin_size * 4);
			for (int i = 0; i < Sample_bin_size; ++i)
			{
				dmaSound[i] = Sample_bin[i];
			}
			dmaSoundCB.sourceAddress = dmaSound;
			//PlaySoundDMA();
		}

		f_chdir("/1541");

		m_IEC_Commands.SetDeviceId(deviceID);
		m_IEC_Commands.SetStarFileName(options.GetStarFileName());

		SetVIAsDeviceID(deviceID);

		pi1541.drive.SetVIA(&pi1541.VIA[1]);
		pi1541.VIA[0].GetPortB()->SetPortOut(0, IEC_Bus::PortB_OnPortOut);

		IEC_Bus::Initialise();

#ifdef HAS_MULTICORE
		start_core(3, _spin_core);
		start_core(2, _spin_core);
#ifdef USE_MULTICORE
		start_core(1, _init_core);
		UpdateScreen();		// core0 now loops here where it will handle interrupts and passively update the screen.
		while (1);
#else
		start_core(1, _spin_core);
#endif
#endif
#ifndef USE_MULTICORE
		emulator();	// If only one core the emulator runs on it now.
#endif
	}
}
