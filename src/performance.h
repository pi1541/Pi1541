// Part of PiTubeDirect
// https://github.com/hoglet67/PiTubeDirect
// performance.h

#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#if defined(RPI3) ||  defined(RPI2)

// TODO - More work is needed on the RPI2 performance metrics

#define MAX_COUNTERS 6

#define PERF_TYPE_SW_INCR                    0x00
#define PERF_TYPE_L1I_CACHE_REFILL           0x01
#define PERF_TYPE_L1I_TLB_REFILL             0x02
#define PERF_TYPE_L1D_CACHE_REFILL           0x03
#define PERF_TYPE_L1D_CACHE                  0x04
#define PERF_TYPE_L1D_TLB_REFILL             0x05
#define PERF_TYPE_LD_RETIRED                 0x06
#define PERF_TYPE_ST_RETIRED                 0x07
#define PERF_TYPE_INST_RETIRED               0x08
#define PERF_TYPE_EXC_TAKEN                  0x09
#define PERF_TYPE_EXC_RETURN                 0x0A
#define PERF_TYPE_CID_WRITE_RETIRED          0x0B
#define PERF_TYPE_PC_WRITE_RETIRED           0x0C
#define PERF_TYPE_BR_IMM_RETIRED             0x0D
#define PERF_TYPE_BR_RETURN_RETIRED          0x0E
#define PERF_TYPE_UNALIGNED_LDST_RETIRED     0x0F
#define PERF_TYPE_BR_MIS_PRED                0x10
#define PERF_TYPE_CPU_CYCLES                 0x11
#define PERF_TYPE_BR_PRED                    0x12
#define PERF_TYPE_MEM_ACCESS                 0x13
#define PERF_TYPE_L1I_CACHE                  0x14
#define PERF_TYPE_L1D_CACHE_WB               0x15
#define PERF_TYPE_L2D_CACHE                  0x16
#define PERF_TYPE_L2D_CACHE_REFILL           0x17
#define PERF_TYPE_L2D_CACHE_WB               0x18
#define PERF_TYPE_BUS_ACCESS                 0x19
#define PERF_TYPE_MEMORY_ERROR               0x1A
#define PERF_TYPE_INST_SPEC                  0x1B
#define PERF_TYPE_TTRB_WRITE_RETIRED         0x1C
#define PERF_TYPE_BUS_CYCLES                 0x1D
#define PERF_TYPE_CHAIN                      0x1E
#define PERF_TYPE_L1D_CACHE_ALLOCATE         0x1F

#else

// See http://infocenter.arm.com/help/index.jsp?topic=/com.arm.doc.dai0195b/index.html
// Read carefully the definitions of the ARM1176 performance events. The “data cache access” events, in particular, only count nonsequential data cache accesses. This important qualification affects the interpretation of performance measurements. In particular, you can’t compute a pure data cache miss ratio, that is, all data cache misses divided by all data cache accesses.
#define MAX_COUNTERS 2

#define PERF_TYPE_I_CACHE_MISS               0x00	// Instruction cache miss. Instruction cache miss to a cacheable location, which requires a fetch from external memory
#define PERF_TYPE_IBUF_STALL                 0x01	// Stall because instruction buffer cannot deliver an instruction. This could indicate an Instruction Cache miss or an Instruction MicroTLB miss. This event occurs every cycle in which the condition is present.
#define PERF_TYPE_DATA_DEP_STALL             0x02	// Stall because of a data dependency. This event occurs every cycle in which the condition is present.
#define PERF_TYPE_I_MICROTLB_MISS            0x03	// Instruction MicroTLB miss (unused on ARM1156).
#define PERF_TYPE_D_MICROTLB_MISS            0x04	// Data MicroTLB miss (unused on ARM1156).
#define PERF_TYPE_BRANCH_EXECUTED            0x05	// Branch instruction executed, branch might or might not have changed program flow.
#define PERF_TYPE_BRANCH_PRED_INCORRECT      0x06	// Branch mis-predicted.
#define PERF_TYPE_INSTRUCTION_EXECUTED       0x07	// Instructions executed.
#define PERF_TYPE_D_CACHE_ACCESS_CACHEABLE   0x09	// Data cache access, not including Cache operations. This event occurs for each non-sequential access to a cache line, for cacheable locations.
#define PERF_TYPE_D_CACHE_ACCESS             0x0A	// Data cache access, not including Cache Operations. This event occurs for each non-sequential access to a cache line, regardless of whether or not the location is cacheable.
#define PERF_TYPE_D_CACHE_MISS               0x0B	// Data cache miss, not including Cache Operations.
#define PERF_TYPE_D_CACHE_WRITEBACK          0x0C	// Data cache write-back. This event occurs once for each half line of four words that is written back from the cache.
#define PERF_TYPE_SOFTWARE_CHANGED_PC        0x0D	// Software changed the PC. This event occurs any time the PC is changed by software and there is not a mode change. For example, a MOV instruction with PC as the destination triggers this event. Executing a SWI from User mode does not trigger this event, because it incurs a mode change.
#define PERF_TYPE_MAINTLB_MISS               0x0F	// Main TLB miss (unused on ARM1156).
#define PERF_TYPE_EXPLICIT_DATA_ACCESS       0x10	// Explicit external data or peripheral access. This includes cache refill, non-cacheable and write-through accesses. It does not include write-backs or page table walks.
#define PERF_TYPE_FULL_LOAD_STORE_REQ_QUEUE  0x11	// Stall because of Load Store Unit request queue being full. This event occurs each clock cycle in which the condition is met. A high incidence of this event indicates the LSU is often waiting for transactions to complete on the external bus.	
#define PERF_TYPE_WRITE_BUFF_DRAINED         0x12	// The number of times the Write Buffer was drained because of a Data Synchronization Barrier command or Strongly Ordered operation.
// 0x13 The number of cycles which FIQ interrupts are disabled (ARM1156 only).
// 0x14 The number of cycles which IRQ interrupts are disabled (ARM1156 only).
#define PERF_TYPE_EXT0                       0x20	// ETMEXTOUT[0] signal was asserted for a cycle.
#define PERF_TYPE_EXT1                       0x21	// ETMEXTOUT[1] signal was asserted for a cycle.
#define PERF_TYPE_EXT0_AND_EXT1              0x22	// ETMEXTOUT[0] or ETMEXTOUT[1] was asserted. If both ETMEXTOUT[0] and ETMEXTOUT[1] signals are asserted then the count is incremented by two.
#define PERF_TYPE_PROC_RETURN_PUSHED         0x23	// Procedure call instruction executed. The procedure return address was pushed on to the return stack (ARM1176 only).
#define PERF_TYPE_PROC_RETURN_POPPED         0x24	// Procedure return instruction executed. The procedure return address was popped off the return stack (ARM1176 only).
#define PERF_TYPE_PROC_RETURN_PRED_CORRECT   0x25	// Procedure return instruction executed and return address predicted. The procedure return address was popped off the return stack and the core branched to this address (ARM1176 only).
#define PERF_TYPE_PROC_RETURN_PRED_INCORRECT 0x26	// Procedure return instruction executed and return address predicted incorrectly. The procedure return address was restored to the return stack following the prediction being identified as incorrect (ARM1176 only).
// 0x30 Instruction cache Tag or Valid RAM parity error (ARM1156 only).
// 0x31 Instruction cache RAM parity error (ARM1156 only).
// 0x32 Data cache Tag or Valid RAM parity error (ARM1156 only).
// 0x33 Data cache RAM parity error (ARM1156 only).
// 0x34 ITCM error (ARM1156 only).
// 0x35 DTCM error (ARM1156 only).
// 0x36 Procedure return address popped off the return stack (ARM1156 only).
// 0x37 Procedure return address popped off the return stack has been incorrectly predicted by the PFU (ARM1156 only).
// 0x38 Data cache Dirty RAM parity error (ARM1156 only).
#define PERF_TYPE_EVERY_CYCLE                0xFF		// An increment each cycle.

#endif

typedef struct {
   unsigned cycle_counter;
   int num_counters;
   int type[MAX_COUNTERS];
   unsigned counter[MAX_COUNTERS];;
} perf_counters_t;

extern void reset_performance_counters(perf_counters_t *pct);

extern void read_performance_counters(perf_counters_t *pct);

extern void print_performance_counters(perf_counters_t *pct);

//extern int benchmark();

#endif
