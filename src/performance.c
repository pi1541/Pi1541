// Part of PiTubeDirect
// https://github.com/hoglet67/PiTubeDirect
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "startup.h"
#include "performance.h"

#if defined(RPI3)

const char * type_names[] = {

   "SW_INCR",
   "L1I_CACHE_REFILL",
   "L1I_TLB_REFILL",
   "L1D_CACHE_REFILL",
   "L1D_CACHE",
   "L1D_TLB_REFILL",
   "LD_RETIRED",
   "ST_RETIRED",
   "INST_RETIRED",
   "EXC_TAKEN",
   "EXC_RETURN",
   "CID_WRITE_RETIRED",
   "PC_WRITE_RETIRED",
   "BR_IMM_RETIRED",
   "BR_RETURN_RETIRED",
   "UNALIGNED_LDST_RETIRED",
   "BR_MIS_PRED",
   "CPU_CYCLES",
   "BR_PRED",
   "MEM_ACCESS",
   "L1I_CACHE",
   "L1D_CACHE_WB",
   "L2D_CACHE",
   "L2D_CACHE_REFILL",
   "L2D_CACHE_WB",
   "BUS_ACCESS",
   "MEMORY_ERROR",
   "INST_SPEC",
   "TTRB_WRITE_RETIRED",
   "BUS_CYCLES",
   "CHAIN",
   "L1D_CACHE_ALLOCATE"
};

#elif defined(RPI2)

const char * type_names[] = {
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO",
   "TODO"
};

#else

const char * type_names[] = {
	"I_CACHE_MISS",				
	"IBUF_STALL",				
	"DATA_DEP_STALL",			
	"I_MICROTLB_MISS",			
	"D_MICROTLB_MISS",			
	"BRANCH_EXECUTED",			
	"BRANCH_PRED_INCORRECT",	
	"INSTRUCTION_EXECUTED",		
	"UNDEFINED",
	"D_CACHE_ACCESS_CACHEABLE",	
	"D_CACHE_ACCESS",			
	"D_CACHE_MISS",				
	"D_CACHE_WRITEBACK",		
	"SOFTWARE_CHANGED_PC",		
	"UNDEFINED",
	"MAINTLB_MISS",				
	"EXPLICIT_DATA_ACCESS",		
	"FULL_LOAD_STORE_REQ_QUEUE",
	"WRITE_BUFF_DRAINED",		
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"EXT0",						
	"EXT1",						
	"EXT0_AND_EXT1",			
	"PROC_RETURN_PUSHED",		
	"PROC_RETURN_POPPED",		
	"PROC_RETURN_PRED_CORRECT",	
	"PROC_RETURN_PRED_INCORRECT",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
	"UNDEFINED",
};

#endif

const char *type_lookup(int type) {
	static const char *UNKNOWN = "UNKNOWN";
	int num_types = sizeof(type_names) / sizeof(type_names[0]);
	if (type >= 0 || type < num_types) {
		return type_names[type];
	}
	else {
		return UNKNOWN;
	}
}


// Set control register and zero counters
void reset_performance_counters(perf_counters_t *pct) {
	// bit 3 = 1 means count every 64th processor cycle
	// bit 2 = 1 means reset cycle counter to zero
	// bit 1 = 1 means reset counters to zero
	// bit 0 = 1 enable counters
	unsigned ctrl = 0x0F;


#if defined(RPI2) || defined(RPI3)
	int i;
	unsigned cntenset = (1 << 31);

	unsigned type_impl;

	// Read the common event identification register to see test whether the requested event is implemented
	asm volatile ("mrc p15,0,%0,c9,c12,6" : "=r" (type_impl));

	for (i = 0; i < pct->num_counters; i++) {
		if ((type_impl >> pct->type[i]) & 1) {
			// Select the event count/type via the event type selection register
			asm volatile ("mcr p15,0,%0,c9,c12,5" :: "r" (i) : "memory");
			// Configure the required event type
			asm volatile ("mcr p15,0,%0,c9,c13,1" :: "r" (pct->type[i]) : "memory");
			// Set the bit to enable the counter
			cntenset |= (1 << i);
		}
		else {
			printf("Event: %s not implemented\r\n", type_lookup(pct->type[i]));
		}
	}
	// Write the control register
	asm volatile ("mcr p15,0,%0,c9,c12,0" :: "r" (ctrl) : "memory");

	// Enable the counters
	asm volatile ("mcr p15,0,%0,c9,c12,1" :: "r" (cntenset) : "memory");
#else
	// Only two counters (0 and 1) are supported on the arm11
	ctrl |= (pct->type[0] << 20);
	ctrl |= (pct->type[1] << 12);
	asm volatile ("mcr p15,0,%0,c15,c12,0" :: "r" (ctrl) : "memory");
#endif
}

void read_performance_counters(perf_counters_t *pct) {
#if defined(RPI2) || defined(RPI3)
	int i;
	for (i = 0; i < pct->num_counters; i++) {
		// Select the event count/type via the event type selection register
		asm volatile ("mcr p15,0,%0,c9,c12,5" :: "r" (i) : "memory");
		// Read the required event count
		asm volatile ("mrc p15,0,%0,c9,c13,2" : "=r" (pct->counter[i]));
	}
	asm volatile ("mrc p15,0,%0,c9,c13,0" : "=r" (pct->cycle_counter));
#else
	// Only two counters (0 and 1) are supported on the arm11
	asm volatile ("mrc p15,0,%0,c15,c12,2" : "=r" (pct->counter[0]));
	asm volatile ("mrc p15,0,%0,c15,c12,3" : "=r" (pct->counter[1]));
	asm volatile ("mrc p15,0,%0,c15,c12,1" : "=r" (pct->cycle_counter));
#endif
}

void print_performance_counters(perf_counters_t *pct) {
	int i;
	uint64_t cycle_counter = pct->cycle_counter;
	cycle_counter *= 64;
	printf("%26s = %"PRIu64"\r\n", "cycle counter", cycle_counter);
	for (i = 0; i < pct->num_counters; i++) {
		printf("%26s = %u\r\n", type_lookup(pct->type[i]), pct->counter[i]);
	}
}
/*
int benchmark() {
	int i;
	int total;
	int size;
	perf_counters_t pct;
	unsigned char mem1[1024 * 1024];
	unsigned char mem2[1024 * 1024];

#if defined(RPI2) || defined(RPI3) 
	pct.num_counters = 6;
	pct.type[0] = PERF_TYPE_L1I_CACHE;
	pct.type[1] = PERF_TYPE_L1I_CACHE_REFILL;
	pct.type[2] = PERF_TYPE_L1D_CACHE;
	pct.type[3] = PERF_TYPE_L1D_CACHE_REFILL;
	pct.type[4] = PERF_TYPE_L2D_CACHE_REFILL;
	pct.type[5] = PERF_TYPE_INST_RETIRED;
	pct.counter[0] = 100;
	pct.counter[1] = 101;
	pct.counter[2] = 102;
	pct.counter[3] = 103;
	pct.counter[4] = 104;
	pct.counter[5] = 105;
#else
	pct.num_counters = 2;
	pct.type[0] = PERF_TYPE_I_CACHE_MISS;
	pct.type[1] = PERF_TYPE_D_CACHE_MISS;
#endif

	printf("benchmarking core....\r\n");
	reset_performance_counters(&pct);
	// These only work on Pi 1
	//_invalidate_icache();
	//_invalidate_dcache();
	total = 0;
	for (i = 0; i < 1000000; i++) {
		if ((i & 3) == 0) {
			total += i;
		}
		else {
			total -= i;
		}
	}
	read_performance_counters(&pct);
	print_performance_counters(&pct);

#ifdef HAS_40PINS
	printf("benchmarking io toggling....\r\n");
	reset_performance_counters(&pct);
	_toggle_test_pin(1000000);
	read_performance_counters(&pct);
	print_performance_counters(&pct);
#endif

	for (i = 0; i <= 10; i++) {
		size = 1 << i;
		printf("benchmarking %dKB memory copy....\r\n", size);
		size *= 1024;
		reset_performance_counters(&pct);
		// These only work on Pi 1
		//_invalidate_icache();
		//_invalidate_dcache();
		memcpy(mem1, mem2, size);
		read_performance_counters(&pct);
		print_performance_counters(&pct);
	}

	return total;
}
*/
