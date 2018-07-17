#include <stdio.h>
#include "rpi-aux.h"
#include "rpi-base.h"
#include "rpi-gpio.h"
#include "startup.h"
#include "stdlib.h"

#include "rpiHardware.h"

/* Define the system clock frequency in MHz for the baud rate calculation.
This is clearly defined on the BCM2835 datasheet errata page:
http://elinux.org/BCM2835_datasheet_errata */
#define FALLBACK_SYS_FREQ    250000000

#define I2C_BSC0_BASE	(PERIPHERAL_BASE + 0x205000)
#define I2C_BSC1_BASE	(PERIPHERAL_BASE + 0x804000)

#define GetBaseAddress(BSCMaster) (BSCMaster == 0 ? I2C_BSC0_BASE : I2C_BSC1_BASE)

#define I2C_BSC_C 0x00		// Control register
#define I2C_BSC_S 0x04		// Statis register
#define I2C_BSC_DLEN 0x08
#define I2C_BSC_A 0x0C		// Slave address
#define I2C_BSC_FIFO 0x10
#define I2C_BSC_DIV 0x14
#define I2C_BSC_DEL 0x18
#define I2C_BSC_CLKT 0x1C

#define CONTROL_BIT_READ (1 << 0)	// READ Read Transfer
#define CONTROL_BIT_CLEAR0 (1 << 4)	// CLEAR FIFO 00 = No action. x1 = Clear FIFO. One shot operation. 1x = Clear FIFO.
#define CONTROL_BIT_CLEAR1 (1 << 5)
#define CONTROL_BIT_ST (1 << 7)		// ST Start Transfer 
#define CONTROL_BIT_INTD (1 << 8)	// INTD Interrupt on DONE
#define CONTROL_BIT_INTT (1 << 9)	// INTT Interrupt on TX
#define CONTROL_BIT_INTR (1 << 10)	// INTR Interrupt on RX
#define CONTROL_BIT_I2CEN (1 << 15)	// I2C Enable


#define STATUS_BIT_TA (1 << 0)		// Transfer Active
#define STATUS_BIT_DONE (1 << 1)	// Done
#define STATUS_BIT_TXW (1 << 2)		// FIFO needs Writing (full)
#define STATUS_BIT_RXR (1 << 3)		// FIFO needs Reading (full)
#define STATUS_BIT_TXD (1 << 4)		// FIFO can accept Data
#define STATUS_BIT_RXD (1 << 5)		// FIFO contains Data
#define STATUS_BIT_TXE (1 << 6)		// FIFO Empty
#define STATUS_BIT_RXF (1 << 7)		// FIFO Full
#define STATUS_BIT_ERR (1 << 8)		// ERR ACK Error
#define STATUS_BIT_CLKT (1 << 9)	// CLKT Clock Stretch Timeout

#define FIFO_SIZE 16

void RPI_I2CSetClock(int BSCMaster, int clock_freq)
{
	_data_memory_barrier();

	int sys_freq = get_clock_rate(CORE_CLK_ID);

	if (!sys_freq) {
		sys_freq = FALLBACK_SYS_FREQ;
	}

	unsigned short divider = (unsigned short)(sys_freq / clock_freq);
	write32(GetBaseAddress(BSCMaster) + I2C_BSC_DIV, divider);

	_data_memory_barrier();
}

void RPI_I2CInit(int BSCMaster, int fast)
{
	if (BSCMaster == 0)
	{
		RPI_SetGpioPinFunction(RPI_GPIO0, FS_ALT0);
		RPI_SetGpioPinFunction(RPI_GPIO1, FS_ALT0);
	}
	else
	{
		RPI_SetGpioPinFunction(RPI_GPIO2, FS_ALT0);
		RPI_SetGpioPinFunction(RPI_GPIO3, FS_ALT0);
	}

	RPI_I2CSetClock(BSCMaster, fast != 0 ? 400000 : 100000);
}

int RPI_I2CRead(int BSCMaster, unsigned char slaveAddress, void* buffer, unsigned count)
{
	int success = 0;
	if (slaveAddress < 0x80)
	{
		if (count > 0)
		{
			unsigned baseAddress = GetBaseAddress(BSCMaster);
			unsigned char* data = (unsigned char*)buffer;

			write32(baseAddress + I2C_BSC_A, slaveAddress);
			write32(baseAddress + I2C_BSC_C, CONTROL_BIT_CLEAR1);
			write32(baseAddress + I2C_BSC_S, STATUS_BIT_CLKT | STATUS_BIT_ERR | STATUS_BIT_DONE);
			write32(baseAddress + I2C_BSC_DLEN, count);
			write32(baseAddress + I2C_BSC_C, CONTROL_BIT_I2CEN | CONTROL_BIT_ST | CONTROL_BIT_READ);

			while (!(read32(baseAddress + I2C_BSC_S) & STATUS_BIT_DONE))
			{
				while (read32(baseAddress + I2C_BSC_S) & STATUS_BIT_RXD)
				{
					*data++ = (unsigned char)read32(baseAddress + I2C_BSC_FIFO);
					count--;
				}
			}

			while (count > 0 && (read32(baseAddress + I2C_BSC_S) & STATUS_BIT_RXD))
			{
				*data++ = (unsigned char)read32(baseAddress + I2C_BSC_FIFO);
				count--;
			}

			unsigned status = read32(baseAddress + I2C_BSC_S);
			if (status & STATUS_BIT_ERR)
			{
				write32(baseAddress + I2C_BSC_S, STATUS_BIT_ERR);
			}
			else if ((status & STATUS_BIT_CLKT) == 0 && count == 0)
			{
				success = 1;
			}

			write32(baseAddress + I2C_BSC_S, STATUS_BIT_DONE);
		}
		else
		{
			success = 1;
		}
	}
	return success;
}

int RPI_I2CWrite(int BSCMaster, unsigned char slaveAddress, void* buffer, unsigned count)
{
	int success = 0;
	if (slaveAddress < 0x80)
	{
		if (count > 0)
		{
			unsigned baseAddress = GetBaseAddress(BSCMaster);
			unsigned char* data = (unsigned char*)buffer;

			write32(baseAddress + I2C_BSC_A, slaveAddress);
			write32(baseAddress + I2C_BSC_C, CONTROL_BIT_CLEAR1);
			write32(baseAddress + I2C_BSC_S, STATUS_BIT_CLKT | STATUS_BIT_ERR | STATUS_BIT_DONE);
			write32(baseAddress + I2C_BSC_DLEN, count);

			for (unsigned i = 0; count > 0 && i < FIFO_SIZE; i++)
			{
				write32(baseAddress + I2C_BSC_FIFO, *data++);
				count--;
			}

			write32(baseAddress + I2C_BSC_C, CONTROL_BIT_I2CEN | CONTROL_BIT_ST);

			while (!(read32(baseAddress + I2C_BSC_S) & STATUS_BIT_DONE))
			{
				while (count > 0 && (read32(baseAddress + I2C_BSC_S) & STATUS_BIT_TXD))
				{
					write32(baseAddress + I2C_BSC_FIFO, *data++);
					count--;
				}
			}

			unsigned status = read32(baseAddress + I2C_BSC_S);
			if (status & STATUS_BIT_ERR)
			{
				write32(baseAddress + I2C_BSC_S, STATUS_BIT_ERR);
			}
			else if ((status & STATUS_BIT_CLKT) == 0 && count == 0)
			{
				success = 1;
			}

			write32(baseAddress + I2C_BSC_S, STATUS_BIT_DONE);
		}
		else
		{
			success = 1;
		}
	}
	//DEBUG_LOG("I2C Write %d %d\r\n", count, success);
	return success;
}

int RPI_I2CScan(int BSCMaster, unsigned char slaveAddress)
{
	int success = 1;
	if (slaveAddress < 0x80)
	{
		unsigned baseAddress = GetBaseAddress(BSCMaster);

		write32(baseAddress + I2C_BSC_A, slaveAddress);
		write32(baseAddress + I2C_BSC_C, CONTROL_BIT_CLEAR1);
		write32(baseAddress + I2C_BSC_S, STATUS_BIT_CLKT | STATUS_BIT_ERR | STATUS_BIT_DONE);
		write32(baseAddress + I2C_BSC_DLEN, 1);
		write32(baseAddress + I2C_BSC_C, CONTROL_BIT_I2CEN | CONTROL_BIT_ST | CONTROL_BIT_READ);

		while (!(read32(baseAddress + I2C_BSC_S) & STATUS_BIT_DONE))
		{
		}

		unsigned status = read32(baseAddress + I2C_BSC_S);
		if (status & (STATUS_BIT_ERR | STATUS_BIT_CLKT) )
		{
			success = 0;
		}

		write32(baseAddress + I2C_BSC_S, STATUS_BIT_CLKT | STATUS_BIT_ERR | STATUS_BIT_DONE);
	}
	return success;
}

