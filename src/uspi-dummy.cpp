#include <circle/startup.h>
#include <unistd.h>
#include "circle-kernel.h"
#include "rpi-gpio.h"

void USPiInitialize(void) {}
int USPiMassStorageDeviceAvailable(void) { return 0; }
int USPiKeyboardAvailable(void) { return 0; }
unsigned USPiMassStorageDeviceRead(void) { return 0 ; }
unsigned USPiMassStorageDeviceWrite(void) { return 0 ; }
void USPiKeyboardRegisterKeyStatusHandlerRaw(void) {}
void _data_memory_barrier(void) {} 
void _invalidate_dtlb_mva(void *x) {}
//void f_getlabel(char *x, char *l, int *) { *l = '\0'; }

void _enable_unaligned_access(void) {}
void enable_MMU_and_IDCaches(void) {}
void reboot_now(void) 
{
    reboot();
}

void TimerSystemInitialize() {};
void InterruptSystemInitialize() {};
void TimerCancelKernelTimer(unsigned hTimer) {};
unsigned TimerStartKernelTimer(
		unsigned nDelay,		// in HZ units
		void* pHandler,
		void* pParam,
		void* pContext) { return 0; };


int GetTemperature(unsigned &value) { return 0; };
void InitialiseLCD() {}
void UpdateLCD(const char* track, unsigned temperature) {}
rpi_gpio_t* RPI_GpioBase = (rpi_gpio_t*) RPI_GPIO_BASE;