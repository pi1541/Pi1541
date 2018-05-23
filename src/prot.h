/* prot.h */
void shift_buffer(BYTE * buffer, int length, int n);
BYTE *align_vmax(BYTE * work_buffer, int track_len);
BYTE *auto_gap(BYTE * work_buffer, int track_len);
BYTE *find_weak_gap(BYTE * work_buffer, int tracklen);
BYTE *find_long_sync(BYTE * work_buffer, int tracklen);
BYTE *auto_gap(BYTE * work_buffer, int tracklen);

