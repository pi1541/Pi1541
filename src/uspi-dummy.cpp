#include <unistd.h>
#include "circle-kernel.h"
#include "rpi-gpio.h"

void _data_memory_barrier(void) {} 
void _invalidate_dtlb_mva(void *x) {}
void _enable_unaligned_access(void) {}
void enable_MMU_and_IDCaches(void) {}
void TimerSystemInitialize() {};
void InterruptSystemInitialize() {};
rpi_gpio_t* RPI_GpioBase = (rpi_gpio_t*) RPI_GPIO_BASE;