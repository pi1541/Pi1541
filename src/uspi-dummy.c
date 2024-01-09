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
