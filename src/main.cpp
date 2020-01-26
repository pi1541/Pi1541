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
#include "rpi-mailbox.h"
}
#include "InputMappings.h"
#include "options.h"
#include "iec_commands.h"
#include "diskio.h"
#include "Pi1541.h"
#include "Pi1581.h"
#include "FileBrowser.h"
#include "ScreenLCD.h"
#include "SpinLock.h"

#include "logo.h"
#include "sample.h"
#include "ssd_logo.h"

unsigned versionMajor = 1;
unsigned versionMinor = 23;

// When the emulated CPU starts we execute the first million odd cycles in non-real-time (ie as fast as possible so the emulated 1541 becomes responsive to CBM-Browser asap)
// During these cycles the CPU is executing the ROM self test routines (these do not need to be cycle accurate)
// ***1581*** Skip to AFCA (how many cycles is this?)
#define FAST_BOOT_CYCLES 1003061

#define COLOUR_BLACK RGBA(0, 0, 0, 0xff)
#define COLOUR_WHITE RGBA(0xff, 0xff, 0xff, 0xff)
#define COLOUR_RED RGBA(0xff, 0, 0, 0xff)
#define COLOUR_GREEN RGBA(0, 0xff, 0, 0xff)
#define COLOUR_CYAN RGBA(0, 0xff, 0xff, 0xff)
#define COLOUR_MAGENTA RGBA(0xff, 0, 0xff, 0xff)
#define COLOUR_YELLOW RGBA(0xff, 0xff, 0x00, 0xff)

// To exit a mounted disk image we need to watch(snoop) what the emulated CPU is doing when it executes code at some critical ROM addresses.
#define SNOOP_CD_CBM 0xEA2D
#define SNOOP_CD_CBM1581 0xAEB7
#define SNOOP_CD_JIFFY_BOTH 0xFC07
#define SNOOP_CD_JIFFY_DRIVEONLY 0xEA16
static const u8 snoopBackCommand[] = {
	'C', 'D', ':', '_'	//0x43, 0x44, 0x3a, 0x5f
};
static int snoopIndex = 0;
static int snoopPC = 0;

enum EmulatingMode
{
	IEC_COMMANDS,
	EMULATING_1541,
	EMULATING_1581
};

volatile EmulatingMode emulating;

typedef void(*func_ptr)();

const long int tempBufferSize = 1024;
char tempBuffer[tempBufferSize];
ROMs roms;

const long int CBMFont_size = 4096;
unsigned char CBMFontData[4096];
unsigned char* CBMFont = 0;

#define LCD_LOGO_MAX_SIZE 1024
u8 LcdLogoFile[LCD_LOGO_MAX_SIZE];

u8 s_u8Memory[0xc000];

int numberOfUSBMassStorageDevices = 0;
DiskCaddy diskCaddy;
Pi1541 pi1541;
#if defined(PI1581SUPPORT)
Pi1581 pi1581;
#endif
CEMMCDevice	m_EMMC;
Screen screen;
ScreenLCD* screenLCD = 0;
Options options;
const char* fileBrowserSelectedName;
u8 deviceID = 8;
IEC_Commands m_IEC_Commands;
InputMappings* inputMappings;
#if not defined(EXPERIMENTALZERO)
Keyboard* keyboard;
#endif
bool USBKeyboardDetected = false;
//bool resetWhileEmulating = false;
bool selectedViaIECCommands = false;
u16 pc;
#if defined(RPI2)
u32 clockCycles1MHz;
#endif

#if not defined(EXPERIMENTALZERO)
SpinLock core0RefreshingScreen;
#endif
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

	int GetTemperature(unsigned& value)
	{
		rpi_mailbox_property_t* mp;

		RPI_PropertyInit();
		RPI_PropertyAddTag(TAG_GET_TEMPERATURE);
		RPI_PropertyProcess();

		value = 0;
		if ((mp = RPI_PropertyGet(TAG_GET_TEMPERATURE)))
		{
			value = mp->data.buffer_32[1];
			return 1;
		}

		return 0;
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

extern u8 read6502(u16 address);
extern u8 read6502ExtraRAM(u16 address);
extern void write6502(u16 address, const u8 value);
extern void write6502ExtraRAM(u16 address, const u8 value);
extern u8 read6502_1581(u16 address);
extern void write6502_1581(u16 address, const u8 value);

void InitialiseHardware()
{
#if defined(RPI3)
	RPI_GpioVirtInit();
	RPI_TouchInit();
#endif

#if not defined(EXPERIMENTALZERO)
	screen.Open(screenWidth, screenHeight, 16);
#endif
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

#if defined(RPI2)
	// Enable clock cycle counter
	asm volatile ("mcr p15,0,%0,c9,c12,0" :: "r" (0b0001));
	asm volatile ("mcr p15,0,%0,c9,c12,1" :: "r" ((1 << 31)));

	clockCycles1MHz = MaxClk / 1000000;
#endif
}

void InitialiseLCD()
{
	FILINFO filLcdIcon;

	int i2cBusMaster = options.I2CBusMaster();
	int i2cLcdAddress = options.I2CLcdAddress();
	int i2cLcdFlip = options.I2CLcdFlip();
	int i2cLcdOnContrast = options.I2CLcdOnContrast();
	int i2cLcdDimContrast = options.I2CLcdDimContrast();
	int i2cLcdDimTime = options.I2CLcdDimTime();
	int i2cLcdUseCBMChar = options.I2cLcdUseCBMChar();
	LCD_MODEL i2cLcdModel = options.I2CLcdModel();

	if (i2cLcdModel)
	{
		int width = 128;
		int height = 64;
		if (i2cLcdModel == LCD_1306_128x32)
			height = 32;
		screenLCD = new ScreenLCD();
		screenLCD->Open(width, height, 1, i2cBusMaster, i2cLcdAddress, i2cLcdFlip, i2cLcdModel, i2cLcdUseCBMChar);
		screenLCD->SetContrast(i2cLcdOnContrast);
		screenLCD->ClearInit(0); // sh1106 needs this

		bool logo_done = false;
		if ( (height == 64) && (strcasecmp(options.GetLcdLogoName(), "1541ii") == 0) )
		{
			screenLCD->PlotRawImage(logo_ssd_1541ii, 0, 0, width, height);
			snprintf(tempBuffer, tempBufferSize, "Pi1541 V%d.%02d", versionMajor, versionMinor);
			screenLCD->PrintText(false, 16, 0, tempBuffer, 0xffffffff);
			logo_done = true;
		}
		else if (( height == 64) && (strcasecmp(options.GetLcdLogoName(), "1541classic") == 0) )
		{
			screenLCD->PlotRawImage(logo_ssd_1541classic, 0, 0, width, height);
			logo_done = true;
		}
		else if (f_stat(options.GetLcdLogoName(), &filLcdIcon) == FR_OK && filLcdIcon.fsize <= LCD_LOGO_MAX_SIZE)
		{
			FIL fp;
			FRESULT res;

			res = f_open(&fp, filLcdIcon.fname, FA_READ);
			if (res == FR_OK)
			{
				u32 bytesRead;
				f_read(&fp, LcdLogoFile, LCD_LOGO_MAX_SIZE, &bytesRead);
				f_close(&fp);
				screenLCD->PlotRawImage(LcdLogoFile, 0, 0, width, height);
				logo_done = true;
			}
		}

		if (!logo_done)
		{
			snprintf(tempBuffer, tempBufferSize, "Pi1541 V%d.%02d", versionMajor, versionMinor);
			int x = (width - 8*strlen(tempBuffer) ) /2;
			int y = (height-16)/2;
			screenLCD->PrintText(false, x, y, tempBuffer, 0x0);
		}
		screenLCD->RefreshScreen();
	}
	else
	{
		screenLCD = 0;
	}
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
#if not defined(EXPERIMENTALZERO)
	bool oldLED = false;
	bool oldMotor = false;
	bool oldATN = false;
	bool oldDATA = false;
	bool oldCLOCK = false;
	bool oldSRQ = false;

	u32 oldTrack = 0;
	u32 textColour = COLOUR_BLACK;
	u32 bgColour = COLOUR_WHITE;
	u32 oldTemp = 0;

	RGBA atnColour = COLOUR_YELLOW;
	RGBA dataColour = COLOUR_GREEN;
	RGBA clockColour = COLOUR_CYAN;
	RGBA SRQColour = COLOUR_MAGENTA;
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

		bool led = false;
		bool motor = false;

		if (emulating == EMULATING_1541)
		{
			led = pi1541.drive.IsLEDOn();
			motor = pi1541.drive.IsMotorOn();
		}
		else if (emulating == EMULATING_1581)
		{
			led = pi1581.IsLEDOn();
			motor = pi1581.IsMotorOn();
		}

		value = led;
		if (value != oldLED)
		{
//			SetACTLed(value);
			oldLED = value;
			snprintf(tempBuffer, tempBufferSize, "%d", value);
			screen.PrintText(false, 4 * 8, y, tempBuffer, value ? COLOUR_RED : textColour, bgColour);
			//refreshUartStatusDisplay = true;
		}

		value = motor;
		if (value != oldMotor)
		{
			oldMotor = value;
			snprintf(tempBuffer, tempBufferSize, "%d", value);
			screen.PrintText(false, 12 * 8, y, tempBuffer, textColour, bgColour);
			//refreshUartStatusDisplay = true;
		}

		if (options.GraphIEC())
			screen.DrawLineV(graphX, top3, bottom, BkColour);

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

		//value = IEC_Bus::GetPI_SRQ();
		//if (options.GraphIEC())
		//{
		//	if (value ^ oldSRQ)
		//	{
		//		screen.DrawLineV(graphX, 0, 100, SRQColour);
		//	}
		//	else
		//	{
		//		if (value) screen.PlotPixel(graphX, 0, SRQColour);
		//		else screen.PlotPixel(graphX, 100, SRQColour);
		//	}
		//}
		//if (value != oldSRQ)
		//{
		//	oldSRQ = value;
		////	snprintf(tempBuffer, tempBufferSize, "%d", value);
		////	screen.PrintText(false, 41 * 8, y, tempBuffer, textColour, bgColour);
		////	//refreshUartStatusDisplay = true;
		//}

		if (graphX++ > screenWidthM1) graphX = 0;
// black vertical line ahead of graph
		if (options.GraphIEC())
			screen.DrawLineV(graphX, top3, bottom, COLOUR_BLACK);

		if (options.DisplayTemperature())
		{
			unsigned temp;
			if (GetTemperature(temp))
			{
				temp /= 1000;
				if (temp != oldTemp)
				{
					oldTemp = temp;
					//DEBUG_LOG("%0x %d %d\r\n", temp, temp, temp / 1000);
					snprintf(tempBuffer, tempBufferSize, "%02d", temp);
					screen.PrintText(false, 43 * 8, y, tempBuffer, textColour, bgColour);
				}
			}
		}

		u32 track;
		if (emulating == EMULATING_1541)
		{
			track = pi1541.drive.Track();
			if (track != oldTrack)
			{
				oldTrack = track;
				snprintf(tempBuffer, tempBufferSize, "%02d.%d", (oldTrack >> 1) + 1, oldTrack & 1 ? 5 : 0);
				screen.PrintText(false, 20 * 8, y, tempBuffer, textColour, bgColour);
				//refreshUartStatusDisplay = true;

				if (screenLCD)
				{
#if not defined(EXPERIMENTALZERO)
					core0RefreshingScreen.Acquire();
#endif

					IEC_Bus::WaitMicroSeconds(100);

					snprintf(tempBuffer, tempBufferSize, "D%02d %02d.%d", deviceID, (oldTrack >> 1) + 1, oldTrack & 1 ? 5 : 0);
					screenLCD->PrintText(false, 0, 0, tempBuffer, 0, RGBA(0xff, 0xff, 0xff, 0xff));
					//				screenLCD->SetContrast(255.0/79.0*track);
					screenLCD->RefreshRows(0, 1);

					IEC_Bus::WaitMicroSeconds(100);
#if not defined(EXPERIMENTALZERO)
					core0RefreshingScreen.Release();
#endif
				}

			}
		}
		else if (emulating == EMULATING_1581)
		{
			track = pi1581.wd177x.GetCurrentTrack();
			if (track != oldTrack)
			{
				oldTrack = track;
				snprintf(tempBuffer, tempBufferSize, "%02d", (oldTrack) + 1);
				screen.PrintText(false, 20 * 8, y, tempBuffer, textColour, bgColour);
				//refreshUartStatusDisplay = true;

				if (screenLCD)
				{
#if not defined(EXPERIMENTALZERO)
					core0RefreshingScreen.Acquire();
#endif
					IEC_Bus::WaitMicroSeconds(100);
					screenLCD->PrintText(false, 0, 0, tempBuffer, 0, RGBA(0xff, 0xff, 0xff, 0xff));
					//				screenLCD->SetContrast(255.0/79.0*track);
					screenLCD->RefreshRows(0, 1);
					IEC_Bus::WaitMicroSeconds(100);
#if not defined(EXPERIMENTALZERO)
					core0RefreshingScreen.Release();
#endif
				}

			}
		}
		if (emulating != IEC_COMMANDS)
		{
			// Putting the semaphore around diskCaddy.Update() keeps this core awake and this breaks emulation on option B hardware.
			// Don't know why. Disabling for now.
//#if not defined(EXPERIMENTALZERO)
//			core0RefreshingScreen.Acquire();
//#endif
			diskCaddy.Update();
//#if not defined(EXPERIMENTALZERO)
//			core0RefreshingScreen.Release();
//#endif
		}

		//if (options.GetSupportUARTInput())
		//	UpdateUartControls(refreshUartStatusDisplay, oldLED, oldMotor, oldATN, oldDATA, oldCLOCK, oldTrack, romIndex);

		// Go back to sleep. The USB irq will wake us up again.
		__asm ("WFE");
	}
#endif
}

static bool Snoop(u8 a)
{
	if (a == snoopBackCommand[snoopIndex] || (snoopIndex == 2 && (a == snoopBackCommand[3])))
	{
		if ((snoopIndex + 1) == sizeof(snoopBackCommand) || (snoopIndex == 2 && (a == snoopBackCommand[3])))
		{
			// Exit full emulation back to IEC commands level simulation.
			snoopIndex = 0;
			return true;
		}
		else
		{
			snoopIndex++;
		}
	}
	else
	{
		snoopIndex = 0;
		snoopPC = 0;
	}
	return false;
}

//--------------------------------------------------------------------------------------
// This is an implementation of FNV-1a
// (http://www.isthe.com/chongo/tech/comp/fnv/)
//--------------------------------------------------------------------------------------
u32 HashBuffer(const void* pBuffer, u32 length)
{
	u8*	pu8Buffer = (u8*)pBuffer;
	u32	hash = 0x811c9dc5U;

	while (length)
	{
		hash ^= *pu8Buffer++;
		hash *= 16777619U;
		--length;
	}
	return hash;
}

EmulatingMode BeginEmulating(FileBrowser* fileBrowser, const char* filenameForIcon)
{
	DiskImage* diskImage = diskCaddy.SelectFirstImage();
	if (diskImage)
	{
#if defined(PI1581SUPPORT)
		if (diskImage->IsD81())
		{
			pi1581.Insert(diskImage);
			fileBrowser->DisplayDiskInfo(diskImage, filenameForIcon);
			fileBrowser->ShowDeviceAndROM( roms.ROMName1581 );
			return EMULATING_1581;
		}
		else
#endif
		{
			pi1541.drive.Insert(diskImage);
			fileBrowser->DisplayDiskInfo(diskImage, filenameForIcon);
			fileBrowser->ShowDeviceAndROM();
			return EMULATING_1541;
		}
	}
	inputMappings->WaitForClearButtons();
	return IEC_COMMANDS;
}
#if not defined(EXPERIMENTALZERO)
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
#endif

#if not defined(EXPERIMENTALZERO)
static void PlaySoundDMA()
{
	write32(PWM_DMAC, PWM_ENAB + 0x0001);
	write32(DMA_ENABLE, 1);	// DMA_EN0
	write32(DMA0_BASE + DMA_CONBLK_AD, (u32)&dmaSoundCB);
	write32(DMA0_BASE + DMA_CS, DMA_ACTIVE);
}
#endif

void GlobalSetDeviceID(u8 id)
{
	deviceID = id;
	m_IEC_Commands.SetDeviceId(id);
	pi1541.SetDeviceID(id);
#if defined(PI1581SUPPORT)
	pi1581.SetDeviceID(id);
#endif
}

void CheckAutoMountImage(EXIT_TYPE reset_reason , FileBrowser* fileBrowser)
{
	const char* autoMountImageName = options.GetAutoMountImageName();
	if (autoMountImageName[0] != 0)
	{
		switch (reset_reason)
		{
			case EXIT_UNKNOWN:
			case EXIT_AUTOLOAD:
			case EXIT_RESET:
				fileBrowser->SelectAutoMountImage(autoMountImageName);
			break;
			case EXIT_CD:
			case EXIT_KEYBOARD:
			break;
			default:
			break;
		}
	}
}

EXIT_TYPE Emulate1541(FileBrowser* fileBrowser)
{
	EXIT_TYPE exitReason = EXIT_UNKNOWN;
	bool oldLED = false;
	unsigned ctBefore = 0;
	unsigned ctAfter = 0;
	int cycleCount = 0;
	unsigned caddyIndex;
	int headSoundCounter = 0;
	int headSoundFreqCounter = 0;
	//			const int headSoundFreq = 833;	// 1200Hz = 1/1200 * 10^6;
	const int headSoundFreq = 1000000 / options.SoundOnGPIOFreq();	// 1200Hz = 1/1200 * 10^6;
	unsigned char oldHeadDir = 0;
	int resetCount = 0;
	bool refreshOutsAfterCPUStep = true;
	unsigned numberOfImages = diskCaddy.GetNumberOfImages();
	unsigned numberOfImagesMax = numberOfImages;
	if (numberOfImagesMax > 10)
		numberOfImagesMax = 10;

#if not defined(EXPERIMENTALZERO)
	core0RefreshingScreen.Acquire();
#endif
	diskCaddy.Display();
#if not defined(EXPERIMENTALZERO)
	core0RefreshingScreen.Release();
#endif

	inputMappings->directDiskSwapRequest = 0;
	// Force an update on all the buttons now before we start emulation mode. 
	IEC_Bus::ReadBrowseMode();

	bool extraRAM = options.GetExtraRAM();
	DataBusReadFn dataBusRead = extraRAM ? read6502ExtraRAM : read6502;
	DataBusWriteFn dataBusWrite = extraRAM ? write6502ExtraRAM : write6502;
	pi1541.m6502.SetBusFunctions(dataBusRead, dataBusWrite);

	IEC_Bus::VIA = &pi1541.VIA[0];
	IEC_Bus::port = pi1541.VIA[0].GetPortB();
	pi1541.Reset();	// will call IEC_Bus::Reset();

	IEC_Bus::LetSRQBePulledHigh();

	//resetWhileEmulating = false;
	selectedViaIECCommands = false;

	u32 hash = pi1541.drive.GetDiskImage()->GetHash();
	// 0x42c02586 = maniac_mansion_s1[lucasfilm_1989](ntsc).g64
	// 0x18651422 = aliens[electric_dreams_1987].g64
	// 0x2a7f4b77 = zak_mckracken_boot[activision_1988](manual)(!).g64
	// 0x97732c3e = maniac_mansion_s1[activision_1987](!).g64
	// 0x63f809d2 = 4x4_offroad_racing_s1[epyx_1988](ntsc)(!).g64
	if (hash == 0x42c02586 || hash == 0x18651422 || hash == 0x2a7f4b77 || hash == 0x97732c3e || hash == 0x63f809d2)
	{
		refreshOutsAfterCPUStep = false;
	}

	// Quickly get through 1541's self test code.
	// This will make the emulated 1541 responsive to commands asap.
	// During this time we don't need to set outputs.

	while (cycleCount < FAST_BOOT_CYCLES)
	{
		IEC_Bus::ReadEmulationMode1541();

		pi1541.m6502.SYNC();

		pi1541.m6502.Step();

		pi1541.Update();

		cycleCount++;
	}

	// Self test code done. Begin realtime emulation.

#if defined(RPI2)
	asm volatile ("mrc p15,0,%0,c9,c13,0" : "=r" (ctBefore));
#else
	ctBefore = read32(ARM_SYSTIMER_CLO);
#endif

	while (exitReason == EXIT_UNKNOWN)
	{
		if (refreshOutsAfterCPUStep)
			IEC_Bus::ReadEmulationMode1541();

		if (pi1541.m6502.SYNC())	// About to start a new instruction.
		{
			pc = pi1541.m6502.GetPC();
			// See if the emulated cpu is executing CD:_ (ie back out of emulated image)
			if (snoopIndex == 0 && (pc == SNOOP_CD_CBM || pc == SNOOP_CD_JIFFY_BOTH || pc == SNOOP_CD_JIFFY_DRIVEONLY)) snoopPC = pc;

			if (pc == snoopPC)
			{
				if (Snoop(pi1541.m6502.GetA()))
				{
					emulating = IEC_COMMANDS;
					exitReason = EXIT_CD;
				}
			}
		}

		pi1541.m6502.Step();	// If the CPU reads or writes to the VIA then clk and data can change

		//To artificialy delay the outputs later into the phi2's cycle (do this on future Pis that will be faster and perhaps too fast)
		//read32(ARM_SYSTIMER_CLO);	//Each one of these is > 100ns
		//read32(ARM_SYSTIMER_CLO);
		//read32(ARM_SYSTIMER_CLO);

//		IEC_Bus::ReadEmulationMode1541();
		if (refreshOutsAfterCPUStep)
			IEC_Bus::RefreshOuts1541();	// Now output all outputs.

		IEC_Bus::OutputLED = pi1541.drive.IsLEDOn();
#if defined(RPI3)
		if (IEC_Bus::OutputLED ^ oldLED)
		{
			SetACTLed(IEC_Bus::OutputLED);
			oldLED = IEC_Bus::OutputLED;
		}
#endif

#if not defined(EXPERIMENTALZERO)
		// Do head moving sound
		unsigned char headDir = pi1541.drive.GetLastHeadDirection();
		if (headDir != oldHeadDir)	// Need to start a new sound?
		{
			oldHeadDir = headDir;
			if (options.SoundOnGPIO())
			{
				headSoundCounter = 1000 * options.SoundOnGPIODuration();
				headSoundFreqCounter = headSoundFreq;
			}
			else
			{
				PlaySoundDMA();
			}
		}


#endif

		IEC_Bus::ReadGPIOUserInput(3);

		// Other core will check the uart (as it is slow) (could enable uart irqs - will they execute on this core?)
#if not defined(EXPERIMENTALZERO)
		inputMappings->CheckKeyboardEmulationMode(numberOfImages, numberOfImagesMax);
#endif
		inputMappings->CheckButtonsEmulationMode();

		bool exitEmulation = inputMappings->Exit();
		bool exitDoAutoLoad = inputMappings->AutoLoad();

		// We have now output so HERE is where the next phi2 cycle starts.
		pi1541.Update();


		bool reset = IEC_Bus::IsReset();
		if (reset)
			resetCount++;
		else
			resetCount = 0;

		if ((emulating == IEC_COMMANDS) || (resetCount > 10) || exitEmulation || exitDoAutoLoad)
		{
			if (reset)
				exitReason = EXIT_RESET;
			if (exitEmulation)
				exitReason = EXIT_KEYBOARD;
			if (exitDoAutoLoad)
				exitReason = EXIT_AUTOLOAD;
		}

#if defined(RPI2)
		do  // Sync to the 1MHz clock
		{
			asm volatile ("mrc p15,0,%0,c9,c13,0" : "=r" (ctAfter));
		} while ((ctAfter - ctBefore) < clockCycles1MHz);
#else
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
		} while (ctAfter == ctBefore);
#endif
		ctBefore = ctAfter;
		
		if (!refreshOutsAfterCPUStep)
		{
			IEC_Bus::ReadEmulationMode1541();
			IEC_Bus::RefreshOuts1541();	// Now output all outputs.
		}
#if not defined(EXPERIMENTALZERO)
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
#endif

		if (numberOfImages > 1)
		{
			bool nextDisk = inputMappings->NextDisk();
			bool prevDisk = inputMappings->PrevDisk();
			if (nextDisk)
			{
				pi1541.drive.Insert(diskCaddy.PrevDisk());
#if defined(EXPERIMENTALZERO)
				diskCaddy.Update();
#endif
			}
			else if (prevDisk)
			{
				pi1541.drive.Insert(diskCaddy.NextDisk());
#if defined(EXPERIMENTALZERO)
				diskCaddy.Update();
#endif
			}
#if not defined(EXPERIMENTALZERO)
			else if (inputMappings->directDiskSwapRequest != 0)
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
#endif
		}
	}
	return exitReason;
}

#if defined(PI1581SUPPORT)
EXIT_TYPE Emulate1581(FileBrowser* fileBrowser)
{
	EXIT_TYPE exitReason = EXIT_UNKNOWN;
	bool oldLED = false;
	unsigned ctBefore = 0;
	unsigned ctAfter = 0;
	int cycleCount = 0;
	unsigned caddyIndex;
	int headSoundCounter = 0;
	int headSoundFreqCounter = 0;
	//			const int headSoundFreq = 833;	// 1200Hz = 1/1200 * 10^6;
	const int headSoundFreq = 1000000 / options.SoundOnGPIOFreq();	// 1200Hz = 1/1200 * 10^6;
	unsigned int oldTrack = 0;
	int resetCount = 0;

	unsigned numberOfImages = diskCaddy.GetNumberOfImages();
	unsigned numberOfImagesMax = numberOfImages;
	if (numberOfImagesMax > 10)
		numberOfImagesMax = 10;

#if not defined(EXPERIMENTALZERO)
	core0RefreshingScreen.Acquire();
#endif
	diskCaddy.Display();
#if not defined(EXPERIMENTALZERO)
	core0RefreshingScreen.Release();
#endif

	inputMappings->directDiskSwapRequest = 0;
	// Force an update on all the buttons now before we start emulation mode. 
	IEC_Bus::ReadBrowseMode();

	DataBusReadFn dataBusRead = read6502_1581;
	DataBusWriteFn dataBusWrite = write6502_1581;
	pi1581.m6502.SetBusFunctions(dataBusRead, dataBusWrite);

	IEC_Bus::CIA = &pi1581.CIA;
	IEC_Bus::port = pi1581.CIA.GetPortB();
	pi1581.Reset();	// will call IEC_Bus::Reset();

#if defined(RPI2)
	asm volatile ("mrc p15,0,%0,c9,c13,0" : "=r" (ctBefore));
#else
	ctBefore = read32(ARM_SYSTIMER_CLO);
#endif

	//resetWhileEmulating = false;
	selectedViaIECCommands = false;

	oldTrack = pi1581.wd177x.GetCurrentTrack();

	while (exitReason == EXIT_UNKNOWN)
	{
		IEC_Bus::ReadEmulationMode1581();

		for (int cycle2MHz = 0; cycle2MHz < 2; ++cycle2MHz)
		{
			if (pi1581.m6502.SYNC())	// About to start a new instruction.
			{
				pc = pi1581.m6502.GetPC();
				// See if the emulated cpu is executing CD:_ (ie back out of emulated image)
				if (snoopIndex == 0 && (pc == SNOOP_CD_CBM1581)) snoopPC = pc;

				if (pc == snoopPC)
				{
					if (Snoop(pi1581.m6502.GetA()))
					{
						emulating = IEC_COMMANDS;
						exitReason = EXIT_CD;
					}
				}
			}
			pi1581.m6502.Step();
			pi1581.Update();
		}

		IEC_Bus::RefreshOuts1581();	// Now output all outputs.

		IEC_Bus::OutputLED = pi1581.IsLEDOn();
#if defined(RPI3)
		if (IEC_Bus::OutputLED ^ oldLED)
		{
			SetACTLed(IEC_Bus::OutputLED);
			oldLED = IEC_Bus::OutputLED;
		}
#endif

#if not defined(EXPERIMENTALZERO)
		// Do head moving sound
		unsigned int track = pi1581.wd177x.GetCurrentTrack();
		if (track != oldTrack)	// Need to start a new sound?
		{
			oldTrack = track;
			if (options.SoundOnGPIO())
			{
				headSoundCounter = 1000 * options.SoundOnGPIODuration();
				headSoundFreqCounter = headSoundFreq;
			}
			else
			{
				PlaySoundDMA();
			}
		}
#endif

		IEC_Bus::ReadGPIOUserInput(3);

		// Other core will check the uart (as it is slow) (could enable uart irqs - will they execute on this core?)
#if not defined(EXPERIMENTALZERO)
		inputMappings->CheckKeyboardEmulationMode(numberOfImages, numberOfImagesMax);
#endif
		inputMappings->CheckButtonsEmulationMode();

		bool exitEmulation = inputMappings->Exit();
		bool exitDoAutoLoad = inputMappings->AutoLoad();


		bool reset = IEC_Bus::IsReset();
		if (reset)
			resetCount++;
		else
			resetCount = 0;

		if ((emulating == IEC_COMMANDS)|| (resetCount > 10) || exitEmulation || exitDoAutoLoad)
		{
			if (reset)
				exitReason = EXIT_RESET;
			if (exitEmulation)
				exitReason = EXIT_KEYBOARD;
			if (exitDoAutoLoad)
				exitReason = EXIT_AUTOLOAD;
		}

#if defined(RPI2)
		do  // Sync to the 1MHz clock
		{
			asm volatile ("mrc p15,0,%0,c9,c13,0" : "=r" (ctAfter));
		} while ((ctAfter - ctBefore) < clockCycles1MHz);
#else
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
		} while (ctAfter == ctBefore);
#endif
		ctBefore = ctAfter;

#if not defined(EXPERIMENTALZERO)
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
#endif

		if (numberOfImages > 1)
		{
			bool nextDisk = inputMappings->NextDisk();
			bool prevDisk = inputMappings->PrevDisk();
			if (nextDisk)
			{
				pi1581.Insert(diskCaddy.PrevDisk());
#if defined(EXPERIMENTALZERO)
				diskCaddy.Update();
#endif
			}
			else if (prevDisk)
			{
				pi1581.Insert(diskCaddy.NextDisk());
#if defined(EXPERIMENTALZERO)
				diskCaddy.Update();
#endif
			}
#if not defined(EXPERIMENTALZERO)
			else if (inputMappings->directDiskSwapRequest != 0)
			{
				for (caddyIndex = 0; caddyIndex < numberOfImagesMax; ++caddyIndex)
				{
					if (inputMappings->directDiskSwapRequest & (1 << caddyIndex))
					{
						DiskImage* diskImage = diskCaddy.SelectImage(caddyIndex);
						if (diskImage && diskImage != pi1581.GetDiskImage())
						{
							pi1581.Insert(diskImage);
							break;
						}
					}
				}
				inputMappings->directDiskSwapRequest = 0;
			}
#endif
		}

	}
	return exitReason;
}
#endif

void emulator()
{
#if not defined(EXPERIMENTALZERO)
	Keyboard* keyboard = Keyboard::Instance();
#endif
	FileBrowser* fileBrowser;
	EXIT_TYPE exitReason = EXIT_UNKNOWN;

	roms.lastManualSelectedROMIndex = 0;

	diskCaddy.SetScreen(&screen, screenLCD, &roms);
	fileBrowser = new FileBrowser(inputMappings, &diskCaddy, &roms, &deviceID, options.DisplayPNGIcons(), &screen, screenLCD, options.ScrollHighlightRate());
	pi1541.Initialise();

	m_IEC_Commands.SetAutoBootFB128(options.AutoBootFB128());
	m_IEC_Commands.Set128BootSectorName(options.Get128BootSectorName());
	m_IEC_Commands.SetLowercaseBrowseModeFilenames(options.LowercaseBrowseModeFilenames());
	m_IEC_Commands.SetNewDiskType(options.GetNewDiskType());

	emulating = IEC_COMMANDS;
	while (1)
	{
		if (emulating == IEC_COMMANDS)
		{
			IEC_Bus::VIA = 0;
			IEC_Bus::CIA = 0;
			IEC_Bus::port = 0;

			IEC_Bus::Reset();

			IEC_Bus::LetSRQBePulledHigh();
#if not defined(EXPERIMENTALZERO)
			core0RefreshingScreen.Acquire();
#endif
			IEC_Bus::WaitMicroSeconds(100);

			roms.ResetCurrentROMIndex();
			fileBrowser->ClearScreen();

			fileBrowserSelectedName = 0;
			fileBrowser->ClearSelections();

			fileBrowser->RefeshDisplay(); // Just redisplay the current folder.
#if not defined(EXPERIMENTALZERO)
			core0RefreshingScreen.Release();
#endif
			selectedViaIECCommands = false;

			inputMappings->Reset();
#if not defined(EXPERIMENTALZERO)
			inputMappings->SetKeyboardBrowseLCDScreen(screenLCD && options.KeyboardBrowseLCDScreen());
#endif
			fileBrowser->ShowDeviceAndROM();

			if (!options.GetDisableSD2IECCommands())
			{
				m_IEC_Commands.SimulateIECBegin();

				CheckAutoMountImage(exitReason, fileBrowser);

				while (emulating == IEC_COMMANDS)
				{
					IEC_Commands::UpdateAction updateAction = m_IEC_Commands.SimulateIECUpdate();

					switch (updateAction)
					{
						case IEC_Commands::RESET:
							if (options.GetOnResetChangeToStartingFolder())
								fileBrowser->DisplayRoot();
							IEC_Bus::Reset();
							m_IEC_Commands.SimulateIECBegin();
							CheckAutoMountImage(EXIT_UNKNOWN, fileBrowser);
							break;
						case IEC_Commands::NONE:
							fileBrowser->Update();
							// Check selections made via FileBrowser
							if (fileBrowser->SelectionsMade())
								emulating = BeginEmulating(fileBrowser, fileBrowser->LastSelectionName());
							break;
						case IEC_Commands::IMAGE_SELECTED:
							// Check selections made via IEC commands (like fb64)

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

								if (diskCaddy.Insert(filInfoSelected, readOnly))
									emulating = BeginEmulating(fileBrowser, filInfoSelected->fname);
								else
									fileBrowserSelectedName = 0;
							}
							else
							{
								fileBrowserSelectedName = 0;
							}

							if (fileBrowserSelectedName == 0)
								m_IEC_Commands.Reset();

							selectedViaIECCommands = true;
							break;
						case IEC_Commands::DIR_PUSHED:
							fileBrowser->FolderChanged();
							break;
						case IEC_Commands::POP_DIR:
							fileBrowser->PopFolder();
							break;
						case IEC_Commands::POP_TO_ROOT:
							fileBrowser->DisplayRoot();
							break;
						case IEC_Commands::REFRESH:
							fileBrowser->FolderChanged();
							break;
						case IEC_Commands::DEVICEID_CHANGED:
							GlobalSetDeviceID( m_IEC_Commands.GetDeviceId() );
							fileBrowser->ShowDeviceAndROM();
							break;
						case IEC_Commands::DEVICE_SWITCHED:
							DEBUG_LOG("DECIVE_SWITCHED\r\n");
							fileBrowser->DeviceSwitched();
							break;
						default:
							break;
					}
					usDelay(1);
				}
			}
			else
			{
				while (emulating == IEC_COMMANDS)
				{
					fileBrowser->Update();
					if (fileBrowser->SelectionsMade())
						emulating = BeginEmulating(fileBrowser, fileBrowser->LastSelectionName());
					usDelay(1);
				}
			}
		}
		else
		{
			if (emulating == EMULATING_1541)
				exitReason = Emulate1541(fileBrowser);
#if defined(PI1581SUPPORT)
			else
				exitReason = Emulate1581(fileBrowser);
#endif

			DEBUG_LOG("Exited emulation\r\n");

			// Clearing the caddy now
			//	- will write back all changed/dirty/written to disk images now
#if not defined(EXPERIMENTALZERO)
			core0RefreshingScreen.Acquire();
#endif
			if (diskCaddy.Empty())
				IEC_Bus::WaitMicroSeconds(2 * 1000000);

			IEC_Bus::WaitUntilReset();
			emulating = IEC_COMMANDS;
	
			if ((exitReason == EXIT_RESET) && (options.GetOnResetChangeToStartingFolder() || selectedViaIECCommands))
				fileBrowser->DisplayRoot(); // TO CHECK

			inputMappings->WaitForClearButtons();

#if not defined(EXPERIMENTALZERO)
			core0RefreshingScreen.Release();
#endif
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

	char ROMName2[256] = "/roms/";

	if (ROMName[0] != '/')	// not a full path, prepend /roms/
		strncat (ROMName2, ROMName, 240);
	else
		ROMName2[0] = 0;

	if ( (FR_OK == f_open(&fp, ROMName, FA_READ))
		|| (FR_OK == f_open(&fp, ROMName2, FA_READ)) )
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
#if not defined(EXPERIMENTALZERO)
	int w;
	int h;
	int channels_in_file;
	stbi_uc* image = stbi_load_from_memory((stbi_uc const*)I__logo_png, I__logo_png_size, &w, &h, &channels_in_file, 0);

	screen.PlotImage((u32*)image, 0, 0, w, h);

	snprintf(tempBuffer, tempBufferSize, "V%d.%02d", versionMajor, versionMinor);
	screen.PrintText(false, 20, 180, tempBuffer, FileBrowser::Colour(VIC2_COLOUR_INDEX_BLUE));
#endif
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

void DisplayOptions(int y_pos)
{
#if not defined(EXPERIMENTALZERO)
	// print confirmation of parsed options
	snprintf(tempBuffer, tempBufferSize, "ignoreReset = %d\r\n", options.IgnoreReset());
	screen.PrintText(false, 0, y_pos += 16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
	snprintf(tempBuffer, tempBufferSize, "RAMBOard = %d\r\n", options.GetRAMBOard());
	screen.PrintText(false, 0, y_pos += 16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
	snprintf(tempBuffer, tempBufferSize, "splitIECLines = %d\r\n", options.SplitIECLines());
	screen.PrintText(false, 0, y_pos += 16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
	snprintf(tempBuffer, tempBufferSize, "invertIECInputs = %d\r\n", options.InvertIECInputs());
	screen.PrintText(false, 0, y_pos += 16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
	snprintf(tempBuffer, tempBufferSize, "invertIECOutputs = %d\r\n", options.InvertIECOutputs());
	screen.PrintText(false, 0, y_pos += 16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
	snprintf(tempBuffer, tempBufferSize, "i2cLcdAddress = %d\r\n", options.I2CLcdAddress());
	screen.PrintText(false, 0, y_pos += 16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
	snprintf(tempBuffer, tempBufferSize, "i2cLcdFlip = %d\r\n", options.I2CLcdFlip());
	screen.PrintText(false, 0, y_pos += 16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
	snprintf(tempBuffer, tempBufferSize, "LCDName = %s\r\n", options.GetLCDName());
	screen.PrintText(false, 0, y_pos += 16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
	snprintf(tempBuffer, tempBufferSize, "LcdLogoName = %s\r\n", options.GetLcdLogoName());
	screen.PrintText(false, 0, y_pos += 16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
	snprintf(tempBuffer, tempBufferSize, "AutoBaseName = %s\r\n", options.GetAutoBaseName());
	screen.PrintText(false, 0, y_pos += 16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
#endif
}

void DisplayI2CScan(int y_pos)
{
#if not defined(EXPERIMENTALZERO)
	int BSCMaster = options.I2CBusMaster();

	snprintf(tempBuffer, tempBufferSize, "Scanning i2c bus %d ...\r\n", BSCMaster);
	screen.PrintText(false, 0, y_pos , tempBuffer, COLOUR_WHITE, COLOUR_BLACK);

	RPI_I2CInit(BSCMaster, 1);

	int count=0;
	int ptr = 0;
	ptr = snprintf (tempBuffer+ptr, tempBufferSize-ptr, "Found ");
	for (int address = 0; address<128; address++)
	{
		if (RPI_I2CScan(BSCMaster, address))
		{
			ptr += snprintf (tempBuffer+ptr, tempBufferSize-ptr, "%3d ", address);
			count++;
		}
	}
	if (count == 0)
		ptr += snprintf (tempBuffer+ptr, tempBufferSize-ptr, "Nothing");

	screen.PrintText(false, 0, y_pos+16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
#endif
}

static void CheckOptions()
{
	FIL fp;
	FRESULT res;

	u32 widthText, heightText;
	u32 widthScreen = screen.Width();
	u32 heightScreen = screen.Height();
	u32 xpos, ypos;

	deviceID = (u8)options.GetDeviceID();
	DEBUG_LOG("DeviceID = %d\r\n", deviceID);
#if not defined(EXPERIMENTALZERO)
	const char* FontROMName = options.GetRomFontName();
	if (FontROMName)
	{
		char FontROMName2[256] = "/roms/";

		if (FontROMName[0] != '/')	// not a full path, prepend /roms/
			strncat (FontROMName2, FontROMName, 240);
		else
			FontROMName2[0] = 0;

		//DEBUG_LOG("%d Rom Name = %s\r\n", ROMIndex, ROMName);
		if ( (FR_OK == f_open(&fp, FontROMName, FA_READ))
			|| (FR_OK == f_open(&fp, FontROMName2, FA_READ)) )
		{
			u32 bytesRead;

			screen.Clear(COLOUR_BLACK);
			snprintf(tempBuffer, tempBufferSize, "Loading Font ROM %s\r\n", FontROMName);
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
#endif
	const char* ROMName1581 = options.GetRomName1581();
	if (ROMName1581)
	{
		//DEBUG_LOG("%d Rom Name = %s\r\n", ROMIndex, ROMName);
		if ((FR_OK == f_open(&fp, ROMName1581, FA_READ)))
		{
			u32 bytesRead;

			screen.Clear(COLOUR_BLACK);
			snprintf(tempBuffer, tempBufferSize, "Loading ROM %s\r\n", ROMName1581);
			screen.MeasureText(false, tempBuffer, &widthText, &heightText);
			xpos = (widthScreen - widthText) >> 1;
			ypos = (heightScreen - heightText) >> 1;
			screen.PrintText(false, xpos, ypos, tempBuffer, COLOUR_WHITE, COLOUR_RED);

			SetACTLed(true);
			res = f_read(&fp, roms.ROMImage1581, ROMs::ROM1581_SIZE, &bytesRead);
			SetACTLed(false);
			if (res == FR_OK && bytesRead == ROMs::ROM1581_SIZE)
			{
				strncpy(roms.ROMName1581, ROMName1581, 255);
				roms.UpdateLongestRomNameLen(strlen(roms.ROMName1581));
			}
			f_close(&fp);
			//DEBUG_LOG("Read ROM %s from options\r\n", ROMName);
		}
	}

	int ROMIndex;

	for (ROMIndex = ROMs::MAX_ROMS - 1; ROMIndex >= 0; --ROMIndex)
	{
		roms.ROMValid[ROMIndex] = false;
		const char* ROMName = options.GetRomName(ROMIndex);
		char ROMName2[256] = "/roms/";

		if (ROMName[0] == 0)
			continue;

		if (ROMName[0] != '/')	// not a full path, prepend /roms/
			strncat (ROMName2, ROMName, 240);
		else
			ROMName2[0] = 0;

		//DEBUG_LOG("%d Rom Name = %s\r\n", ROMIndex, ROMName);
		if ( (FR_OK == f_open(&fp, ROMName, FA_READ))
			|| (FR_OK == f_open(&fp, ROMName2, FA_READ)) )
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
				roms.UpdateLongestRomNameLen( strlen(roms.ROMNames[ROMIndex]) );
			}
			f_close(&fp);
			//DEBUG_LOG("Read ROM %s from options\r\n", ROMName);
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

	inputMappings->INPUT_BUTTON_ENTER = options.GetButtonEnter();
	inputMappings->INPUT_BUTTON_UP = options.GetButtonUp();
	inputMappings->INPUT_BUTTON_DOWN = options.GetButtonDown();
	inputMappings->INPUT_BUTTON_BACK = options.GetButtonBack();
	inputMappings->INPUT_BUTTON_INSERT = options.GetButtonInsert();
}

void Reboot_Pi()
{
	if (screenLCD)
		screenLCD->ClearInit(0);
	reboot_now();
}

bool SwitchDrive(const char* drive)
{
	FRESULT res;

	res = f_chdrive(drive);
	DEBUG_LOG("chdrive %s res %d\r\n", drive, res);
	return res == FR_OK;
}

void UpdateFirmwareToSD()
{
#if not defined(EXPERIMENTALZERO)
	const char* firmwareName = "kernel.img";
	DIR dir;
	FILINFO filInfo;
	FRESULT res;
	u32 widthText, heightText;
	u32 widthScreen = screen.Width();
	u32 heightScreen = screen.Height();
	u32 xpos, ypos;

	if (SwitchDrive("USB01:"))
	{
		char cwd[1024];
		if (f_getcwd(cwd, 1024) == FR_OK)
		{
			f_chdir("\\");

			bool found = f_findfirst(&dir, &filInfo, ".", firmwareName) == FR_OK;

			if (found)
			{
				char* mem = (char*)malloc((u32)filInfo.fsize);
				if (mem)
				{
					FIL fp;
					u32 bytes;
					res = f_open(&fp, firmwareName, FA_READ);
					if (res == FR_OK)
					{
						screen.Clear(COLOUR_BLACK);
						snprintf(tempBuffer, tempBufferSize, "Checking firmware on USB.\r\n");
						screen.MeasureText(false, tempBuffer, &widthText, &heightText);
						xpos = (widthScreen - widthText) >> 1;
						ypos = (heightScreen - heightText) >> 1;
						screen.PrintText(false, xpos, ypos, tempBuffer, COLOUR_WHITE, COLOUR_RED);

						res = f_read(&fp, mem, (u32)filInfo.fsize, &bytes);
						f_close(&fp);
						if ((res == FR_OK) && ((u32)filInfo.fsize == bytes))
						{
							if (SwitchDrive("SD:"))
							{
								if (f_chdir("\\") == FR_OK)
								{
									bool same = true;
									if (FR_OK == f_open(&fp, firmwareName, FA_READ))
									{
										char* ptr = mem;
										char buffer[256];
										unsigned bufferIndex = 0;
										unsigned bytesRead;
										do
										{
											f_read(&fp, buffer, 256, &bytesRead);

											for (unsigned index = 0; index < bytesRead; ++index)
											{
												if (buffer[index] != mem[bufferIndex + index])
												{
													same = false;
													break;
												}
											}
											bufferIndex += bytesRead;
										} while (same && (bufferIndex < (u32)filInfo.fsize));
										f_close(&fp);
									}

									screen.Clear(COLOUR_BLACK);
									if (!same && (FR_OK == f_open(&fp, firmwareName, FA_CREATE_ALWAYS | FA_WRITE)))
									{
										snprintf(tempBuffer, tempBufferSize, "Updating firmware.\r\n");
										screen.MeasureText(false, tempBuffer, &widthText, &heightText);
										xpos = (widthScreen - widthText) >> 1;
										ypos = (heightScreen - heightText) >> 1;
										screen.PrintText(false, xpos, ypos, tempBuffer, COLOUR_WHITE, COLOUR_RED);

										res = f_write(&fp, mem, (u32)filInfo.fsize, &bytes);
										f_close(&fp);
									}
								}
							}
						}
					}
					else
					{
						DEBUG_LOG("failed to open file %s %d\r\n", firmwareName, (int)res);
					}
					free(mem);
				}
			}

			SwitchDrive("USB01:");
			f_chdir(cwd);
		}
	}
#endif
}

void DisplayMessage(int x, int y, bool LCD, const char* message, u32 textColour, u32 backgroundColour)
{
#if not defined(EXPERIMENTALZERO)
	char buffer[256] = { 0 };

	if (!LCD)
	{
		x = screen.ScaleX(x);
		y = screen.ScaleY(y);

		screen.PrintText(false, x, y, (char*)message, textColour, backgroundColour);
	}
	else if (screenLCD)
	{
		RGBA BkColour = RGBA(0, 0, 0, 0xFF);

		core0RefreshingScreen.Acquire();

		screenLCD->Clear(BkColour);
		screenLCD->PrintText(false, x, y, (char*)message, textColour, backgroundColour);
		screenLCD->SwapBuffers();

		core0RefreshingScreen.Release();
	}
#else
	RGBA BkColour = RGBA(0, 0, 0, 0xFF);

	screenLCD->Clear(BkColour);
	screenLCD->PrintText(false, x, y, (char*)message, textColour, backgroundColour);
	screenLCD->SwapBuffers();

#endif
}

extern "C"
{
	void kernel_main(unsigned int r0, unsigned int r1, unsigned int atags)
	{
		FRESULT res;
		FATFS fileSystemSD;
		FATFS fileSystemUSB[16];

		m_EMMC.Initialize();

#if not defined(EXPERIMENTALZERO)
		RPI_AuxMiniUartInit(115200, 8);
#endif

		disk_setEMM(&m_EMMC);
		f_mount(&fileSystemSD, "SD:", 1);

		LoadOptions();

		InitialiseHardware();
		enable_MMU_and_IDCaches();
		_enable_unaligned_access();

		write32(ARM_GPIO_GPCLR0, 0xFFFFFFFF);

		DisplayLogo();

		InitialiseLCD();
#if not defined(EXPERIMENTALZERO)
		int y_pos = 184;
		snprintf(tempBuffer, tempBufferSize, "Copyright(C) 2018 Stephen White");
		screen.PrintText(false, 0, y_pos+=16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
		snprintf(tempBuffer, tempBufferSize, "This program comes with ABSOLUTELY NO WARRANTY.");
		screen.PrintText(false, 0, y_pos+=16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);
		snprintf(tempBuffer, tempBufferSize, "This is free software, and you are welcome to redistribute it.");
		screen.PrintText(false, 0, y_pos+=16, tempBuffer, COLOUR_WHITE, COLOUR_BLACK);

		if (options.I2CScan())
			DisplayI2CScan(y_pos+=32);

		if (options.ShowOptions())
			DisplayOptions(y_pos+=32);

#endif
		//if (!options.QuickBoot())
			//IEC_Bus::WaitMicroSeconds(3 * 1000000);

		InterruptSystemInitialize();
#if not defined(EXPERIMENTALZERO)
		TimerSystemInitialize();

		USPiInitialize();

		DEBUG_LOG("\r\n");

		numberOfUSBMassStorageDevices = USPiMassStorageDeviceAvailable();
		DEBUG_LOG("%d USB Mass Storage Devices found\r\n", numberOfUSBMassStorageDevices);

		USBKeyboardDetected = USPiKeyboardAvailable();
		if (!USBKeyboardDetected)
			DEBUG_LOG("Keyboard not found\r\n");
		else
			DEBUG_LOG("Keyboard found\r\n");

		//if (!USPiMouseAvailable())
		//	DEBUG_LOG("Mouse not found\r\n");
		//else
		//	DEBUG_LOG("Mouse found\r\n");

		keyboard = new Keyboard();
#endif
		inputMappings = new InputMappings();
		//USPiMouseRegisterStatusHandler(MouseHandler);


		CheckOptions();

		IEC_Bus::SetSplitIECLines(options.SplitIECLines());
		IEC_Bus::SetInvertIECInputs(options.InvertIECInputs());
		IEC_Bus::SetInvertIECOutputs(options.InvertIECOutputs());
		IEC_Bus::SetIgnoreReset(options.IgnoreReset());
		//ROTARY: Added for rotary encoder support - 09/05/2019 by Geo...
		IEC_Bus::SetRotaryEncoderEnable(options.RotaryEncoderEnable());
#if not defined(EXPERIMENTALZERO)
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

		for (int USBDriveIndex = 0; USBDriveIndex < numberOfUSBMassStorageDevices; ++USBDriveIndex)
		{
			char USBDriveId[16];
			disk_setUSB(USBDriveIndex);
			sprintf(USBDriveId, "USB%02d:", USBDriveIndex + 1);
			res = f_mount(&fileSystemUSB[USBDriveIndex], USBDriveId, 1);
		}
		if (numberOfUSBMassStorageDevices > 0)
		{
			if (SwitchDrive("USB01:"))
				UpdateFirmwareToSD();
		}
#endif
		f_chdir("/1541");

		m_IEC_Commands.SetStarFileName(options.GetStarFileName());

		GlobalSetDeviceID(deviceID);

		pi1541.drive.SetVIA(&pi1541.VIA[1]);
		pi1541.VIA[0].GetPortB()->SetPortOut(0, IEC_Bus::PortB_OnPortOut);
		IEC_Bus::Initialise();
		if (screenLCD)
			screenLCD->ClearInit(0);

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

