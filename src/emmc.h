#ifndef _SDCard_emmc_h
#define _SDCard_emmc_h

#define PACKED		__attribute__ ((packed))

#include "types.h"

#define BOOT_SIGNATURE		0xAA55

struct TSCR			// SD configuration register
{
	u32	scr[2];
	u32	sd_bus_widths;
	int	sd_version;
};

class CEMMCDevice
{
public:
	CEMMCDevice();
	~CEMMCDevice();

	bool Initialize(void);

	int Read(void *pBuffer, unsigned nCount);
	int Write(const void *pBuffer, unsigned nCount);

	unsigned long long Seek(unsigned long long ullOffset);

	int DoRead2(u8 *buf, size_t buf_size, u32 block_no);
	int DoRead(u8 *buf, size_t buf_size, u32 block_no);
	int DoWrite(u8 *buf, size_t buf_size, u32 block_no);

private:
	bool PowerOn(void);
	void PowerOff(void);

	u32 GetBaseClock(void);
	u32 GetClockDivider(u32 base_clock, u32 target_rate);
	int SwitchClockRate(u32 base_clock, u32 target_rate);

	int ResetCmd(void);
	int ResetDat(void);

	void IssueCommandInt(u32 cmd_reg, u32 argument, int timeout);
	void HandleCardInterrupt(void);
	void HandleInterrupts(void);
	bool IssueCommand(u32 command, u32 argument, int timeout = 500000);

	int CardReset(void);
	int CardInit(void);

	int EnsureDataMode(void);
	int DoDataCommand(int is_write, u8 *buf, size_t buf_size, u32 block_no);

	int TimeoutWait(unsigned reg, unsigned mask, int value, unsigned usec);

	void usDelay(unsigned usec);

private:
	u64 m_ullOffset;

	u32 m_hci_ver;

	// was: struct emmc_block_dev
	u32 m_device_id[4];

	u32 m_card_supports_sdhc;
	u32 m_card_supports_18v;
	u32 m_card_ocr;
	u32 m_card_rca;
	u32 m_last_interrupt;
	u32 m_last_error;

	TSCR m_SCR;

	int m_failed_voltage_switch;

	u32 m_last_cmd_reg;
	u32 m_last_cmd;
	u32 m_last_cmd_success;
	u32 m_last_r0;
	u32 m_last_r1;
	u32 m_last_r2;
	u32 m_last_r3;

	void *m_buf;
	int m_blocks_to_transfer;
	size_t m_block_size;
	int m_card_removal;
	u32 m_base_clock;

	static const char *sd_versions[];
	static const char *err_irpts[];
	static const u32 sd_commands[];
	static const u32 sd_acommands[];
};
#endif
