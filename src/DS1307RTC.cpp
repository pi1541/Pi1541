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

#include "DS1307RTC.h"
#include "debug.h"
#include <string.h>


/*
 * DS1307RTC.h - library for DS1307 RTC
  
  Copyright (c) Michael Margolis 2009
  This library is intended to be uses with Arduino Time library functions

  The library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
  
  30 Dec 2009 - Initial release
  5 Sep 2011 updated for Arduino 1.0
 */


#include "DS1307RTC.h"

#define DS1307_CTRL_ID 0x68 


DS1307RTC::DS1307RTC(int BSCMaster, u8 address, RTC_MODEL type)
	: BSCMaster(BSCMaster)
	, address(address)
	, type(type)
	, exists(false)
{
	RPI_I2CInit(BSCMaster, 1);
}

 
// PUBLIC FUNCTIONS
time_t DS1307RTC::get()   // Aquire data from buffer and convert to time_t
{
  tmElements_t tm;
  if (read(tm) == false) return 42;
  return(makeTime(tm));
}

bool DS1307RTC::set(time_t t)
{
  tmElements_t tm;
  breakTime(t, tm);
  return write(tm); 
}

// Aquire data from the RTC chip in BCD format
bool DS1307RTC::read(tmElements_t &tm)
{
  uint8_t sec;

  WireBeginTransmission(DS1307_CTRL_ID);
  WireWrite((uint8_t)0x00);;
  if (WireEndTransmission() != 0) {
    exists = false;
    return false;
  }
  exists = true;

  // request the 7 data fields   (secs, min, hr, dow, date, mth, yr)
#define tmNbrFields 7
  WireRequestFrom(DS1307_CTRL_ID, tmNbrFields);
  if (WireAvailable() < tmNbrFields) return false;

  sec = WireRead();
  tm.Second = bcd2dec(sec & 0x7f);   
  tm.Minute = bcd2dec(WireRead() );
  tm.Hour =   bcd2dec(WireRead() & 0x3f);  // mask assumes 24hr clock
  tm.Wday = bcd2dec(WireRead() );
  tm.Day = bcd2dec(WireRead() );
  tm.Month = bcd2dec(WireRead() );
  tm.Year = y2kYearToTm((bcd2dec(WireRead())));

  if (sec & 0x80) return false; // clock is halted
  return true;
}

bool DS1307RTC::write(tmElements_t &tm)
{
  // To eliminate any potential race conditions,
  // stop the clock before writing the values,
  // then restart it after.
  WireBeginTransmission(DS1307_CTRL_ID);
  WireWrite((uint8_t)0x00); // reset register pointer  
  WireWrite((uint8_t)0x80); // Stop the clock. The seconds will be written last
  WireWrite(dec2bcd(tm.Minute));
  WireWrite(dec2bcd(tm.Hour));      // sets 24 hour format
  WireWrite(dec2bcd(tm.Wday));   
  WireWrite(dec2bcd(tm.Day));
  WireWrite(dec2bcd(tm.Month));
  WireWrite(dec2bcd(tmYearToY2k(tm.Year))); 

  if (WireEndTransmission() != 0) {
    exists = false;
    return false;
  }
  exists = true;

  // Now go back and set the seconds, starting the clock back up as a side effect
  WireBeginTransmission(DS1307_CTRL_ID);
  WireWrite((uint8_t)0x00); // reset register pointer  
  WireWrite(dec2bcd(tm.Second)); // write the seconds, with the stop bit clear to restart
  if (WireEndTransmission() != 0) {
    exists = false;
    return false;
  }
  exists = true;
  return true;
}

unsigned char DS1307RTC::isRunning()
{
  WireBeginTransmission(DS1307_CTRL_ID);

  WireWrite((uint8_t)0x00); 

  WireEndTransmission();

  // Just fetch the seconds register and check the top bit
  WireRequestFrom(DS1307_CTRL_ID, 1);

  return !(WireRead() & 0x80);
}

void DS1307RTC::setCalibration(char calValue)
{
  unsigned char calReg = abs(calValue) & 0x1f;
  if (calValue >= 0) calReg |= 0x20; // S bit is positive to speed up the clock
  WireBeginTransmission(DS1307_CTRL_ID);

  WireWrite((uint8_t)0x07); // Point to calibration register
  WireWrite(calReg);

  WireEndTransmission();  
}

char DS1307RTC::getCalibration()
{
  WireBeginTransmission(DS1307_CTRL_ID);
  WireWrite((uint8_t)0x07); 
  WireEndTransmission();

  WireRequestFrom(DS1307_CTRL_ID, 1);
  unsigned char calReg = WireRead();

  char out = calReg & 0x1f;
  if (!(calReg & 0x20)) out = -out; // S bit clear means a negative value
  return out;
}

// PRIVATE FUNCTIONS

// Convert Decimal to Binary Coded Decimal (BCD)
uint8_t DS1307RTC::dec2bcd(uint8_t num)
{
  return ((num/10 * 16) + (num % 10));
}

// Convert Binary Coded Decimal (BCD) to Decimal
uint8_t DS1307RTC::bcd2dec(uint8_t num)
{
  return ((num/16 * 10) + (num % 16));
}

//bool DS1307RTC::exists = false;

//DS1307RTC RTC = DS1307RTC(); // create an instance for the user

void DS1307RTC::WireBeginTransmission(int new_address)
{
	if (new_address)
		address = new_address;
	I2Cbuffer_ptr = 0;
	I2Cbuffer_len = 0;
}

void DS1307RTC::WireWrite(u8 data)
{
	if (I2Cbuffer_ptr < 256-1)
		I2Cbuffer[I2Cbuffer_ptr++] = data;
}

int DS1307RTC::WireEndTransmission(void)
{
	int count = RPI_I2CWrite(BSCMaster, address, I2Cbuffer, I2Cbuffer_ptr);
	if (count)
		return 0;
	else
		return 1;
}

int DS1307RTC::WireRequestFrom(int new_address, int num_bytes)
{
	if (new_address)
		address = new_address;

	if (num_bytes < 256)
	{
		I2Cbuffer_ptr = 0;
		I2Cbuffer_len = RPI_I2CRead(BSCMaster, address, I2Cbuffer, num_bytes);
		return I2Cbuffer_len;
	}
	else
	{
		I2Cbuffer_ptr = 0;
		I2Cbuffer_len = 0;
		return 0;
	}
}

int DS1307RTC::WireAvailable(void)
{
	return (I2Cbuffer_len - I2Cbuffer_ptr);
}

u8 DS1307RTC::WireRead(void)
{
	return I2Cbuffer[I2Cbuffer_ptr++];
}

/*
  time.c - low level time and date functions
  Copyright (c) Michael Margolis 2009-2014

*/

#define LEAP_YEAR(Y)     ( ((1970+(Y))>0) && !((1970+(Y))%4) && ( ((1970+(Y))%100) || !((1970+(Y))%400) ) )

static  const uint8_t monthDays[]={31,28,31,30,31,30,31,31,30,31,30,31}; // API starts months from 1, this array starts from 0

void DS1307RTC::breakTime(time_t timeInput, tmElements_t &tm){
// break the given time_t into time components
// this is a more compact version of the C library localtime function
// note that year is offset from 1970 !!!

  uint8_t year;
  uint8_t month, monthLength;
  uint32_t time;
  unsigned long days;

  time = (uint32_t)timeInput;
  tm.Second = time % 60;
  time /= 60; // now it is minutes
  tm.Minute = time % 60;
  time /= 60; // now it is hours
  tm.Hour = time % 24;
  time /= 24; // now it is days
  tm.Wday = ((time + 4) % 7) + 1;  // Sunday is day 1

  year = 0;
  days = 0;
  while((unsigned)(days += (LEAP_YEAR(year) ? 366 : 365)) <= time) {
    year++;
  }
  tm.Year = year; // year is offset from 1970

  days -= LEAP_YEAR(year) ? 366 : 365;
  time  -= days; // now it is days in this year, starting at 0

  days=0;
  month=0;
  monthLength=0;
  for (month=0; month<12; month++) {
    if (month==1) { // february
      if (LEAP_YEAR(year)) {
        monthLength=29;
      } else {
        monthLength=28;
      }
    } else {
      monthLength = monthDays[month];
    }

    if (time >= monthLength) {
      time -= monthLength;
    } else {
        break;
    }
  }
  tm.Month = month + 1;  // jan is month 1
  tm.Day = time + 1;     // day of month
}


time_t DS1307RTC::makeTime(const tmElements_t &tm){
// assemble time elements into time_t
// note year argument is offset from 1970 (see macros in time.h to convert to other formats)
// previous version used full four digit year (or digits since 2000),i.e. 2009 was 2009 or 9

  int i;
  uint32_t seconds;

  // seconds from 1970 till 1 jan 00:00:00 of the given year
  seconds= tm.Year*(SECS_PER_DAY * 365);
  for (i = 0; i < tm.Year; i++) {
    if (LEAP_YEAR(i)) {
      seconds +=  SECS_PER_DAY;   // add extra days for leap years
    }
  }

  // add days for this year, months start from 1
  for (i = 1; i < tm.Month; i++) {
    if ( (i == 2) && LEAP_YEAR(tm.Year)) {
      seconds += SECS_PER_DAY * 29;
    } else {
      seconds += SECS_PER_DAY * monthDays[i-1];  //monthDay array starts from 0
    }
  }
  seconds+= (tm.Day-1) * SECS_PER_DAY;
  seconds+= tm.Hour * SECS_PER_HOUR;
  seconds+= tm.Minute * SECS_PER_MIN;
  seconds+= tm.Second;
  return (time_t)seconds;
}
