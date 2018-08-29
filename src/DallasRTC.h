/*
 * DallasRTC.h - library for DS1307 style RTC
 * This library is intended to be uses with Arduino Time library functions
 */

#ifndef DallasRTC_h
#define DallasRTC_h

#include <cmath>
#include <sys/types.h>
#include "types.h"
extern "C" {
#include "rpi-i2c.h"
}

typedef struct  {
  uint8_t Second;
  uint8_t Minute;
  uint8_t Hour;
  uint8_t Wday;   // day of week, sunday is day 1
  uint8_t Day;
  uint8_t Month;
  uint8_t Year;   // offset from 1970;
}       tmElements_t, TimeElements, *tmElementsPtr_t;

#define  tmYearToY2k(Y)      ((Y) - 30)    // offset is from 2000
#define  y2kYearToTm(Y)      ((Y) + 30)
/* Useful Constants */
#define SECS_PER_MIN  ((time_t)(60UL))
#define SECS_PER_HOUR ((time_t)(3600UL))
#define SECS_PER_DAY  ((time_t)(SECS_PER_HOUR * 24UL))
#define DAYS_PER_WEEK ((time_t)(7UL))
#define SECS_PER_WEEK ((time_t)(SECS_PER_DAY * DAYS_PER_WEEK))
#define SECS_PER_YEAR ((time_t)(SECS_PER_DAY * 365UL)) // TODO: ought to handle leap years
#define SECS_YR_2000  ((time_t)(946684800UL)) // the time at the start of y2k


// library interface description
class DallasRTC
{
  // user-accessible "public" interface
  public:
    DallasRTC(int BSCMaster = 1, u8 address = 0x68, RTC_MODEL type = RTC_UNKNOWN);
    time_t get();
    bool set(time_t t);
    bool read(tmElements_t &tm);
    bool write(tmElements_t &tm);
    bool chipPresent() { return exists; }
    unsigned char isRunning();
    void setCalibration(char calValue);
    char getCalibration();

// Convert Decimal to Binary Coded Decimal (BCD)
static uint8_t dec2bcd(uint8_t num)
{
  return ((num/10 * 16) + (num % 10));
}

static uint8_t bcd2dec(uint8_t num)
{
  return ((num/16 * 10) + (num % 16));
}


  protected:
    int BSCMaster;
    u8 address;
    RTC_MODEL type;

  private:
    bool exists;

    time_t makeTime(const tmElements_t &tm);
    void breakTime(time_t timeInput, tmElements_t &tm);
    char I2Cbuffer[256];
    int I2Cbuffer_ptr = 0;
    int I2Cbuffer_len = 0;


};

//extern DallasRTC RTC;

#endif
 
