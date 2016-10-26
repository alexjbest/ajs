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

#define regreg2(N,M)  do {if (istrcmp(name, #N)) return M;} while(0)
#define regreg(N) regreg2(N,N)

static inline bool
istrcmp(const std::string s1, const char *s2) {
  bool result = true;
  if (s1.size() != strlen(s2))
    result = false;
  for (size_t i = 0; result && i < s1.size(); i++) {
    if (toupper(s1.at(i)) != toupper(s2[i]))
      result = false;
  }
  return result;
}

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

static asmjit::X86Reg getYmmRegFromName(std::string name) {
    using namespace asmjit::x86;
    regreg(ymm0);
    regreg(ymm1);
    regreg(ymm2);
    regreg(ymm3);
    regreg(ymm4);
    regreg(ymm5);
    regreg(ymm6);
    regreg(ymm7);
    regreg(ymm8);
    regreg(ymm9);
    regreg(ymm10);
    regreg(ymm11);
    regreg(ymm12);
    regreg(ymm13);
    regreg(ymm14);
    regreg(ymm15);
    return noGpReg;
}

/* The ax, bx, cx, dx registers can be named, e.g.,
 * rax, eax, ax, ah, al, axl (=al)
 * to refer to different widths/parts.
 * The "axl" form is supported by gas, so we allow it, too. */
#define regabcd(X) \
    do { \
        regreg(r##X##x); \
        regreg(e##X##x); \
        regreg(X##x); \
        regreg(X##h); \
        regreg(X##l); \
        regreg2(X##xl,X##l); \
    } while (0)

/* The bp, sp, si, di registers can be named, e.g.,
 * rbp, ebp, bp, bpl.
 */

#define regbssd(X) \
    do { \
        regreg(r##X); \
        regreg(e##X); \
        regreg(X); \
        regreg(X##l); \
    } while (0)

/* The r8, ..., r15 registers can be named, e.g.,
 * r8, r8d, r8w, r8b.
 */

#define reg8_15(X) \
    do { \
        regreg(r##X); \
        regreg(r##X##d); \
        regreg(r##X##w); \
        regreg(r##X##b); \
    } while (0)

static asmjit::X86GpReg
getGpRegFromName(std::string name) {
  using namespace asmjit::x86;
  regabcd(a);  regabcd(b);  regabcd(c);  regabcd(d);
  regbssd(bp); regbssd(sp); regbssd(si); regbssd(di);
  reg8_15(8);  reg8_15(9);  reg8_15(10); reg8_15(11);
  reg8_15(12); reg8_15(13); reg8_15(14); reg8_15(15);
  return noGpReg;
}

static asmjit::X86Reg
getRegFromName(std::string name) {
  if (tolower(name.at(0)) == 'x') {
      return getXmmRegFromName(name);
  } else if (tolower(name.at(0)) == 'y') {
      return getYmmRegFromName(name);
  } else
      return getGpRegFromName(name);
}


asmjit::X86Mem
parse_pointer_intel(std::string ptr_str, uint32_t size, bool verbose=false);

#undef regreg

#endif /* AJS_PARSING_H_ */
