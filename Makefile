LIBASMJIT = libasmjit.a
CC = g++
ODIR = obj
SDIR = src
INC = -Iasmjit -I.
CFLAGS = -g -Wno-attributes -O2 --std=c++11

ASMJITOBJS = asmjit/base/assembler.o asmjit/base/compiler.o \
       asmjit/base/compilercontext.o asmjit/base/constpool.o \
       asmjit/base/containers.o asmjit/base/cpuinfo.o asmjit/base/globals.o \
       asmjit/base/hlstream.o asmjit/base/logger.o asmjit/base/operand.o \
       asmjit/base/runtime.o asmjit/base/utils.o asmjit/base/vmem.o \
       asmjit/base/zone.o asmjit/x86/x86assembler.o asmjit/x86/x86compiler.o \
       asmjit/x86/x86compilercontext.o asmjit/x86/x86compilerfunc.o \
       asmjit/x86/x86cpuinfo.o asmjit/x86/x86inst.o asmjit/x86/x86operand.o \
       asmjit/x86/x86operand_regs.o asmjit/x86/x86scheduler.o

AJSOBJS = line.o

all: $(LIBASMJIT) ajs.cpp
	$(CC) -o ajs ajs.cpp $(AJSOBJS) -L. -lasmjit $(INC) $(CFLAGS)

%.o: %.cpp
	$(CC) -c $(INC) -o $@ $< $(CFLAGS)

$(LIBASMJIT): $(ASMJITOBJS)
	ar rvs $(LIBASMJIT) $^

.PHONY: clean

clean:
	rm -f $(ASMJITOBJS) $(LIBASMJIT) ajs
