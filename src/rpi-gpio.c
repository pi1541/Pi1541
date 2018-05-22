#include <stdint.h>
#include "rpi-gpio.h"
#include "rpiHardware.h"
//#include "rpi-mailbox-interface.h"
#include "rpi-mailbox.h"

rpi_gpio_t* RPI_GpioBase = (rpi_gpio_t*) RPI_GPIO_BASE;

void RPI_SetGpioPinFunction(rpi_gpio_pin_t gpio, rpi_gpio_alt_function_t func)
{
  rpi_reg_rw_t* fsel_reg = &RPI_GpioBase->GPFSEL[gpio / 10];

  rpi_reg_rw_t fsel_copy = *fsel_reg;
  fsel_copy &= ~(FS_MASK << ((gpio % 10) * 3));
  fsel_copy |= (func << ((gpio % 10) * 3));
  *fsel_reg = fsel_copy;
}

void RPI_SetGpioOutput(rpi_gpio_pin_t gpio)
{
  RPI_SetGpioPinFunction(gpio, FS_OUTPUT);
}

void RPI_SetGpioInput(rpi_gpio_pin_t gpio)
{
  RPI_SetGpioPinFunction(gpio, FS_INPUT);
}

rpi_gpio_value_t RPI_GetGpioValue(rpi_gpio_pin_t gpio)
{
  rpi_gpio_value_t result = RPI_IO_UNKNOWN;

  switch (gpio / 32)
  {
    case 0:
      result = RPI_GpioBase->GPLEV0[0] & (1 << gpio);
    break;

    case 1:
      //result = RPI_GpioBase->GPLEV1 & (1 << (gpio - 32));
	  result = RPI_GpioBase->GPLEV0[1] & (1 << (gpio - 32));
	  break;

    default:
    break;
  }

  if (result != RPI_IO_UNKNOWN)
  {
    if (result)
      result = RPI_IO_HI;
  }

  return result;
}

void RPI_ToggleGpio(rpi_gpio_pin_t gpio)
{
  if (RPI_GetGpioValue(gpio))
    RPI_SetGpioLo(gpio);
  else
    RPI_SetGpioHi(gpio);
}

void RPI_SetGpioHi(rpi_gpio_pin_t gpio)
{
  switch (gpio / 32)
  {
    case 0:
      RPI_GpioBase->GPSET0[0] = (1 << gpio);
    break;

    case 1:
		//RPI_GpioBase->GPSET1 = (1 << (gpio - 32));
		RPI_GpioBase->GPSET0[1] = (1 << (gpio - 32));
    break;

    default:
    break;
  }

}

void RPI_SetGpioLo(rpi_gpio_pin_t gpio)
{
  switch (gpio / 32)
  {
    case 0:
      RPI_GpioBase->GPCLR0[0] = (1 << gpio);
    break;

    case 1:
		//RPI_GpioBase->GPCLR1 = (1 << (gpio - 32));
		RPI_GpioBase->GPCLR0[1] = (1 << (gpio - 32));
    break;

    default:
    break;
  }
}

void RPI_SetGpioValue(rpi_gpio_pin_t gpio, rpi_gpio_value_t value)
{
  if ((value == RPI_IO_LO) || (value == RPI_IO_OFF))
    RPI_SetGpioLo(gpio);
  else if ((value == RPI_IO_HI) || (value == RPI_IO_ON))
    RPI_SetGpioHi(gpio);
}

void EnableGpioDetect(rpi_gpio_pin_t gpio, unsigned type)
{
	unsigned mask = (1 << gpio);
	unsigned offset = gpio / 32;

	switch (type) {
	case ARM_GPIO_GPREN0:
		RPI_GpioBase->GPREN0[offset] |= mask;
		break;
	case ARM_GPIO_GPFEN0:
		RPI_GpioBase->GPFEN0[offset] |= mask;
		break;
	case ARM_GPIO_GPHEN0:
		RPI_GpioBase->GPHEN0[offset] |= mask;
		break;
	case ARM_GPIO_GPLEN0:
		RPI_GpioBase->GPLEN0[offset] |= mask;
		break;
	case ARM_GPIO_GPAREN0:
		RPI_GpioBase->GPAREN0[offset] |= mask;
		break;
	case ARM_GPIO_GPAFEN0:
		RPI_GpioBase->GPAFEN0[offset] |= mask;
		break;
	}
}

void DisableGpioDetect(rpi_gpio_pin_t gpio, unsigned type)
{
	unsigned mask = ~(1 << (gpio % 32));
	unsigned offset = gpio / 32;

	switch (type) {
	case ARM_GPIO_GPREN0:
		RPI_GpioBase->GPREN0[offset] &= mask;
		break;
	case ARM_GPIO_GPFEN0:
		RPI_GpioBase->GPFEN0[offset] &= mask;
		break;
	case ARM_GPIO_GPHEN0:
		RPI_GpioBase->GPHEN0[offset] &= mask;
		break;
	case ARM_GPIO_GPLEN0:
		RPI_GpioBase->GPLEN0[offset] &= mask;
		break;
	case ARM_GPIO_GPAREN0:
		RPI_GpioBase->GPAREN0[offset] &= mask;
		break;
	case ARM_GPIO_GPAFEN0:
		RPI_GpioBase->GPAFEN0[offset] &= mask;
		break;
	}
}

void ClearGpioEvent(rpi_gpio_pin_t gpio)
{
	unsigned mask = ~(1 << (gpio % 32));
	unsigned offset = gpio / 32;

	RPI_GpioBase->GPEDS0[offset] |= mask;
}

void SetACTLed(int value)
{
#if defined(RPI3)
	RPI_GpioVirtSetLed(value);
#endif
}

#if defined(RPI3)

#define MAXIMUM_SUPPORTED_POINTS 10

struct touch_regs
{
	uint8_t device_mode;
	uint8_t gesture_id;
	uint8_t num_points;
	struct touch {
		uint8_t xh;
		uint8_t xl;
		uint8_t yh;
		uint8_t yl;
		uint8_t res1;
		uint8_t res2;
	} point[MAXIMUM_SUPPORTED_POINTS];
};

static volatile uint32_t touchbuf;
//static uint32_t enables_disables[NUM_GPIO];

void RPI_TouchInit(void)
{
	rpi_mailbox_property_t* mp;

	RPI_PropertyInit();
	RPI_PropertyAddTag(TAG_GET_TOUCHBUF);
	RPI_PropertyProcess();

	if ((mp = RPI_PropertyGet(TAG_GET_TOUCHBUF)))
		touchbuf = mp->data.buffer_32[0] & ~0xC0000000;		// Bus to physical
	else
		touchbuf = 0;
}

void RPI_UpdateTouch(void)
{
	struct touch_regs* regs = (struct touch_regs*)touchbuf;
	int known_ids = 0;

	regs->num_points = MAXIMUM_SUPPORTED_POINTS + 1;

	if (!(regs->num_points == (MAXIMUM_SUPPORTED_POINTS + 1) || (regs->num_points == 0 && known_ids == 0)))
	{
		int i;
		int modified_ids = 0, released_ids;
		for (i = 0; i < regs->num_points; i++)
		{
			int x = (((int)regs->point[i].xh & 0xf) << 8) + regs->point[i].xl;
			int y = (((int)regs->point[i].yh & 0xf) << 8) + regs->point[i].yl;
			int touchid = (regs->point[i].yh >> 4) & 0xf;

			modified_ids |= 1 << touchid;

			if (!((1 << touchid) & known_ids))
				DEBUG_LOG("x = %d, y = %d, touchid = %d\n", x, y, touchid);

		}

		released_ids = known_ids & ~modified_ids;

		for (i = 0; released_ids && i < MAXIMUM_SUPPORTED_POINTS; i++)
		{
			if (released_ids & (1 << i))
			{
				DEBUG_LOG("Released %d, known = %x modified = %x\n", i, known_ids, modified_ids);
				modified_ids &= ~(1 << i);
			}
		}
		known_ids = modified_ids;

	}
}

#define NUM_GPIO 2

static volatile uint32_t gpiovirtbuf;
static uint32_t enables_disables[NUM_GPIO];

uint32_t RPI_GpioVirtGetAddress(void)
{
	return gpiovirtbuf;
}

void RPI_GpioVirtInit(void)
{
	//int i;

	//for (i = 0; i < NUM_GPIO; i++)
	//{
	//	enables_disables[i] = 0;
	//}

	rpi_mailbox_property_t* mp;

	RPI_PropertyInit();
	RPI_PropertyAddTag(TAG_GET_GPIOVIRTBUF);
	RPI_PropertyProcess();

	if ((mp = RPI_PropertyGet(TAG_GET_GPIOVIRTBUF)))
		gpiovirtbuf = mp->data.buffer_32[0] & ~0xC0000000;		// Bus to physical
	else
		gpiovirtbuf = 0;
}

// https://www.raspberrypi.org/forums/viewtopic.php?f=72&t=139753

// https://github.com/raspberrypi/firmware/blob/master/extra/dt-blob.dts ln 998
//128 - Bluetooth ON
//129 - WLan power
//130 - LEDS_DISK_ACTIVITY
//131 - LAN_RUN
//132 - HDMI_CONTROL_ATTACHED
//133 - camera power
//134 - camera LED
//135 - POWER_LOW
void RPI_GpioVirtSetLed(int val)
{
	//unsigned pin = 0;
	//if (gpiovirtbuf == 0)
	//	return;
	//DataMemBarrier();
	//if (val)
	//	write32(gpiovirtbuf + pin * 4, 1 <<16);
	//else
	//	write32(gpiovirtbuf + pin * 4, 1);

	uint16_t enables, disables;
	int16_t diff;
	int lit;
	unsigned pin = 0;

	//printf("gpiovirtbuf = %08x\r\n", (int)gpiovirtbuf);
	if (gpiovirtbuf == 0)
		return;

	enables = enables_disables[pin] >> 16;
	disables = enables_disables[pin] >> 0;

	diff = (int16_t)(enables - disables);
	lit = diff > 0;

	if (!(val ^ lit))
		return;

	if (val) enables++;
	else disables++;

	//printf("e = %d d = %d\r\n", enables, disables);

	enables_disables[pin] = (enables << 16) | (disables << 0);

	DataMemBarrier();

	write32(gpiovirtbuf + pin * 4, enables_disables[pin]);
}
#endif
