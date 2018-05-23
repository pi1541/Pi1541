/*-----------------------------------------------------------------------*/
/* Low level disk I/O module skeleton for FatFs     (C)ChaN, 2016        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include "diskio.h"		/* FatFs lower layer API */

/* Definitions of physical drive number for each drive */
#define DEV_MMC		0	/* Example: Map MMC/SD card to physical drive 0 */

//static struct emmc_block_dev *emmc_dev;
static CEMMCDevice* pEMMC;

#define SD_BLOCK_SIZE		512

void disk_setEMM(CEMMCDevice* pEMMCDevice)
{
	pEMMC = pEMMCDevice;
}

int sd_card_init(struct block_device **dev)
{
	return 0;
}

size_t sd_read(uint8_t *buf, size_t buf_size, uint32_t block_no)
{
//	g_pLogger->Write("", LogNotice, "sd_read %d", block_no);

	return pEMMC->DoRead(buf, buf_size, block_no);
}

size_t sd_write(uint8_t *buf, size_t buf_size, uint32_t block_no)
{
	return pEMMC->DoWrite(buf, buf_size, block_no);
}


/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	//DSTATUS stat;
	//int result;

	//switch (pdrv) {
	////case DEV_RAM :
	////	result = RAM_disk_status();

	////	// translate the reslut code here

	////	return stat;

	//case DEV_MMC :

	//	result = MMC_disk_status();

	//	// translate the reslut code here

	//	return stat;

	////case DEV_USB :
	////	result = USB_disk_status();

	////	// translate the reslut code here

	////	return stat;
	//}
	//return STA_NOINIT;
	return 0;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	//DSTATUS stat;
	//int result;

	//switch (pdrv) {
	////case DEV_RAM :
	////	result = RAM_disk_initialize();

	////	// translate the reslut code here

	////	return stat;

	////case DEV_MMC :
	////	result = MMC_disk_initialize();

	////	// translate the reslut code here

	////	return stat;

	////case DEV_USB :
	////	result = USB_disk_initialize();

	////	// translate the reslut code here

	////	return stat;
	//}
	//return STA_NOINIT;
	return 0;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	DWORD sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	//DRESULT res;
	//int result;

//	g_pLogger->Write("", LogNotice, "disk_read pdrv = %d", pdrv);

	switch (pdrv) {
	//case DEV_RAM :
	//	// translate the arguments here

	//	result = RAM_disk_read(buff, sector, count);

	//	// translate the reslut code here

	//	return res;

	case DEV_MMC :
		// translate the arguments here

		//result = MMC_disk_read(buff, sector, count);

//		g_pLogger->Write("", LogNotice, "!!!!!!!!!!!!!!!!!!!!!!!!!!!!disk_read %d %d buf_size = 0x%x", sector, count, buf_size);

		for (UINT s = 0; s < count; ++s)
		{
			if (sd_read(buff, SD_BLOCK_SIZE, sector+s) < SD_BLOCK_SIZE)
			{
				return RES_ERROR;
			}
			buff += SD_BLOCK_SIZE;
		}
		return RES_OK;

	//case DEV_USB :
	//	// translate the arguments here

	//	result = USB_disk_read(buff, sector, count);

	//	// translate the reslut code here

	//	return res;
	}

	return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	DWORD sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	//DRESULT res;
	//int result;

	switch (pdrv) {
	//case DEV_RAM :
	//	// translate the arguments here

	//	result = RAM_disk_write(buff, sector, count);

	//	// translate the reslut code here

	//	return res;

	case DEV_MMC :
		// translate the arguments here

		//result = MMC_disk_write(buff, sector, count);
		//size_t buf_size = count * SD_BLOCK_SIZE;
		//if (sd_write((uint8_t *)buff, buf_size, sector) < buf_size)
		//{
		//	return RES_ERROR;
		//}

		for (UINT s = 0; s < count; ++s)
		{
			if (sd_write((uint8_t *)buff, SD_BLOCK_SIZE, sector+s) < SD_BLOCK_SIZE)
			{
				return RES_ERROR;
			}
			buff += SD_BLOCK_SIZE;
		}

		return RES_OK;

	//case DEV_USB :
	//	// translate the arguments here

	//	result = USB_disk_write(buff, sector, count);

	//	// translate the reslut code here

	//	return res;
	}

	return RES_PARERR;
}



/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	//DRESULT res;
	//int result;

	//switch (pdrv) {
	//case DEV_RAM :

	//	// Process of the command for the RAM drive

	//	return res;

	//case DEV_MMC :

	//	// Process of the command for the MMC/SD card

	//	return res;

	//case DEV_USB :

	//	// Process of the command the USB drive

	//	return res;
	//}

	return RES_PARERR;
}

