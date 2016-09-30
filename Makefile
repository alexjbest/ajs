LIB_ASMJIT = $(HOME)/lib/libasmjit.a
INCLUDE_ASMJIT = $(HOME)/include/asmjit/

INTELPCM_BASE = $(HOME)/sources/IntelPerformanceCounterMonitor-V2.11
INCLUDE_INTELPCM = -I$(INTELPCM_BASE)
LIB_INTELPCM = $(INTELPCM_BASE)/intelpcm.so/libintelpcm.a

GMPIMPL = $(HOME)/sources/gmp-6.1.1/
GMPMPARAM = $(HOME)/build/gmp-6.1.1/

JEVENTS_BASE = ../pmu-tools/jevents/
INCLUDE_JEVENTS = -I$(JEVENTS_BASE)
LIB_JEVENTS = -L$(JEVENTS_BASE) -ljevents

LIBPERF = -lperf

CC = gcc
CXX = g++
ODIR = obj
SDIR = src
INC = -I. -I$(INCLUDE_ASMJIT) -I$(GMPIMPL) -I$(GMPMPARAM)
LIB = -lasmjit -lgmp
CFLAGS = -g -O2 -std=gnu11 -Wall -Wextra
CXXFLAGS = -g -O2 --std=c++0x -Wall

AJSOBJS = line.o transform.o utils.o ajs_parsing.o eval.o
TESTSOBJS = utils.o ajs_parsing.o eval.o

all: ajs ajs-perf ajs-jev

ajs: $(LIB_ASMJIT) $(AJSOBJS) ajs.cpp *.h
	$(CXX) -o $@ ajs.cpp $(AJSOBJS) $(INC) $(CXXFLAGS) $(LIB_ASMJIT) $(LIB)

ajs-pcm: $(LIB_ASMJIT) $(AJSOBJS) ajs.cpp *.h
	$(CXX) -o $@ ajs.cpp -DUSE_INTEL_PCM=1 $(AJSOBJS) $(INC) $(CXXFLAGS) $(INCLUDE_INTELPCM) $(LIB_ASMJIT) $(LIB_INTELPCM) $(LIB) -pthread

ajs-perf: $(LIB_ASMJIT) $(AJSOBJS) ajs.cpp *.h
	$(CXX) -o $@ ajs.cpp -DUSE_PERF=1 $(AJSOBJS) $(INC) $(CXXFLAGS) $(LIB_ASMJIT) $(LIBPERF) $(LIB) -pthread -static

ajs-jev: $(LIB_ASMJIT) $(AJSOBJS) ajs.cpp *.h
	$(CXX) -o $@ ajs.cpp -DUSE_JEVENTS=1 $(AJSOBJS) $(INC) $(CXXFLAGS) $(INCLUDE_JEVENTS) $(LIB_ASMJIT) $(LIB_JEVENTS) $(LIB)

%.o: %.cpp
	$(CXX) -c $(INC) -o $@ $< $(CXXFLAGS)

%.o: %.c
	$(CC) -c $(INC) -o $@ $< $(CFLAGS)

unittests: unittests.cpp $(TESTSOBJS)
	$(CXX) -o $@ $< -O0 -g -Wall -Wextra $(TESTSOBJS) $(LIB)
	
check: unittests ajs-jev
	./unittests
	./unittests.sh ./ajs-jev

clean:
	rm -f $(AJSOBJS) ajs ajs-pcm ajs-perf ajs-jev unittests

.PHONY: clean check all ajs ajs-pcm ajs-perf ajs-jev
