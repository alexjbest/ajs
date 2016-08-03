LIBASMJIT = libasmjit.a
ASMJITBASE = asmjit
INTELPCMBASE = ../intelpcm/intelpcm.so
GMPBASE = ../gmp-6.1.0
CC = g++
ODIR = obj
SDIR = src
INC = -I. -I$(ASMJITBASE) -I$(GMPBASE)
CFLAGS = -g -Wno-attributes -O2 --std=c++0x -lasmjit -lgmp

ASMJITOBJS = $(ASMJITBASE)/base/assembler.o $(ASMJITBASE)/base/compiler.o \
             $(ASMJITBASE)/base/compilercontext.o $(ASMJITBASE)/base/constpool.o \
             $(ASMJITBASE)/base/containers.o $(ASMJITBASE)/base/cpuinfo.o \
             $(ASMJITBASE)/base/globals.o $(ASMJITBASE)/base/hlstream.o \
             $(ASMJITBASE)/base/logger.o $(ASMJITBASE)/base/operand.o \
             $(ASMJITBASE)/base/podvector.o $(ASMJITBASE)/base/runtime.o \
             $(ASMJITBASE)/base/utils.o $(ASMJITBASE)/base/vmem.o $(ASMJITBASE)/base/zone.o \
             $(ASMJITBASE)/x86/x86assembler.o $(ASMJITBASE)/x86/x86compiler.o \
             $(ASMJITBASE)/x86/x86compilercontext.o $(ASMJITBASE)/x86/x86compilerfunc.o \
             $(ASMJITBASE)/x86/x86inst.o $(ASMJITBASE)/x86/x86operand.o \
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
