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
#else
#define START_COUNTER rdtscp
#define END_COUNTER rdtscp
	static uint64_t start_time, end_time;
#endif


__attribute__((UNUSED))
static inline uint64_t rdtsc()
{
  uint32_t high, low;
  asm volatile("RDTSC\n\t"
               : "=d" (high), "=a" (low)
              );
  return ((uint64_t) high << 32) + (uint64_t) low;
}

__attribute__((UNUSED))
static inline uint64_t rdtscp()
{
  uint32_t high, low;
  asm volatile("RDTSCP\n\t"
               : "=d" (high), "=a" (low)
               :: "%rbx", "%rcx"
               );
  return ((uint64_t) high << 32) + (uint64_t) low;
}

__attribute__((UNUSED))
static inline uint64_t rdtscid()
{
  uint32_t high, low;
  asm volatile("RDTSC\n\t"
               "mov %%edx, %0\n\t"
               "mov %%eax, %1\n\t"
               "CPUID\n\t"
               : "=rm" (high), "=rm" (low) ::
               "%rax", "%rbx", "%rcx", "%rdx");
  return ((uint64_t) high << 32) + (uint64_t) low;
}

__attribute__((UNUSED))
static inline uint64_t idrdtsc()
{
  uint32_t high, low;
  asm volatile(
               "CPUID\n\t"
               "RDTSC\n\t"
               : "=d" (high), "=a" (low) ::
               "%rbx", "%rcx");
  return ((uint64_t) high << 32) + (uint64_t) low;
}

__attribute__((UNUSED))
static inline uint64_t rdtscpid()
{
  uint32_t high, low;
  asm volatile("RDTSCP\n\t"
               "mov %%edx, %0\n\t"
               "mov %%eax, %1\n\t"
               "CPUID\n\t"
               : "=rm" (high), "=rm" (low) ::
               "%rax", "%rbx", "%rcx", "%rdx");
  return ((uint64_t) high << 32) + (uint64_t) low;
}

__attribute__((UNUSED))
static inline uint64_t idrdtscp()
{
  uint32_t high, low;
  asm volatile(
               "CPUID\n\t"
               "RDTSCP\n\t"
               : "=d" (high), "=a" (low) ::
               "%rbx", "%rcx");
  return ((uint64_t) high << 32) + (uint64_t) low;
}

static void init_timing()
{
#ifdef USE_INTEL_PCM
  printf("Using Intel PCM library\n");
  m = PCM::getInstance();
  if (m->program() != PCM::Success) {
    printf("Could not initialise PCM\n");
    exit(EXIT_FAILURE);
  }

  const bool have_smt = m->getSMT();
  if (have_smt) {
	  printf("CPU uses SMT\n");
  } else {
	  printf("CPU does not use SMT\n");
  }
#elif defined(USE_PERF)
  printf("Using PERF library\n");
  pd = libperf_initialize(-1,-1); /* init lib */
  libperf_enablecounter(pd, LIBPERF_COUNT_HW_CPU_CYCLES);
                                        /* enable HW counter */
#else
  printf("Using " _xstr(START_COUNTER) " to start and " _xstr(END_COUNTER) " to end measurement\n");
#endif
}

static void clear_timing()
{
#ifdef USE_INTEL_PCM
  m->cleanup();
#elif defined(USE_PERF)
  libperf_close(pd);
  pd = NULL;
#endif
}

static void start_timing()
{
#ifdef USE_INTEL_PCM
    before_sstate = getCoreCounterState(cpu);
#elif defined(USE_PERF)
    start_time = libperf_readcounter(pd, LIBPERF_COUNT_HW_CPU_CYCLES);
                                          /* obtain counter value */
#else
    start_time = START_COUNTER();
#endif
}

static void end_timing()
{
#ifdef USE_INTEL_PCM
    after_sstate = getCoreCounterState(cpu);
#elif defined(USE_PERF)
    end_time = libperf_readcounter(pd, LIBPERF_COUNT_HW_CPU_CYCLES);
                                          /* obtain counter value */
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
