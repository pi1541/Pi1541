#include <circle/startup.h>
#include <unistd.h>

extern "C" {
void USPiInitialize(void) {}
int USPiMassStorageDeviceAvailable(void) { return 0; }
int USPiKeyboardAvailable(void) { return 0; }
unsigned USPiMassStorageDeviceRead(void) { return 0 ; }
unsigned USPiMassStorageDeviceWrite(void) { return 0 ; }
void USPiKeyboardRegisterKeyStatusHandlerRaw(void) {}
void _data_memory_barrier(void) {} 
void _invalidate_dtlb_mva(void *x) {}
void f_getlabel(char *x, char *l, int *) { *l = '\0'; }
void _enable_unaligned_access(void) {}
void enable_MMU_and_IDCaches(void) {};
void reboot_now(void) 
{
    reboot();
}

void usDelay(unsigned nMicroSeconds) 
{
//    usleep(nMicroSeconds);
}
void MsDelay(unsigned ms)
{
//    usleep(1000 * ms);
}
void TimerSystemInitialize() {};
void TimerCancelKernelTimer(unsigned hTimer) {};
unsigned TimerStartKernelTimer(
		unsigned nDelay,		// in HZ units
		void* pHandler,
		void* pParam,
		void* pContext) { return 0; };

}

int GetTemperature(unsigned &value) { return 0; };
