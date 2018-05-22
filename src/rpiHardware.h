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

#ifndef RPIHARDWARE_H
#define RPIHARDWARE_H

#include <stdio.h>
#include "types.h"
#include "rpi-gpio.h"
#include "debug.h"

#define DMA_ENABLE (PERIPHERAL_BASE + 0x7FF0)	// Global Enable bits for each DMA Channel
#define DMA0_BASE (PERIPHERAL_BASE + 0x7000)	// DMA Channel 0 Register Set
#define DMA_CONBLK_AD 4		// DMA Channel 0..14 Control Block Address
#define DMA_CS 0			// DMA Channel 0..14 Control & Status
#define DMA_ACTIVE 1
#define DMA_END 2

#define DMA_DEST_DREQ 0x40
#define DMA_SRC_INC 0x100
#define DMA_PERMAP_5 0x50000

#define ARM_GPIO_GPFSEL0	(RPI_GPIO_BASE + 0x00)
#define ARM_GPIO_GPFSEL1	(RPI_GPIO_BASE + 0x04)
#define ARM_GPIO_GPFSEL4	(RPI_GPIO_BASE + 0x10)
#define ARM_GPIO_GPSET0		(RPI_GPIO_BASE + 0x1C)
#define ARM_GPIO_GPCLR0		(RPI_GPIO_BASE + 0x28)
#define ARM_GPIO_GPLEV0		(RPI_GPIO_BASE + 0x34)
#define ARM_GPIO_GPEDS0		(RPI_GPIO_BASE + 0x40)
#define ARM_GPIO_GPREN0		(RPI_GPIO_BASE + 0x4C)
#define ARM_GPIO_GPFEN0		(RPI_GPIO_BASE + 0x58)
#define ARM_GPIO_GPHEN0		(RPI_GPIO_BASE + 0x64)
#define ARM_GPIO_GPLEN0		(RPI_GPIO_BASE + 0x70)
#define ARM_GPIO_GPAREN0	(RPI_GPIO_BASE + 0x7C)
#define ARM_GPIO_GPAFEN0	(RPI_GPIO_BASE + 0x88)
#define ARM_GPIO_GPPUD		(RPI_GPIO_BASE + 0x94)
#define ARM_GPIO_GPPUDCLK0	(RPI_GPIO_BASE + 0x98)


#define ARM_SYSTIMER_BASE	(PERIPHERAL_BASE + 0x3000)

#define ARM_SYSTIMER_CS		(ARM_SYSTIMER_BASE + 0x00)
#define ARM_SYSTIMER_CLO	(ARM_SYSTIMER_BASE + 0x04)
#define ARM_SYSTIMER_CHI	(ARM_SYSTIMER_BASE + 0x08)
#define ARM_SYSTIMER_C0		(ARM_SYSTIMER_BASE + 0x0C)
#define ARM_SYSTIMER_C1		(ARM_SYSTIMER_BASE + 0x10)
#define ARM_SYSTIMER_C2		(ARM_SYSTIMER_BASE + 0x14)
#define ARM_SYSTIMER_C3		(ARM_SYSTIMER_BASE + 0x18)

// External Mass Media Controller (SD Card)
#define ARM_EMMC_BASE		(PERIPHERAL_BASE + 0x300000)

#define DEVICE_ID_SD_CARD	0
#define DEVICE_ID_USB_HCD	3

#define POWER_STATE_OFF		(0 << 0)
#define POWER_STATE_ON		(1 << 0)
#define POWER_STATE_WAIT	(1 << 1)
#define POWER_STATE_DEVICE_DOESNT_EXIST	(1 << 1)	// in response

#define CLOCK_ID_EMMC		1
#define CLOCK_ID_UART		2
#define CLOCK_ID_CORE		4

#define CM_BASE (PERIPHERAL_BASE + 0x101000)
#define CM_PWMCTL (CM_BASE + 0xA0)			// Clock Manager PWM Clock Control
#define CM_PWMDIV (CM_BASE + 0xA4)			// Clock Manager PWM Clock Divisor
#define CM_PASSWORD 0x5A000000				// Clock Control Password "5A"

#define CM_SRC_OSCILLATOR 0x01
#define CM_ENAB 0x10	// Enable The Clock Generator

#define PWM_BASE (PERIPHERAL_BASE + 0x20C000)
#define PWM_CTL (PWM_BASE + 0x0)
#define PWM_STATUS (PWM_BASE + 0x4)
#define PWM_DMAC (PWM_BASE + 0x8)
#define PWM_RNG1 (PWM_BASE + 0x10)
#define PWM_FIF1 (PWM_BASE + 0x18) // PWM FIFO Input
#define PWM_RNG2 (PWM_BASE + 0x20)

#define PWM_ENAB  0x80000000	// PWM DMA Configuration  DMA Enable

#define PWM_PWEN1 0x1 // Channel 1 Enable
#define PWM_MODE1 0x2 // Channel 1 Mode
#define PWM_RPTL1 0x4 // Channel 1 Repeat Last Data
#define PWM_SBIT1 0x8 // Channel 1 Silence Bit
#define PWM_POLA1 0x10 // Channel 1 Polarity
#define PWM_USEF1 0x20 // Channel 1 Use Fifo
#define PWM_CLRF1 0x40 // Clear Fifo
#define PWM_MSEN1 0x80 // Channel 1 M / S Enable
#define PWM_PWEN2 0x100 // Channel 2 Enable
#define PWM_MODE2 0x200 // Channel 2 Mode
#define PWM_RPTL2 0x400 // Channel 2 Repeat Last Data
#define PWM_SBIT2 0x800 // Channel 2 Silence Bit
#define PWM_POLA2 0x1000 // Channel 2 Polarity
#define PWM_USEF2 0x2000 // Channel 2 Use Fifo
#define PWM_MSEN2 0x8000 // Channel 2 M / S Enable

// PWM Status flags
#define PWM_FULL1 0x1
#define PWM_EMPT1 0x2

//
// Interrupt Controller
//
#define ARM_IC_BASE		(PERIPHERAL_BASE + 0xB000)

#define ARM_IC_IRQ_BASIC_PENDING  (ARM_IC_BASE + 0x200)
#define ARM_IC_IRQ_PENDING_1	  (ARM_IC_BASE + 0x204)
#define ARM_IC_IRQ_PENDING_2	  (ARM_IC_BASE + 0x208)
#define ARM_IC_FIQ_CONTROL	  (ARM_IC_BASE + 0x20C)
#define ARM_IC_ENABLE_IRQS_1	  (ARM_IC_BASE + 0x210)
#define ARM_IC_ENABLE_IRQS_2	  (ARM_IC_BASE + 0x214)
#define ARM_IC_ENABLE_BASIC_IRQS  (ARM_IC_BASE + 0x218)
#define ARM_IC_DISABLE_IRQS_1	  (ARM_IC_BASE + 0x21C)
#define ARM_IC_DISABLE_IRQS_2	  (ARM_IC_BASE + 0x220)
#define ARM_IC_DISABLE_BASIC_IRQS (ARM_IC_BASE + 0x224)

#ifdef __cplusplus
extern "C" {
#endif
#include "rpi-mailbox-interface.h"

	static inline u32 read32(unsigned int nAddress)
	{
		return *(u32 volatile *)nAddress;
	}

	static inline void write32(unsigned int nAddress, u32 nValue)
	{
		*(u32 volatile *)nAddress = nValue;
	}

	static inline void delay_us(u32 amount)
	{
		u32 count;

		for (count = 0; count < amount; ++count)
		{
			unsigned before;
			unsigned after;
			// We try to update every micro second and use as a rough timer to count micro seconds
			before = read32(ARM_SYSTIMER_CLO);
			do
			{
				after = read32(ARM_SYSTIMER_CLO);
			} while (after == before);
		}
	}

	static inline int get_clock_rate(int clk_id)
	{
		rpi_mailbox_property_t *buf;
		RPI_PropertyInit();
		RPI_PropertyAddTag(TAG_GET_CLOCK_RATE, clk_id);
		RPI_PropertyProcess();
		buf = RPI_PropertyGet(TAG_GET_CLOCK_RATE);
		if (buf)
			return buf->data.buffer_32[1];
		else
			return 0;
	}

#ifdef __cplusplus
}
#endif

//DMB - whenever a memory access requires ordering with regards to another memory access.
//DSB - whenever a memory access needs to have completed before program execution progresses.
//ISB - whenever instruction fetches need to explicitly take place after a certain point in the program, for example after memory map updates or after writing code to be executed. (In practice, this means "throw away any prefetched instructions at this point".)

//DMB - It prevents reordering of data accesses instructions across itself. All data accesses by this processor / core before the DMB will be visible to all other masters within the specified shareability domain before any of the data accesses after it.
//		It also ensures that any explicit preceding data(or unified) cache maintenance operations have completed before any subsequent data accesses are executed.

#if defined(RPI2) || defined(RPI3)
	#define DataSyncBarrier()	asm volatile ("dsb" ::: "memory")
	#define DataMemBarrier() 	asm volatile ("dmb" ::: "memory")

	#define InstructionSyncBarrier() __asm volatile ("isb" ::: "memory")
	#define InstructionMemBarrier()	__asm volatile ("isb" ::: "memory")
#else
	#define DataSyncBarrier()	asm volatile ("mcr p15, 0, %0, c7, c10, 4" : : "r" (0) : "memory")
	#define DataMemBarrier() 	asm volatile ("mcr p15, 0, %0, c7, c10, 5" : : "r" (0) : "memory")

	#define FlushPrefetchBuffer()	__asm volatile ("mcr p15, 0, %0, c7, c5,  4" : : "r" (0) : "memory")

	#define InstructionSyncBarrier() FlushPrefetchBuffer()
	#define InstructionMemBarrier()	FlushPrefetchBuffer()
#endif

#endif
