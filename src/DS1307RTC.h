/*
 * DS1307RTC.h - library for DS1307 RTC
 * This library is intended to be uses with Arduino Time library functions
 */

#ifndef DS1307RTC_h
#define DS1307RTC_h

#include <cmath>
#include "TimeLib.h"
#include "types.h"
extern "C" {
#include "rpi-i2c.h"
}

// library interface description
class DS1307RTC
{
  // user-accessible "public" interface
  public:
    DS1307RTC(int BSCMaster = 1, u8 address = 0x68, RTC_MODEL type = RTC_UNKNOWN);
    time_t get();
    bool set(time_t t);
    bool read(tmElements_t &tm);
    bool write(tmElements_t &tm);
    bool chipPresent() { return exists; }
    unsigned char isRunning();
    void setCalibration(char calValue);
    char getCalibration();

  protected:
    int BSCMaster;
    u8 address;
    RTC_MODEL type;

  private:
    bool exists;
    bool happy;
    uint8_t dec2bcd(uint8_t num);
    uint8_t bcd2dec(uint8_t num);
    int WireWrite(u8 command);
    u8 WireRead(void);

};

#ifdef RTC
#undef RTC // workaround for Arduino Due, which defines "RTC"...
#endif

//extern DS1307RTC RTC;

#endif
 
