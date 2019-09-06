/*
 *  dmRrotary.h  - A simple decoder to use a KY-040 rotary encoder for browse
 *                 naviation in Pi1541.
 * 
 *  Copyright © 2019 devMash.com
 * 
 *  https://devMash.com
 *  https://github.com/devMashHub
 * 
 *  Permission is hereby granted, free of charge, to any person obtaining a copy of this
 *  software and associated documentation files (the “Software”), to deal in the Software
 *  without restriction, including without limitation the rights to use, copy, modify, merge,
 *  publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons
 *  to whom the Software is furnished to do so, subject to the following conditions:
 *  
 *  The above copyright notice and this permission notice shall be included in all copies or
 *  substantial portions of the Software.
 *  
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE
 * FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 * 
 */

#ifndef DM_ROTARY_H
#define DM_ROTARY_H

extern "C"
{
    #include "rpi-aux.h"
    #include "rpiHardware.h"
}

//Enable debugging messages on the mini uart
//#define DM_ROTARY_DEBUG


//------------------------------------------------------------------------------
// rotary_result_t
//
typedef enum {
    NoChange = 0,
    ButtonDown = 1,
    ButtonUp = 2,
    RotatePositive = 3,     // clockwise rotation
    RotateNegative = 4      // counter-clockwise rotation
} rotary_result_t;


//------------------------------------------------------------------------------
// RotaryPin
//
// NOTES:
//
// This helper class represents a single rotary encoder pin to encapsulate
// debouncing and state engine logic.
//
class RotaryPin 
{

private:

    rpi_gpio_pin_t _gpioPin = RPI_GPIO0;

    int _count = 0;
    int _threshold = 256; // I just like powers of two

    bool _currentState = false;

public:

    rpi_gpio_pin_t GetGpioPin() const { return _gpioPin; }  
    void SetGpioPin(rpi_gpio_pin_t value) { _gpioPin = value; }

    unsigned GetGpioPinMask() { return (1 << _gpioPin); }

    bool GetState() const { return _currentState; }

    void Update(bool state)
    {

		_count += state ? 1 : -1;

        bool newState = _currentState;

        if (_count <= 0)
        {
            _count = 0;
            newState = false;
        }
        else if (_count >= _threshold)
        {
            _count = _threshold;
            newState = true;
        }

        _currentState = newState;

    }

};


//------------------------------------------------------------------------------
// RotaryEncoder
//
//  NOTES:
//
//  The KY-040 rotary encoder has the following pins:
//
//     CLK  -  Encoder pin A
//     DT   -  Encoder pin B
//     SW   -  Pushbutton switch
//     VCC  -  Supply voltage
//     GND  -  Ground
//
//
//  Decoding the KY-040 
//
//  Determining which encoder pin changes state first is how the direction
//	of rotation is determined.  If CLK changes state first, the encoder
//	is rotating in a positive (clockwise) direction.  If DT changes state
//  first, the encoder is rotating in a negative (counter-clockwise) direction.
//
//  The entire rotation sequence can be represented as a predictable bit
//  pattern by mapping the CLK state in the higher bit and mapping the DT
//  state in the lower bit:
//
//     Positive rotation  -  00 -> 10 -> 11 -> 01 -> 00   (0xb4)
//     Negative rotation  -  00 -> 01 -> 11 -> 10 -> 00   (0x78)
//
//  To decode, we simply monitor the CLK and DT and watch for one of these
//  two patterns.
//
//  However, in the real world, some pin transitions can be missed due to
//  lag during the polling interval or contact bounce - resulting in less
//  than perfect decode sequences.  To combat against this, the code will
//  also accept several permutations that are 'close enough':
//  
//     Positive (clockwise) rotation:							
//
//        0xb4 - 00 10 11 01 00 - Received and decoded perfect sequence
//	      0x2c -    00 10 11 00 - Missed data but decoded unique sequence
//	      0x34 -    00 11 01 00 - Missed data but decoded unique sequence
//        0xb8 - 00 10 11 10 00 - Invalid but usually denotes positive
//
//     Detect negative (counter-clockwise) rotation:
//
//	      0x78 - 00 01 11 10 00 - Received and decoded perfect sequence
//	      0x1c -    00 01 11 00 - Missed data but decoded unique sequence
//	      0x38 -    00 11 10 00 - Missed data but decoded unique sequence
//        0x74 - 00 01 11 01 00 - Invalid but usually denotes negative
//
//
//  Wiring the KY-040
//
//  The GPIO pins used for the rotary encoder are specified when initializing
//  the class.  However, it is probably a good idea to reuse the same pins as
//  the original Pi1541 pushbuttons:
//
//     GPIO 22	-  Menu up       -  Encoder pin A (CLK)
//     GPIO 23	-  Menu down     -  Encoder pin B (DT)
//     GPIO 27  -  Enter/Select  -  Encoder pushbutton (SW)
//
//
//  USAGE:
//
//  *** Please Note!  I have only tried this with a Raspberry Pi 3.  I have no
//  reason to believe this wouldn't also work on an earlier model, but your
//  results may vary!
//
//  To use the RotaryEncoder, instance the class and initialize with the desired
//  GPIO pins like this:
//
//     //Initialize using CLK on GPIO22, DT on GPIO32 and SW on GPIO27  
//     RotaryEncoder rotaryEncoder;
//     rotaryEncoder.Initialize(RPI_GPIO22, RPI_GPIO23, RPI_GPIO27);
//
//  Monitor the encoder by calling the Poll() method during your main processing
//  loop.  The polling logic is constrained by only evaluating GPLEV0, which
//  restricts usable pins to GPIO00 through GPIO31.  An overloaded version of
//  Poll() exists to accept a value representing GPLEV0 without having to
//  perform a re-read (providing a small optimization when the current value of
//  GPLEV0 is already available).
//
//      // Read GPLEV0 and decode the rotary state
//      rotary_result_t result = rotaryEncoder.Poll();
//
//      or
//
//      // Read GPLEV0 locally and decode the rotary state
//      unsigned gplev0 = read32(ARM_GPIO_GPLEV0);
//      { some other logic here }
//      rotary_result_t result = rotaryEncoder.Poll(gplev0);
//
//  Note, the Poll() logic depends on frequent polling of the encoder.  Calling
//  Poll() as often as possible/permissable will yield better decode results.
//
//  Any event detected by the polling logic will be returned as the result
//  from the polling method as a rotary_result_t.  The controlling logic can
//  then take whatever action is appropriate based on the result.
//
//
//  HISTORY:
//
//  09/03/2019 - Initial implementation and notes
//  09/04/2019 - Integration into Pi1541 with shim logic to simulate 'original style' button presses
//  09/05/2019 - Code cleanup, improved documentation, options.txt logic, dynamic button indexes
//
class RotaryEncoder {

private:

    // Switch data

    RotaryPin _switchPin;

    bool _currentSwitchState = false;

    // Rotation data

    RotaryPin _clockPin;
    RotaryPin _dataPin;

    int _currentRotaryState = 0;
    int _currentRotarySequence = 0;

    // Private methods

    void SetGpioPullUpDown(unsigned controlSignal, unsigned gpioPinMask);

    #ifdef DM_ROTARY_DEBUG    
        void WriteToMiniUart(char* pMessage);
    #endif

public:

    // Public methods

    void Initialize(rpi_gpio_pin_t clkGpioPin, rpi_gpio_pin_t dtGpioPin, rpi_gpio_pin_t swGpioPin);

    rotary_result_t Poll();
    rotary_result_t Poll(unsigned gplev0);

};

#endif