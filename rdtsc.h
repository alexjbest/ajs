/*
 * rdtsc.h
 *
 *  Created on: 09.08.2016
 *      Author: kruppa
 */

#ifndef RDTSC_H_
#define RDTSC_H_

/* Enable at most one of these two defines */
// #define USE_INTEL_PCM
// #define USE_PERF

#ifndef _xstr
#define _xstr(s) _str(s)
#endif
#ifndef _str
#define _str(x) #x
#endif

#ifdef USE_INTEL_PCM
	#include "cpucounters.h"
	static PCM * m;
	static CoreCounterState before_sstate, after_state;
#elif defined USE_PERF
	#include "libperf.h"  /* standard libperf include */
	static struct libperf_data* pd;
	static uint64_t start_time, end_time;
#elif USE_JEVENTS
	extern "C" {
#include "rdpmc.h"
	}
	struct rdpmc_ctx ctx;
	uint64_t start_time, end_time;
#else
#define START_COUNTER rdpmc_cycles
#define END_COUNTER rdpmc_cycles
	static uint64_t start_time, end_time;
#endif

	__attribute__((UNUSED))
static inline void serialize()
{
		unsigned long dummy = 0;
		asm volatile("CPUID\n\t"
	                 : "+a" (dummy)
	                 :: "%rbx", "%rcx", "%rdx"
	                );
}

__attribute__((UNUSED))
static inline void rdtsc(uint32_t &low, uint32_t &high)
{
  asm volatile("RDTSC\n\t"
               : "=d" (high), "=a" (low)
              );
}

__attribute__((UNUSED))
static inline void rdtscp(uint32_t &low, uint32_t &high)
{
  asm volatile("RDTSCP\n\t"
               : "=d" (high), "=a" (low)
               :: "%rcx"
              );
}

__attribute__((UNUSED))
static inline uint64_t rdtscl()
{
  uint32_t high, low;
  rdtsc(low, high);
  return ((uint64_t) high << 32) + (uint64_t) low;
}

__attribute__((UNUSED))
static inline uint64_t rdtscpl()
{
  uint32_t high, low;
  rdtscp(low, high);
  return ((uint64_t) high << 32) + (uint64_t) low;
}

__attribute__((UNUSED))
static inline uint64_t rdtscidl()
{
  uint32_t high, low;
  rdtsc(low, high);
  serialize();
  return ((uint64_t) high << 32) + (uint64_t) low;
}

__attribute__((UNUSED))
static inline uint64_t idrdtscl()
{
  uint32_t high, low;
  serialize();
  rdtsc(low, high);
  return ((uint64_t) high << 32) + (uint64_t) low;
}

__attribute__((UNUSED))
static inline uint64_t rdtscpidl()
{
  uint32_t high, low;
  rdtscp(low, high);
  serialize();
  return ((uint64_t) high << 32) + (uint64_t) low;
}

__attribute__((UNUSED))
static inline uint64_t idrdtscpl()
{
  uint32_t high, low;
  serialize();
  rdtscp(low, high);
  return ((uint64_t) high << 32) + (uint64_t) low;
}

// rdpmc_cycles uses a "fixed-function" performance counter to return
// the count of actual CPU core cycles executed by the current core.
// Core cycles are not accumulated while the processor is in the "HALT"
// state, which is used when the operating system has no task(s) to run
// on a processor core.
// Note that this counter continues to increment during system calls
// and task switches. As such, it may be unreliable for timing long
// functions where the CPU may serve an interrupt request or where
// the kernel may preempt execution and switch to another process.
// It is best used for timing short intervals which usually run
// uninterrupted, and where occurrences of interruption are easily
// detected by an abnormally large cycle count.

// The RDPMC instruction must be enabled for execution in user-space.
// This requires a total of three bits to be set in CR4 and MSRs of
// the CPU:
// Bit 1<<8 in CR4 must be set to 1. On Linux, this can be effected by
// executing as root:
//   echo 1 >> /sys/devices/cpu/rdpmc
// Bit 1<<33 must be set to 1 in the MSR_CORE_PERF_GLOBAL_CTRL
// (MSR address 0x38f). This enables the cycle counter so that it
// actually increment with each clock cycle; while this bit is 0,
// the counter value stays fixed.
// Bit 1<<5 must be set to 1 in the MSR_CORE_PERF_FIXED_CTR_CTRL
// (MSR address 0x38d) which causes the counter to be incremented
// on non-halted clock cycles that occur while the CPL is >0
// (user-mode). If bit 1<<4 is set to 1, then the counter will
// increment while CPL is 0 (kernel mode), e.g., during interrupts,
// etc.

unsigned long rdpmc_cycles()
{
   unsigned a, d;
   const unsigned c = (1<<30) + 1; /* Second Fixed-function counter:
                                      clock cycles in non-HALT */

   __asm__ volatile("rdpmc" : "=a" (a), "=d" (d) : "c" (c));

   return ((unsigned long)a) | (((unsigned long)d) << 32);
}

static void init_timing()
{
#ifdef USE_INTEL_PCM
  printf("# Using Intel PCM library\n");
  m = PCM::getInstance();
  if (m->program() != PCM::Success) {
    printf("Could not initialise PCM\n");
    exit(EXIT_FAILURE);
  }

  const bool have_smt = m->getSMT();
  if (have_smt) {
	  printf("# CPU uses SMT\n");
  } else {
	  printf("# CPU does not use SMT\n");
  }
#elif defined(USE_PERF)
  printf("# Using PERF library\n");
  pd = libperf_initialize(-1,-1); /* init lib */
  libperf_enablecounter(pd, LIBPERF_COUNT_HW_CPU_CYCLES);
                                        /* enable HW counter */
#elif defined(USE_JEVENTS)
  printf("# Using jevents library\n");
  if (rdpmc_open(PERF_COUNT_HW_CPU_CYCLES, &ctx) < 0)
	exit(EXIT_FAILURE);
#else
  printf("# Using " _xstr(START_COUNTER) " to start and " _xstr(END_COUNTER) " to end measurement\n");
#endif
}

static void clear_timing()
{
#ifdef USE_INTEL_PCM
  m->cleanup();
#elif defined(USE_PERF)
  libperf_close(pd);
  pd = NULL;
#elif defined(USE_JEVENTS)
  rdpmc_close (&ctx);
#endif
}

static void start_timing()
{
#ifdef USE_INTEL_PCM
    before_sstate = getCoreCounterState(cpu);
#elif defined(USE_PERF)
    start_time = libperf_readcounter(pd, LIBPERF_COUNT_HW_CPU_CYCLES);
                                          /* obtain counter value */
#elif defined(USE_JEVENTS)
	start_time = rdpmc_cycles();
	// start_time = rdpmc_read(&ctx);
#else
    // start_time = START_COUNTER();
#endif
}

static void end_timing()
{
#ifdef USE_INTEL_PCM
    after_sstate = getCoreCounterState(cpu);
#elif defined(USE_PERF)
    end_time = libperf_readcounter(pd, LIBPERF_COUNT_HW_CPU_CYCLES);
                                          /* obtain counter value */
#elif defined(USE_JEVENTS)
    end_time = rdpmc_cycles();
    // end_time = rdpmc_read(&ctx);
#else
    end_time = END_COUNTER();
#endif
}

static uint64_t get_diff_timing()
{
#ifdef USE_INTEL_PCM
    return getCycles(before_sstate,after_sstate);
#elif defined(USE_PERF)
    return end_time - start_time;
#else
    return end_time - start_time;
#endif

}

#endif /* RDTSC_H_ */
