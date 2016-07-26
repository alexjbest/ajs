LIBASMJIT = libasmjit.a
CC = g++
ODIR = obj
SDIR = src
INC = -Iasmjit -I. -I../gmp-6.1.1/
CFLAGS = -g -Wno-attributes -O2 --std=c++0x -lasmjit -lgmp -pthread

ASMJITOBJS = asmjit/base/assembler.o asmjit/base/compiler.o \
             asmjit/base/compilercontext.o asmjit/base/constpool.o \
             asmjit/base/containers.o asmjit/base/cpuinfo.o \
             asmjit/base/globals.o asmjit/base/hlstream.o \
             asmjit/base/logger.o asmjit/base/operand.o \
             asmjit/base/podvector.o asmjit/base/runtime.o \
             asmjit/base/utils.o asmjit/base/vmem.o asmjit/base/zone.o \
             asmjit/x86/x86assembler.o asmjit/x86/x86compiler.o \
             asmjit/x86/x86compilercontext.o asmjit/x86/x86compilerfunc.o \
             asmjit/x86/x86inst.o asmjit/x86/x86operand.o \
             asmjit/x86/x86operand_regs.o

INTELPCMOBJS = ../intelpcm/intelpcm.so/client_bw.o \
		../intelpcm/intelpcm.so/cpucounters.o \
		../intelpcm/intelpcm.so/msr.o \
		../intelpcm/intelpcm.so/pci.o \
		../intelpcm/intelpcm.so/utils.o

AJSOBJS = line.o transform.o

all: $(INTELPCMOBJS)$(LIBASMJIT) $(AJSOBJS) ajs.cpp
	$(CC) -o ajs ajs.cpp $(INTELPCMOBJS) $(AJSOBJS) -L. $(INC) $(CFLAGS)

%.o: %.cpp
	$(CC) -c $(INC) -o $@ $< $(CFLAGS)

$(LIBASMJIT): $(ASMJITOBJS)
	ar rvs $(LIBASMJIT) $^

clean:
	rm -f $(ASMJITOBJS) $(LIBASMJIT) $(AJSOBJS) ajs

check: all
	./test

.PHONY: clean check all
