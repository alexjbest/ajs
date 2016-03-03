OUT = libasmjit.a
CC = g++
ODIR = obj
SDIR = src
INC = -Iasmjit
CFLAGS = -g -Wno-attributes -O2

OBJS = asmjit/base/assembler.o asmjit/base/compiler.o \
       asmjit/base/compilercontext.o asmjit/base/constpool.o \
       asmjit/base/containers.o asmjit/base/cpuinfo.o asmjit/base/globals.o \
       asmjit/base/hlstream.o asmjit/base/logger.o asmjit/base/operand.o \
       asmjit/base/runtime.o asmjit/base/utils.o asmjit/base/vmem.o \
       asmjit/base/zone.o asmjit/x86/x86assembler.o asmjit/x86/x86compiler.o \
       asmjit/x86/x86compilercontext.o asmjit/x86/x86compilerfunc.o \
       asmjit/x86/x86cpuinfo.o asmjit/x86/x86inst.o asmjit/x86/x86operand.o \
       asmjit/x86/x86operand_regs.o asmjit/x86/x86scheduler.o

all: $(OUT) ajs.cpp
	$(CC) -o ajs ajs.cpp -L. -lasmjit -I. -Iasmjit $(CFLAGS)

%.o: %.cpp 
	$(CC) -c $(INC) -o $@ $< $(CFLAGS)

$(OUT): $(OBJS) 
	ar rvs $(OUT) $^

.PHONY: clean

clean:
	rm -f $(OBJS) $(OUT) ajs
