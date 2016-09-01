/*
 * ajs_parsing.h
 *
 *  Created on: 01.09.2016
 *      Author: kruppa
 */

#ifndef AJS_PARSING_H_
#define AJS_PARSING_H_

#include <string>
#include <asmjit/asmjit.h>

#define regreg(N)  do {if (name == #N) return N;} while(0)

static asmjit::X86Reg getXmmRegFromName(std::string name) {
    using namespace asmjit::x86;
    regreg(xmm0);
    regreg(xmm1);
    regreg(xmm2);
    regreg(xmm3);
    regreg(xmm4);
    regreg(xmm5);
    regreg(xmm6);
    regreg(xmm7);
    regreg(xmm8);
    regreg(xmm9);
    regreg(xmm10);
    regreg(xmm11);
    regreg(xmm12);
    regreg(xmm13);
    regreg(xmm14);
    regreg(xmm15);
    return noGpReg;
}

static inline asmjit::X86GpReg
getGpRegFromName(std::string name) {
  using namespace asmjit::x86;
  regreg(rax);  regreg(eax);   regreg(ax);   regreg(ah);  regreg(al);
  regreg(rbx);  regreg(ebx);   regreg(bx);   regreg(bh);  regreg(bl);
  regreg(rcx);  regreg(ecx);   regreg(cx);   regreg(ch);  regreg(cl);
  regreg(rdx);  regreg(edx);   regreg(dx);   regreg(dh);  regreg(dl);
  regreg(rbp);  regreg(ebp);   regreg(bp);
  regreg(rsp);  regreg(esp);   regreg(sp);
  regreg(rsi);  regreg(esi);   regreg(si);
  regreg(rdi);  regreg(edi);   regreg(di);
  regreg(r8);   regreg(r8d);   regreg(r8w);               regreg(r8b);
  regreg(r9);   regreg(r9d);   regreg(r9w);               regreg(r9b);
  regreg(r10);  regreg(r10d);  regreg(r10w);              regreg(r10b);
  regreg(r11);  regreg(r11d);  regreg(r11w);              regreg(r11b);
  regreg(r12);  regreg(r12d);  regreg(r12w);              regreg(r12b);
  regreg(r13);  regreg(r13d);  regreg(r13w);              regreg(r13b);
  regreg(r14);  regreg(r14d);  regreg(r14w);              regreg(r14b);
  regreg(r15);  regreg(r15d);  regreg(r15w);              regreg(r15b);
  return noGpReg;
}

static inline asmjit::X86Reg
getRegFromName(std::string name) {
  if (name.at(0) == 'x')
    return getXmmRegFromName(name);
  return getGpRegFromName(name);
}


asmjit::X86Mem
parse_pointer_intel(std::string ptr_str, uint32_t size, bool verbose=false);

#undef regreg

#endif /* AJS_PARSING_H_ */
