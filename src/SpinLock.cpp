//
// spinlock.cpp
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2015  R. Stange <rsta2@o2online.de>
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include "SpinLock.h"
#include "defs.h"

bool SpinLock::s_bEnabled = true;

SpinLock::SpinLock()
	: m_bLocked(false)
{
}

SpinLock::~SpinLock(void)
{
}

void SpinLock::Acquire(void)
{
#ifdef HAS_MULTICORE
	if (s_bEnabled)
	{
		// See: ARMv7-A Architecture Reference Manual, Section D7.3
		asm volatile
			(
			"mov r1, %0\n"
			"mov r2, #1\n"
			"1: ldrex r3, [r1]\n"
			"cmp r3, #0\n"
			"wfene\n"
			"strexeq r3, r2, [r1]\n"
			"cmpeq r3, #0\n"
			"bne 1b\n"
			"dmb\n"

			: : "r" ((u32)&m_bLocked)
			);
	}
#endif
}

void SpinLock::Release(void)
{
#ifdef HAS_MULTICORE
	if (s_bEnabled)
	{
		// See: ARMv7-A Architecture Reference Manual, Section D7.3
		asm volatile
			(
			"mov r1, %0\n"
			"mov r2, #0\n"
			"dmb\n"
			"str r2, [r1]\n"
			"dsb\n"
			"sev\n"

			: : "r" ((u32)&m_bLocked)
			);
	}
#endif
}

