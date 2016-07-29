LIBASMJIT = libasmjit.a
ASMJITBASE = asmjit
INTELPCMBASE = ../intelpcm/intelpcm.so
GMPBASE = ../gmp-6.1.0
CC = g++
ODIR = obj
SDIR = src
INC = -I. -I$(ASMJITBASE) -I$(GMPBASE)
CFLAGS = -g -Wno-attributes -O2 --std=c++0x -lasmjit -lgmp

ASMJITOBJS = $(ASMJITBASE)/base/assembler.o asmjit/base/compiler.o \
             $(ASMJITBASE)/base/compilercontext.o asmjit/base/constpool.o \
             $(ASMJITBASE)/base/containers.o asmjit/base/cpuinfo.o \
             $(ASMJITBASE)/base/globals.o asmjit/base/hlstream.o \
             $(ASMJITBASE)/base/logger.o asmjit/base/operand.o \
             $(ASMJITBASE)/base/podvector.o asmjit/base/runtime.o \
             $(ASMJITBASE)/base/utils.o asmjit/base/vmem.o asmjit/base/zone.o \
             $(ASMJITBASE)/x86/x86assembler.o asmjit/x86/x86compiler.o \
             $(ASMJITBASE)/x86/x86compilercontext.o asmjit/x86/x86compilerfunc.o \
             $(ASMJITBASE)/x86/x86inst.o asmjit/x86/x86operand.o \
             $(ASMJITBASE)/x86/x86operand_regs.o

INTELPCMOBJS = $(INTELPCMBASE)/client_bw.o \
		$(INTELPCMBASE)/cpucounters.o \
		$(INTELPCMBASE)/msr.o \
		$(INTELPCMBASE)/pci.o \
		$(INTELPCMBASE)/utils.o

AJSOBJS = line.o transform.o

all: ajs

ajs: $(LIBASMJIT) $(AJSOBJS) ajs.cpp *.h
	$(CC) -o ajs ajs.cpp $(AJSOBJS) -L. $(INC) $(CFLAGS)

ajs-pcm: $(INTELPCMOBJS) $(LIBASMJIT) $(AJSOBJS) ajs.cpp *.h
	$(CC) -o ajs ajs.cpp $(INTELPCMOBJS) $(AJSOBJS) -L. $(INC) $(CFLAGS) -pthread

%.o: %.cpp
	$(CC) -c $(INC) -o $@ $< $(CFLAGS)

$(LIBASMJIT): $(ASMJITOBJS)
	ar rvs $(LIBASMJIT) $^

clean:
	rm -f $(ASMJITOBJS) $(LIBASMJIT) $(AJSOBJS) ajs

check: all
	./test

.PHONY: clean check all ajs-pcm
