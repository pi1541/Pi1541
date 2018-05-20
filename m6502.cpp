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

#include "m6502.h"

M6502::OpcodeCycleFunction M6502::opcodeFunctions[256] =
{
//       0           1           2           3           4           5           6           7           8           9           A           B           C           D           E           F
&M6502::BRK,&M6502::ORA,&M6502::JAM,&M6502::SLO,&M6502::NOP,&M6502::ORA,&M6502::ASL,&M6502::SLO,&M6502::PHP,&M6502::ORA,&M6502::ASL,&M6502::ANC,&M6502::NOP,&M6502::ORA,&M6502::ASL,&M6502::SLO,// 0
&M6502::BPL,&M6502::ORA,&M6502::JAM,&M6502::SLO,&M6502::NOP,&M6502::ORA,&M6502::ASL,&M6502::SLO,&M6502::CLC,&M6502::ORA,&M6502::NOP,&M6502::SLO,&M6502::NOP,&M6502::ORA,&M6502::ASL,&M6502::SLO,// 1
&M6502::JSR,&M6502::AND,&M6502::JAM,&M6502::RLA,&M6502::BIT,&M6502::AND,&M6502::ROL,&M6502::RLA,&M6502::PLP,&M6502::AND,&M6502::ROL,&M6502::ANC,&M6502::BIT,&M6502::AND,&M6502::ROL,&M6502::RLA,// 2
&M6502::BMI,&M6502::AND,&M6502::JAM,&M6502::RLA,&M6502::NOP,&M6502::AND,&M6502::ROL,&M6502::RLA,&M6502::SEC,&M6502::AND,&M6502::NOP,&M6502::RLA,&M6502::NOP,&M6502::AND,&M6502::ROL,&M6502::RLA,// 3
&M6502::RTI,&M6502::EOR,&M6502::JAM,&M6502::SRE,&M6502::NOP,&M6502::EOR,&M6502::LSR,&M6502::SRE,&M6502::PHA,&M6502::EOR,&M6502::LSR,&M6502::ASR,&M6502::JMP,&M6502::EOR,&M6502::LSR,&M6502::SRE,// 4
&M6502::BVC,&M6502::EOR,&M6502::JAM,&M6502::SRE,&M6502::NOP,&M6502::EOR,&M6502::LSR,&M6502::SRE,&M6502::CLI,&M6502::EOR,&M6502::NOP,&M6502::SRE,&M6502::NOP,&M6502::EOR,&M6502::LSR,&M6502::SRE,// 5
&M6502::RTS,&M6502::ADC,&M6502::JAM,&M6502::RRA,&M6502::NOP,&M6502::ADC,&M6502::ROR,&M6502::RRA,&M6502::PLA,&M6502::ADC,&M6502::ROR,&M6502::ARR,&M6502::JMP,&M6502::ADC,&M6502::ROR,&M6502::RRA,// 6
&M6502::BVS,&M6502::ADC,&M6502::JAM,&M6502::RRA,&M6502::NOP,&M6502::ADC,&M6502::ROR,&M6502::RRA,&M6502::SEI,&M6502::ADC,&M6502::NOP,&M6502::RRA,&M6502::NOP,&M6502::ADC,&M6502::ROR,&M6502::RRA,// 7
&M6502::NOP,&M6502::STA,&M6502::NOP,&M6502::SAX,&M6502::STY,&M6502::STA,&M6502::STX,&M6502::SAX,&M6502::DEY,&M6502::NOP,&M6502::TXA,&M6502::XAA,&M6502::STY,&M6502::STA,&M6502::STX,&M6502::SAX,// 8
&M6502::BCC,&M6502::STA,&M6502::JAM,&M6502::SHA,&M6502::STY,&M6502::STA,&M6502::STX,&M6502::SAX,&M6502::TYA,&M6502::STA,&M6502::TXS,&M6502::SHS,&M6502::SHY,&M6502::STA,&M6502::SHX,&M6502::SHA,// 9
&M6502::LDY,&M6502::LDA,&M6502::LDX,&M6502::LAX,&M6502::LDY,&M6502::LDA,&M6502::LDX,&M6502::LAX,&M6502::TAY,&M6502::LDA,&M6502::TAX,&M6502::LXA,&M6502::LDY,&M6502::LDA,&M6502::LDX,&M6502::LAX,// A
&M6502::BCS,&M6502::LDA,&M6502::JAM,&M6502::LAX,&M6502::LDY,&M6502::LDA,&M6502::LDX,&M6502::LAX,&M6502::CLV,&M6502::LDA,&M6502::TSX,&M6502::LAS,&M6502::LDY,&M6502::LDA,&M6502::LDX,&M6502::LAX,// B
&M6502::CPY,&M6502::CMP,&M6502::NOP,&M6502::DCP,&M6502::CPY,&M6502::CMP,&M6502::DEC,&M6502::DCP,&M6502::INY,&M6502::CMP,&M6502::DEX,&M6502::SBX,&M6502::CPY,&M6502::CMP,&M6502::DEC,&M6502::DCP,// C
&M6502::BNE,&M6502::CMP,&M6502::JAM,&M6502::DCP,&M6502::NOP,&M6502::CMP,&M6502::DEC,&M6502::DCP,&M6502::CLD,&M6502::CMP,&M6502::NOP,&M6502::DCP,&M6502::NOP,&M6502::CMP,&M6502::DEC,&M6502::DCP,// D
&M6502::CPX,&M6502::SBC,&M6502::NOP,&M6502::ISB,&M6502::CPX,&M6502::SBC,&M6502::INC,&M6502::ISB,&M6502::INX,&M6502::SBC,&M6502::NOP,&M6502::SBC,&M6502::CPX,&M6502::SBC,&M6502::INC,&M6502::ISB,// E
&M6502::BEQ,&M6502::SBC,&M6502::JAM,&M6502::ISB,&M6502::NOP,&M6502::SBC,&M6502::INC,&M6502::ISB,&M6502::SED,&M6502::SBC,&M6502::NOP,&M6502::ISB,&M6502::NOP,&M6502::SBC,&M6502::INC,&M6502::ISB // F
};

M6502::AddressModeCycleFunction M6502::T1AddressModeFunctions[256] =
{
//       0                     1                2                       3                4                  5                  6                 7                  8                  9                  A                 B                  C                  D                    E              F
&M6502::brk_5_4_T1,&M6502::idx_2_4_T1,&M6502::sb_jam_T1, &M6502::idx_Undoc_T1,&M6502::zp_2_1_T1, &M6502::zp_2_1_T1, &M6502::zp_4_1_T1, &M6502::zp_4_1_T1, &M6502::ph_5_1_T1,&M6502::imm_2_1_T1, &M6502::sb_1_T1,&M6502::imm_2_1_T1, &M6502::abs_2_3_T1, &M6502::abs_2_3_T1, &M6502::abs_4_2_T1, &M6502::abs_4_2_T1, //0
&M6502::rel_5_8_T1,&M6502::idy_2_7_T1,&M6502::sb_jam_T1, &M6502::idy_Undoc_T1,&M6502::zpx_2_6_T1,&M6502::zpx_2_6_T1,&M6502::zpx_4_3_T1,&M6502::zpx_4_3_T1,&M6502::sb_1_T1,  &M6502::absy_2_5_T1,&M6502::sb_1_T1,&M6502::absy_4_4_T1,&M6502::absx_2_5_T1,&M6502::absx_2_5_T1,&M6502::absx_4_4_T1,&M6502::absx_4_4_T1,//1
&M6502::jsr_5_3_T1,&M6502::idx_2_4_T1,&M6502::sb_jam_T1, &M6502::idx_Undoc_T1,&M6502::zp_2_1_T1, &M6502::zp_2_1_T1, &M6502::zp_4_1_T1, &M6502::zp_4_1_T1, &M6502::pl_5_2_T1,&M6502::imm_2_1_T1, &M6502::sb_1_T1,&M6502::imm_2_1_T1, &M6502::abs_2_3_T1, &M6502::abs_2_3_T1, &M6502::abs_4_2_T1, &M6502::abs_4_2_T1, //2
&M6502::rel_5_8_T1,&M6502::idy_2_7_T1,&M6502::sb_jam_T1, &M6502::idy_Undoc_T1,&M6502::zpx_2_6_T1,&M6502::zpx_2_6_T1,&M6502::zpx_4_3_T1,&M6502::zpx_4_3_T1,&M6502::sb_1_T1,  &M6502::absy_2_5_T1,&M6502::sb_1_T1,&M6502::absy_4_4_T1,&M6502::absx_2_5_T1,&M6502::absx_2_5_T1,&M6502::absx_4_4_T1,&M6502::absx_4_4_T1,//3
&M6502::rti_5_5_T1,&M6502::idx_2_4_T1,&M6502::sb_jam_T1, &M6502::idx_Undoc_T1,&M6502::zp_2_1_T1, &M6502::zp_2_1_T1, &M6502::zp_4_1_T1, &M6502::zp_4_1_T1, &M6502::ph_5_1_T1,&M6502::imm_2_1_T1, &M6502::sb_1_T1,&M6502::imm_2_1_T1, &M6502::abs5_6_1_T1,&M6502::abs_2_3_T1, &M6502::abs_4_2_T1, &M6502::abs_4_2_T1, //4
&M6502::rel_5_8_T1,&M6502::idy_2_7_T1,&M6502::sb_jam_T1, &M6502::idy_Undoc_T1,&M6502::zpx_2_6_T1,&M6502::zpx_2_6_T1,&M6502::zpx_4_3_T1,&M6502::zpx_4_3_T1,&M6502::sb_1_T1,  &M6502::absy_2_5_T1,&M6502::sb_1_T1,&M6502::absy_4_4_T1,&M6502::absx_2_5_T1,&M6502::absx_2_5_T1,&M6502::absx_4_4_T1,&M6502::absx_4_4_T1,//5
&M6502::rts_5_7_T1,&M6502::idx_2_4_T1,&M6502::sb_jam_T1, &M6502::idx_Undoc_T1,&M6502::zp_2_1_T1, &M6502::zp_2_1_T1, &M6502::zp_4_1_T1, &M6502::zp_4_1_T1, &M6502::pl_5_2_T1,&M6502::imm_2_1_T1, &M6502::sb_1_T1,&M6502::imm_2_1_T1, &M6502::abs5_6_2_T1,&M6502::abs_2_3_T1, &M6502::abs_4_2_T1, &M6502::abs_4_2_T1, //6
&M6502::rel_5_8_T1,&M6502::idy_2_7_T1,&M6502::sb_jam_T1, &M6502::idy_Undoc_T1,&M6502::zpx_2_6_T1,&M6502::zpx_2_6_T1,&M6502::zpx_4_3_T1,&M6502::zpx_4_3_T1,&M6502::sb_1_T1,  &M6502::absy_2_5_T1,&M6502::sb_1_T1,&M6502::absy_4_4_T1,&M6502::absx_2_5_T1,&M6502::absx_2_5_T1,&M6502::absx_4_4_T1,&M6502::absx_4_4_T1,//7
&M6502::imm_2_1_T1,&M6502::idx_3_3_T1,&M6502::imm_2_1_T1,&M6502::idx_3_3_T1,  &M6502::zp_3_1_T1, &M6502::zp_3_1_T1, &M6502::zp_2_1_T1, &M6502::zp_3_1_T1, &M6502::sb_1_T1,  &M6502::imm_2_1_T1, &M6502::sb_1_T1,&M6502::imm_2_1_T1, &M6502::abs_3_2_T1, &M6502::abs_3_2_T1, &M6502::abs_3_2_T1, &M6502::abs_3_2_T1, //8
&M6502::rel_5_8_T1,&M6502::idy_3_6_T1,&M6502::sb_jam_T1, &M6502::idy_3_6_T1,  &M6502::zpx_3_5_T1,&M6502::zpx_3_5_T1,&M6502::zpy_3_5_T1,&M6502::zpy_3_5_T1,&M6502::sb_1_T1,  &M6502::absy_3_4_T1,&M6502::sb_1_T1,&M6502::absy_3_4_T1,&M6502::absx_3_4_T1,&M6502::absx_3_4_T1,&M6502::absy_3_4_T1,&M6502::absy_3_4_T1,//9
&M6502::imm_2_1_T1,&M6502::idx_2_4_T1,&M6502::imm_2_1_T1,&M6502::idx_2_4_T1,  &M6502::zp_2_1_T1, &M6502::zp_2_1_T1, &M6502::zp_2_1_T1, &M6502::zp_2_1_T1, &M6502::sb_1_T1,  &M6502::imm_2_1_T1, &M6502::sb_1_T1,&M6502::imm_2_1_T1, &M6502::abs_2_3_T1, &M6502::abs_2_3_T1, &M6502::abs_2_3_T1, &M6502::abs_2_3_T1, //A
&M6502::rel_5_8_T1,&M6502::idy_2_7_T1,&M6502::sb_jam_T1, &M6502::idy_2_7_T1,  &M6502::zpx_2_6_T1,&M6502::zpx_2_6_T1,&M6502::zpy_2_6_T1,&M6502::zpy_2_6_T1,&M6502::sb_1_T1,  &M6502::absy_2_5_T1,&M6502::sb_1_T1,&M6502::absy_4_4_T1,&M6502::absx_2_5_T1,&M6502::absx_2_5_T1,&M6502::absy_2_5_T1,&M6502::absy_2_5_T1,//B
&M6502::imm_2_1_T1,&M6502::idx_2_4_T1,&M6502::imm_2_1_T1,&M6502::idx_Undoc_T1,&M6502::zp_2_1_T1, &M6502::zp_2_1_T1, &M6502::zp_4_1_T1, &M6502::zp_4_1_T1, &M6502::sb_1_T1,  &M6502::imm_2_1_T1, &M6502::sb_1_T1,&M6502::imm_2_1_T1, &M6502::abs_2_3_T1, &M6502::abs_2_3_T1, &M6502::abs_4_2_T1, &M6502::abs_4_2_T1, //C
&M6502::rel_5_8_T1,&M6502::idy_2_7_T1,&M6502::sb_jam_T1, &M6502::idy_Undoc_T1,&M6502::zpx_2_6_T1,&M6502::zpx_2_6_T1,&M6502::zpx_4_3_T1,&M6502::zpx_4_3_T1,&M6502::sb_1_T1,  &M6502::absy_2_5_T1,&M6502::sb_1_T1,&M6502::absy_4_4_T1,&M6502::absx_2_5_T1,&M6502::absx_2_5_T1,&M6502::absx_4_4_T1,&M6502::absx_4_4_T1,//D
&M6502::imm_2_1_T1,&M6502::idx_2_4_T1,&M6502::imm_2_1_T1,&M6502::idx_Undoc_T1,&M6502::zp_2_1_T1, &M6502::zp_2_1_T1, &M6502::zp_4_1_T1, &M6502::zp_4_1_T1, &M6502::sb_1_T1,  &M6502::imm_2_1_T1, &M6502::sb_1_T1,&M6502::imm_2_1_T1, &M6502::abs_2_3_T1, &M6502::abs_2_3_T1, &M6502::abs_4_2_T1, &M6502::abs_4_2_T1, //E
&M6502::rel_5_8_T1,&M6502::idy_2_7_T1,&M6502::sb_jam_T1, &M6502::idy_Undoc_T1,&M6502::zpx_2_6_T1,&M6502::zpx_2_6_T1,&M6502::zpx_4_3_T1,&M6502::zpx_4_3_T1,&M6502::sb_1_T1,  &M6502::absy_2_5_T1,&M6502::sb_1_T1,&M6502::absy_4_4_T1,&M6502::absx_2_5_T1,&M6502::absx_2_5_T1,&M6502::absx_4_4_T1,&M6502::absx_4_4_T1 //F
};

void M6502::ADC(void)
{
	u16 result;

	result = a + value + (status & FLAG_CARRY);
	EstablishZ(result);

	if (status & FLAG_DECIMAL)
	{
		result = (a & 0xf) + (value & 0xf) + (status & FLAG_CARRY);
		if (result > 0x9) result += 0x6;
		if (result <= 0x0f) result = (result & 0xf) + (a & 0xf0) + (value & 0xf0);
		else result = (result & 0xf) + (a & 0xf0) + (value & 0xf0) + 0x10;
		EstablishV(result, value);
		EstablishN(result);
		if ((result & 0x1f0) > 0x90) result += 0x60;
		EstablishC(result);
	}
	else
	{
		EstablishC(result);
		EstablishV(result, value);
		EstablishN(result);
	}
	a = (u8)result;
}

void M6502::ARR(void)
{
	u16 result = a & value;
	u16 carry = status & FLAG_CARRY;

	if (status & FLAG_DECIMAL)
	{
		u16 resultDEC = result;
		resultDEC |= carry << 8;
		resultDEC >>= 1;
		SetN(carry);
		SetZ(resultDEC == 0);
		SetV((resultDEC ^ result) & 0x40);
		if (((result & 0xf) + (result & 0x1)) > 0x5) resultDEC = (resultDEC & 0xf0) | ((resultDEC + 0x6) & 0xf);
		if (((result & 0xf0) + (result & 0x10)) > 0x50)
		{
			resultDEC = (resultDEC & 0x0f) | ((resultDEC + 0x60) & 0xf0);
			SetC();
		}
		else ClearC();
		a = (u8)resultDEC;
	}
	else
	{
		result |= carry << 8;
		result >>= 1;
		EstablishNZ(result);

		u16 and40 = result & 0x40;
		SetC(and40 != 0);
		SetV(and40 ^ ((result & 0x20) << 1));
		a = (u8)result;
	}
}

void M6502::SBC(void)
{
	u16 result = a - value - ((status & FLAG_CARRY) ? 0 : 1);
	if (status & FLAG_DECIMAL)
	{
		u16 tmp_a;
		tmp_a = (a & 0xf) - (value & 0xf) - ((status & FLAG_CARRY) ? 0 : 1);
		if (tmp_a & 0x10) tmp_a = ((tmp_a - 6) & 0xf) | ((a & 0xf0) - (value & 0xf0) - 0x10);
		else tmp_a = (tmp_a & 0xf) | ((a & 0xf0) - (value & 0xf0));
		if (tmp_a & 0x100) tmp_a -= 0x60;
		SetC(result < 0x100);
		EstablishV(result, value ^ 0xff);
		EstablishNZ(result);
		a = (u8)tmp_a;
	}
	else 
	{
		EstablishNZ(result);
		SetC(result < 0x100);
		EstablishV(result, value ^ 0xff);
		a = (u8)result;
	}
}

void M6502::absx_2_5_T3(void)
{
	u16 startpage = ea & 0xFF00;
	ea += x;
	if (startpage != (ea & 0xFF00))
	{
		BUS_READ(startpage | (ea & 0xff));
		addressModeCycleFn = &M6502::absx_2_5_T4;
	}
	else
	{
		value = BUS_READ(ea);
		ExecuteOpcode();
	}
}

void M6502::absy_2_5_T3(void)
{
	u16 startpage = ea & 0xFF00;
	ea += y;
	if (startpage != (ea & 0xFF00))
	{
		BUS_READ(startpage | (ea & 0xff));
		addressModeCycleFn = &M6502::absy_2_5_T4;
	}
	else
	{
		value = BUS_READ(ea);
		ExecuteOpcode();
	}
}

void M6502::idy_2_7_T4(void)
{
	u16 startpage = ea & 0xFF00;
	ea += y;
	if (startpage != (ea & 0xFF00))
	{
		BUS_READ(startpage | (ea & 0xff));
		addressModeCycleFn = &M6502::idy_2_7_T5;
	}
	else
	{
		value = BUS_READ(ea);
		ExecuteOpcode();
	}
}

void M6502::rel_5_8_T2(void)
{
	BUS_READ(oldpc);
	pc = oldpc + ra;
	if ((oldpc & 0xFF00) == (pc & 0xFF00))
	{
		BranchTakenMaskingInterrupt = true;
		addressModeCycleFn = &M6502::InstructionFetch;	// Opcode has already been executed in T1 so just move on to the next instruction.
	}
	else
	{
		addressModeCycleFn = &M6502::rel_5_8_T3;
	}
}

// When executing a BRK and an interrupt condition is triggered between T0 and T4 the BRK morphs into the interrupt instruction.
// We check here if we continue on executing the BRK or morph and take the interrupt.
void M6502::brk_5_4_T4(void)
{
#ifdef  SUPPORT_NMI
	if (NMIPending)
	{
		NMIPending = 0;
		NMI_T4();
		return;
	}
#endif
#ifdef  SUPPORT_IRQ
	if (IRQPending && !IRQDisabled())
	{
		IRQPending = 0;
		IRQ_T4();
		return;
	}
#endif
	Push(status | FLAG_CONSTANT | FLAG_BREAK);
	addressModeCycleFn = &M6502::brk_5_4_T5;
}

// It is possible for a BRK/IRQ to mask a NMI for short burts of NMI assertions.
// If the NMI asserts and un-asserts between IRQ_T4 and IRQ_T6 this will occur.
// This occurs on real hardware and it will be emulated correctly.
// The processor has already commited to fetching the interrupt vectors and will now complete this process.
// If the NMI now un-asserts between now and the end of IRQ_T6 it will be missed/masked.
// The ability for a BRK/IRQ to turn into a NMI shows how the designers of the 6502 anticipated this masking and kept it to a minimum of only four 1/2 cycles!
// But then again, perhpas not, as a NMI that asserts after IRQ_T4 and remains asserted will not be processed until the first instruction of the IRQ routine has completed (see InstructionFetchIRQ).
void M6502::IRQ_T4(void)
{
#ifdef  SUPPORT_NMI
	if (NMIPending)
	{
		NMIPending = 0;
		NMI_T4();
		return;
	}
#endif
	ClearB();
	Push(status);
	addressModeCycleFn = &M6502::IRQ_T5;
}

// Interrupts are polled before starting a new instruction
// T0 of every address mode (except reset).
void M6502::InstructionFetch()
{
	opcode = BUS_READ(pc);	// Technically the InstructionFetch cycle T0 is part of the previous instruction's execution and the check for interrupts occurs after this fetch.

#ifdef  SUPPORT_NMI
	if (NMIPending)
		addressModeCycleFn = &M6502::NMI_T1;
	else
#endif //  SUPPORT_NMI
#ifdef  SUPPORT_IRQ
	if (IRQPending && !IRQDisabled())
	{
		IRQPending = 0;
		addressModeCycleFn = &M6502::IRQ_T1;
	}
	else
#endif //  SUPPORT_IRQ
	{
		pc++;
		addressModeCycleFn = T1AddressModeFunctions[opcode];
		opcodeCycleFn = opcodeFunctions[opcode];
	}
}

#ifdef  SUPPORT_IRQ
// If a NMI asserts too late during the IRQ execution ie after IRQ_T4 then it must wait one more instruction. So no polling is performed during this fetch.
// This is an idiosyncrasy of the real hardware and will be emulated using this fuction.
void M6502::InstructionFetchIRQ()
{
	opcode = BUS_READ(pc++);	// T0
	addressModeCycleFn = T1AddressModeFunctions[opcode];
	opcodeCycleFn = opcodeFunctions[opcode];
}
#endif

// A single step emulates both real 6502 1/2 cycles.
// On a real 6502, interrupts can be asserted between 1/2 cycles. When this occurs the hardware effectively ignores it for a further 1/2 cycle anyway.
// Here, interrupts are polled at the start of a cycle (in an instruction fetch cycle) emulating this behaviour.
// High frequency (1/2 cycle) bursts of interrupts assertions and un-assertions occuring mid full cycle will be missed by the hardware anyway.
void M6502::Step(void)
{
	bool irq;

	// If an IRQ occurs during a CLI then it will not take effect until the instruction after the CLI has been executed.
	// To emulate this, CLI simply sets CLIMaskingInterrupt flag.
	// Similar behaviour can be witnessed with the 3 cycle branch taken instruction.
#ifdef  SUPPORT_IRQ
	irq = IRQ.IsAsserted();
	if (irq && ((status & FLAG_INTERRUPT) == 0) && !CLIMaskingInterrupt && !BranchTakenMaskingInterrupt)
		IRQPending = 1;
	if (!irq)
		IRQPending = 0;
#endif //  SUPPORT_IRQ

#ifdef  SUPPORT_NMI
	NMIPending = NMI.IsAsserted() && !CLIMaskingInterrupt && !BranchTakenMaskingInterrupt;
#endif //  SUPPORT_NMI

	if (CLIMaskingInterrupt)	// If so we have delayed the IRQ long enough for the next instruction to now start. The CLI will then take effect after that instruction completes executing.
		CLIMaskingInterrupt = false;
	if (BranchTakenMaskingInterrupt)	// If so we have delayed the IRQ long enough for the next instruction to now start.
		BranchTakenMaskingInterrupt = false;

#ifdef  SUPPORT_RDY_HALTING
	if (!Halted())
	{
		CheckForHalt();
		(this->*M6502::addressModeCycleFn)();
	}
#else
	(this->*M6502::addressModeCycleFn)();
#endif //  SUPPORT_RDY_HALTING
}

void M6502::Reset(void)
{
	CLIMaskingInterrupt = false;
	BranchTakenMaskingInterrupt = false;
#ifdef  SUPPORT_IRQ
	IRQ.Reset();
	IRQPending = 0;
#endif //  SUPPORT_IRQ
#ifdef  SUPPORT_NMI
	NMI.Reset();
	NMIPending = 0;
#endif //  SUPPORT_NMI
#ifdef  SUPPORT_RDY_HALTING
	RDYCounter = 0;		// Don't know if the real hardware does this.
	RDYAsserted = 0;
	RDYHalted = 0;
#endif //  SUPPORT_RDY_HALTING
	Reset_T0();
}

#ifdef  SUPPORT_RDY_HALTING
void M6502::RDY(bool asserted)
{
	if (asserted && (RDYHalted == 0)) RDYCounter = 3;
	RDYAsserted = asserted;
	if (RDYHalted && !asserted) RDYHalted = 0;
}

void M6502::CheckForHalt()
{
	if ((RDYHalted == 0) && RDYAsserted && RDYCounter)
	{
		RDYCounter--;
		if (RDYCounter == 0) RDYHalted = 1;
	}
}

u8 M6502::BusRead(u16 address)
{
	if ((RDYHalted == 0) && RDYAsserted) RDYHalted = 1;
	return dataBusReadFn(address);
}
#endif //  SUPPORT_RDY_HALTING
