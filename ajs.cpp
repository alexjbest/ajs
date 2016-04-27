#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <sstream>
#include <vector>
#include <map>
#include <list>
#include <assert.h>
#include <asmjit/asmjit.h>
#include <stdlib.h>
#include <getopt.h>
#include "line.h"

#define regreg(N)  if (name == #N) return N
#define debug_print(fmt, ...) \
  do { fprintf(stderr, "# %s:%d:%s(): " fmt, __FILE__, \
      __LINE__, __func__, __VA_ARGS__); } while (0)

#define stringify( x ) static_cast< std::ostringstream & >( \
            ( std::ostringstream() << std::dec << x ) ).str()
#define MAX_OPS 3

using namespace asmjit;
using namespace x86;
using namespace std;

/* TODO list:
 * ----------
 * check if for any other base for immediate values needed
 * extend parsing part to handle spaces well
 * check if there are other directives that will affect performance
 * check if there are other directives that will affect correctness
 * make repz ret work
 * make shr %r9 work
 * do registers more programmatically?
 * add help option, usage message etc.
 * add make dependencies for headers
 * make the dependency update stuff work better when adding nops, i.e. update indices
 * add transforming swaps
 * make byte emitting print to output better
 */


class ajs {

  public:
    static std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
      std::stringstream ss(s);
      std::string item;
      while (std::getline(ss, item, delim)) {
        elems.push_back(item);
      }
      return elems;
    }

    static std::vector<std::string> &split2(const std::string &s, char delim, char delim2, std::vector<std::string> &elems) {
      std::stringstream ss(s);
      std::string item;
      while (std::getline(ss, item, delim)) {
        std::vector<std::string> elems2 = split(item, delim2);
        elems.insert(elems.end(), elems2.begin(), elems2.end());
      }
      return elems;
    }

    static std::vector<std::string> split(const std::string &s, char delim) {
      std::vector<std::string> elems;
      split(s, delim, elems);
      return elems;
    }

    static std::vector<std::string> split2(const std::string &s, char delim, char delim2) {
      std::vector<std::string> elems;
      split2(s, delim, delim2, elems);
      return elems;
    }

    // trim from start
    static inline std::string &ltrim(std::string &s) {
      s.erase(s.begin(), std::find_if(s.begin(), s.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
      return s;
    }

    // trim from end
    static inline std::string &rtrim(std::string &s) {
      s.erase(std::find_if(s.rbegin(), s.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), s.end());
      return s;
    }

    // trim from both ends
    static inline std::string &trim(std::string &s) {
      return ltrim(rtrim(s));
    }

    static int64_t getVal(string val) {
      val = trim(val);
      size_t loc = val.find_first_of("+-\0", 1);
      if (loc != std::string::npos)
        return getVal(val.substr(0, loc)) + getVal(val.substr(loc));
      loc = val.find('x');
      if (loc != std::string::npos)
        return std::stol(val.c_str(), NULL, 16);
      return std::stol(val.c_str(), NULL, 10);
    }

    static Imm getValAsImm(string val) {
      return Imm(getVal(val));
    }

    static X86Reg getXmmRegFromName(string name) {
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

    static X86GpReg getGpRegFromName(string name) {
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

    static X86Reg getRegFromName(string name) {
      if (name.at(0) == 'x')
        return getXmmRegFromName(name);
      return getGpRegFromName(name);
    }

    // Parses expressions of the form disp(base,index,scalar) or [base+index*scale+disp]
    // into asmjit's X86Mem
    static X86Mem getPtrFromAddress(string addr, uint32_t size, int intelSyntax)
    {
      const char openBracket = intelSyntax ? '[' : '(';
      if (!intelSyntax) // GAS syntax
      {
        size_t i = addr.find(openBracket);
        int32_t disp = 0;
        if (i > 0)
        {
          string d = addr.substr(0, i);
          disp = getVal(d);
        }
        vector<string> bis = split(addr.substr(i + 1, addr.size() - 2 - i), ',');
        X86GpReg base;
        if (trim(bis[0]).length() != 0) // no base register
          base = getGpRegFromName(trim(bis[0]).substr(1));
        if (bis.size() > 1)
        {
          X86GpReg index = getGpRegFromName(trim(bis[1]).substr(1));
          uint32_t scalar;
          if (bis.size() > 2 && bis[2].length() != 0)
            scalar = getVal(bis[2]);
          else
            scalar = 1;
          uint32_t shift =
            (scalar == 1) ? 0 :
            (scalar == 2) ? 1 :
            (scalar == 4) ? 2 :
            (scalar == 8) ? 3 : -1;
          if (trim(bis[0]).length() == 0)
            return ptr_abs(0, index, shift, disp, size);
          return ptr(base, index, shift, disp, size);
        }
        if (trim(bis[0]).length() == 0)
          return ptr_abs(0, disp, size);
        return ptr(base, disp, size);
      }
      else // Intel syntax
      {
        X86GpReg base = noGpReg, index = noGpReg;
        size_t i, pi;
        int32_t disp = 0;
        uint32_t shift = 0;
        int wantShift = 0, neg = 0;
        string token;
        for (i = addr.find(openBracket) + 1, pi = i; i < addr.size(); i++)
        {
          if (addr.at(i) == '+' || addr.at(i) == '-' || addr.at(i) == '*' || addr.at(i) == ']')
          {
            token = addr.substr(pi, i - pi);
            try // Is it an number?
            {
              uint32_t val = getVal(token);
              if (wantShift) {
                shift =
                  (val == 1) ? 0 :
                  (val == 2) ? 1 :
                  (val == 4) ? 2 :
                  (val == 8) ? 3 : -1;
                if (index == noGpReg) {
                  index = base;
                  base = noGpReg;
                }
              }
              else {
                disp = neg ? -val : val;
              }
            }
            catch (...) {
              if (base == noGpReg)
                base = getGpRegFromName(token);
              else
                index = getGpRegFromName(token);
            }

            pi = i + 1;
            if (addr.at(i) == '*') {
              wantShift = 1;
            }
            else {
              wantShift = 0;
            }
            if (addr.at(i) == '-') {
              neg = 1;
            }
            else {
              neg = 0;
            }
          }
        }
        if (index != noGpReg) {
          if (base == noGpReg)
            return ptr_abs(0, index, shift, disp, size);
          return ptr(base, index, shift, disp, size);
        }
        if (base == noGpReg)
          return ptr_abs(0, disp, size);
        return ptr(base, disp, size);
      }
    }

    static Operand getOpFromStr(string op, X86Assembler& a, map<string, Label>& labels, uint32_t size, int intelSyntax)
    {
      // bracket style depends on syntax used
      const char openBracket = intelSyntax ? '[' : '(';
      op = trim(op);

      if (count(op.begin(), op.end(), openBracket) > 0)
        return getPtrFromAddress(op, size, intelSyntax);
      if (!intelSyntax) // GAS syntax
      {
        string sub = op.substr(1);
        if (op.at(0) == '%')
          return getRegFromName(sub);
        if (op.at(0) == '$')
          return getValAsImm(sub);
        if (op.length() > 0)
        {
          if (labels.count(op) == 0)
            labels[op] = a.newLabel();
          return labels[op];
        }
      }
      else // Intel syntax
      {
        X86Reg reg = getRegFromName(op);
        if (reg != noGpReg) // Is it a register?
          return reg;
        try // Is it an immediate?
        {
          Imm val = getValAsImm(op);
          return val;
        }
        catch (...)
        {
          if (op.length() > 0) // No, it's a label!
          {
            if (labels.count(op) == 0)
              labels[op] = a.newLabel();
            return labels[op];
          }
        }
      }
      assert(0);
      return noOperand;
    }

    static vector<X86Reg> intersection(const vector<X86Reg>& v1, const vector<X86Reg>& v2)
    {
      vector<X86Reg> in;
      for (vector<X86Reg>::const_iterator ci1 = v1.begin(); ci1 != v1.end(); ++ci1)
        for (vector<X86Reg>::const_iterator ci2 = v2.begin(); ci2 != v2.end(); ++ci2)
          if (*ci1 == *ci2)
            in.push_back(*ci2);
      return in;
    }

    // returns whether line a depends on line b or not
    // i.e. if a followed b originally whether swapping is permissible in general
    static bool dependsOn(vector<Line>::const_iterator ai, vector<Line>::const_iterator bi,
        vector<Line>& func)
    {
      Line a = *ai, b = *bi;
      // labels, align and byte statements should have all possible dependencies
      if (a.isLabel())
        return true;
      if (b.isLabel())
        return true;

      if (a.isAlign())
        return true;
      if (b.isAlign())
        return true;

      if (a.isByte())
        return true;
      if (b.isByte())
        return true;

      const X86InstInfo& ainfo = X86Util::getInstInfo(a.getInstruction()),
            &binfo = X86Util::getInstInfo(b.getInstruction());

      // if a or b are control flow instructions there is always a dependency
      if (ainfo.getExtendedInfo().isFlow())
        return true;
      if (binfo.getExtendedInfo().isFlow())
        return true;

      // if a reads flags set by b there is a dependency
      if (ainfo.getEFlagsIn() & binfo.getEFlagsOut())
        return true;

      // if a sets flags also set by b there is a dependency
      if (ainfo.getEFlagsOut() & binfo.getEFlagsOut())
        return true;

      // if a sets flags read by b there is a dependency
      if (ainfo.getEFlagsOut() & binfo.getEFlagsOut())
        return true;

      vector<X86Reg> in;

      in = intersection(a.getRegsIn(), b.getRegsOut());
      for (vector<X86Reg>::const_iterator ci = in.begin(); ci != in.end(); ++ci) // TODO poss change to if len
      {
        return true;
      }

      in = intersection(a.getRegsOut(), b.getRegsIn());
      for (vector<X86Reg>::const_iterator ci = in.begin(); ci != in.end(); ++ci)
      {
        return true;
      }

      in = intersection(a.getRegsOut(), b.getRegsOut());
      for (vector<X86Reg>::const_iterator ci = in.begin(); ci != in.end(); ++ci)
      {
        vector<Line>::const_iterator li;
        for (li = ai + 1; li != func.end(); ++li)
        {
          if (std::find(li->getRegsIn().begin(), li->getRegsIn().end(), *ci) != li->getRegsIn().end())
            return true;
          if (std::find(li->getRegsOut().begin(), li->getRegsOut().end(), *ci) != li->getRegsOut().end())
            break;
        }
        if (li == func.end())
          return true;
      }

      return false;
    }

    static void addRegsRead(Line& l)
    {
      if (!l.isInstruction())
        return;
      for (int i = 0; i < MAX_OPS; i++)
      {
        if (l.getOp(i).isReg())
        {
          if (i == 0)
          {
            const X86InstInfo& info = X86Util::getInstInfo(l.getInstruction());

            // Mov instruction does not read first op
            if (info.getExtendedInfo().isMove())
              continue;
          }
          l.addRegIn(*static_cast<const X86Reg*>(l.getOpPtr(i)));
        }
        if (l.getOp(i).isMem())
        {
          const X86Mem* m = static_cast<const X86Mem*>(l.getOpPtr(i));
          if (m->hasIndex())
          {
            X86GpReg r;
            r.setIndex(m->getIndex());
            r.setType(kX86RegTypeGpq);
            r.setSize(8); // TODO this better, maybe 32 bit regs are needed
            l.addRegIn(r);
          }
          if (m->hasBase())
          {
            X86GpReg r;
            r.setIndex(m->getBase());
            r.setType(kX86RegTypeGpq);
            r.setSize(8); // TODO this better, maybe 32 bit regs are needed
            l.addRegIn(r);
          }
        }
      }
    }

    static void addRegsWritten(Line& l)
    {
      if (!l.isInstruction())
        return;
      if (l.getOp(0).isReg())
        l.addRegOut(*static_cast<const X86Reg*>(l.getOpPtr(0)));
    }

    static void addDeps(vector<Line>::iterator newLine, vector<Line>& func)
    {
      // try to determine other dependencies
      int index = 0;
      for (vector<Line>::iterator prevLine = func.begin(); prevLine != newLine; ++prevLine, index++)
      {
        if (dependsOn(newLine, prevLine, func))
        {
          if (find(newLine->getDependencies().begin(), newLine->getDependencies().end(), index) == newLine->getDependencies().end())
          {
            newLine->addDependency(index);

            // we can now remove any of prevLine's dependencies from newLine
            // TODO does this make things faster?
            std::vector<int>::iterator position = newLine->getDependencies().begin();
            for (vector<int>::iterator ci2 = prevLine->getDependencies().begin(); ci2 != prevLine->getDependencies().end(); ++ci2)
            {
              position = std::find(position, newLine->getDependencies().end(), *ci2);
              if (position != newLine->getDependencies().end())
                position = newLine->getDependencies().erase(position);
            }
          }
        }
      }
    }

    static int loadFuncFromFile(vector<Line>& func, X86Assembler& a, const char* file, const int intelSyntax)
    {
      map<string, Label> labels;

      // if we are given a file path use it, otherwise try stdin.
      ifstream ifs;
      if (file != NULL)
        ifs.open(file);
      istream& is = file != NULL ? ifs : cin;
      string str;

      if (is.fail())
      {
        printf("# error: opening file failed, is filename correct?\n");
        return -1;
      }

      a.reset();

      // set which chars define which line types based on intelSyntax flag
      const char commentChar   = intelSyntax ? ';' : '#';
      const char directiveChar = intelSyntax ? '[' : '.';

      while (getline(is, str))
      {
        str = trim(str);
        if (str.length() == 0)
          continue;

        std::vector<string> parsed = split2(str, '\t', ' ');

        while (parsed.size() > 0)
        {
          parsed[0] = trim(parsed[0]);
          if (parsed[0].length() == 0)
          {
            parsed.erase(parsed.begin());
            continue;
          }

          // first char is the comment character so we have a comment
          if (parsed[0].at(0) == commentChar)
            break;

          // first char is the %, the nasm syntax macro character, ignore
          if (parsed[0].at(0) == '%')
            break;

          Line newLine;
          if (*parsed[0].rbegin() == ':') // last character of first token is colon, so we are at a label
          {
            string label = parsed[0].substr(0, parsed[0].size() - 1);
            if (labels.count(label) == 0)
              labels[label] = a.newLabel();

            newLine.setLabel(labels[label].getId());

            parsed.erase(parsed.begin());
          }
          else if (parsed[0].at(0) == directiveChar) // first char is the directive character so we have a directive
          {
            if (parsed[0].substr(1, 5) == "align") {
              parsed.erase(parsed.begin());

              std::vector<std::string> args = split(parsed[0], ',');
              while (parsed.size() > 0) // done with this line
                parsed.erase(parsed.begin());

              newLine.setAlign(getVal(args[0]));
            }
            else if (parsed[0] == ".byte") {
              parsed.erase(parsed.begin());

              std::vector<std::string> args = split(parsed[0], ',');
              while (parsed.size() > 0) // done with this line
                parsed.erase(parsed.begin());

              newLine.setByte(getVal(args[0]));
            }
            else // ignore non-align/byte directives
            {
              break;
            }
          }
          else if (intelSyntax && parsed[0] == "db") // yasm pseudo instructions TODO check for more
          {
            parsed.erase(parsed.begin());

            std::vector<std::string> args = split(parsed[0], ',');
            while (parsed.size() > 0) // done with this line
              parsed.erase(parsed.begin());

            newLine.setByte(getVal(args[0]));
          }
          else // normal instruction
          {
            if (parsed[0] == "ASM_START")
              break;
            if (parsed[0] == "end")
              break;
            if (parsed[0] == "jrcxz" || parsed[0] == "jecxz") // add extra arg to jr/ecx instrs
            {
              parsed[1] = parsed[0].substr(1,3) + ',' + parsed[1];
              parsed[0] = "jecxz";
            }
            uint32_t id = X86Util::getInstIdByName(parsed[0].c_str());
            uint32_t size = 0;
            if (id == kInstIdNone)
            {
              size = 1;
              // last character of instruction is q,l,w,b try removing it
              switch (*parsed[0].rbegin())
              {
                case 'q':
                  size *= 2;

                case 'l':
                  size *= 2;

                case 'w':
                  size *= 2;

                case 'b':
                  id = X86Util::getInstIdByName(parsed[0].substr(0, parsed[0].size() - 1).c_str());
                  break;
              }
            }
            parsed.erase(parsed.begin());

            newLine.setInstruction(id);
            while (parsed.size() >= 2)
            {
              if (!parsed[1].size())
              {
                parsed.erase(parsed.begin() + 1);
                continue;
              }
              if (parsed[1].at(0) == commentChar)
                break;
              parsed[0] += parsed[1];
              parsed.erase(parsed.begin() + 1);
            }
            if (parsed.size() > 0)
            {
              std::vector<std::string> args = split(parsed[0], ',');
              parsed.erase(parsed.begin());

              // stick bracketed expressions back together again
              for (int j = 0; j < args.size(); j++)
              {
                string arg = args[j];
                if (count(arg.begin(), arg.end(), '(') != count(arg.begin(), arg.end(), ')'))
                {
                  assert(j < args.size() - 1);
                  args[j] = args[j] + "," + args[j + 1];
                  args.erase(args.begin() + j + 1);
                  j--;
                }
              }

              if (!intelSyntax)
                reverse(args.begin(), args.end());

              for (int i = 0; i < args.size(); i++)
                newLine.setOp(i, getOpFromStr(args[i], a, labels, size, intelSyntax));
            }

            addRegsRead(newLine);
            addRegsWritten(newLine);
          }

          // check for dependencies annotated in the source
          if (parsed.size() > 0 && parsed[0].substr(0,5) == "#ajs:")
          {
            std::vector<std::string> deps = split(parsed[0].substr(5), ',');
            parsed.erase(parsed.begin());

            for (vector<string>::const_iterator ci = deps.begin(); ci != deps.end(); ++ci)
            {
              int newDep = atoi(ci->c_str()) - 1;
              //assert(newDep < index); // make sure the user gave us a valid sequence
              newLine.addDependency(newDep);
            }
          }

          func.insert(func.end(), newLine);
        }
      }

      a.reset();

      for (vector<Line>::iterator i = func.begin(); i != func.end(); ++i)
        addDeps(i, func);

      return labels.size();
    }

    static uint64_t callFunc(void* funcPtr, JitRuntime& runtime, uint64_t target, const int verbose, const uint64_t overhead,
        uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
    {
      uint32_t cycles_high, cycles_high1, cycles_low, cycles_low1;
      uint64_t start, end, total;
      const int loopsize = 50;
      volatile int k = 0;

      // In order to run 'funcPtr' it has to be casted to the desired type.
      // Typedef is a recommended and safe way to create a function-type.
      typedef int (*FuncType)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

      // Using asmjit_cast is purely optional, it's basically a C-style cast
      // that tries to make it visible that a function-type is returned.
      FuncType callableFunc = asmjit_cast<FuncType>(funcPtr);

      for (volatile int timing = 0; timing < 2; timing++)
      {
        total = 0;
        for (k = 0; k < loopsize; k++)
        {
          asm volatile (
              "CPUID\n\t"
              "RDTSC\n\t"
              "mov %%edx, %0\n\t"
              "mov %%eax, %1\n\t":
              "=r" (cycles_high), "=r" (cycles_low)::
              "%rax", "%rbx", "%rcx", "%rdx");

          callableFunc(arg1, arg2, arg3, arg4, arg5, arg6);

          asm volatile(
              "RDTSCP\n\t"
              "mov %%edx, %0\n\t"
              "mov %%eax, %1\n\t"
              "CPUID\n\t" :
              "=r" (cycles_high1), "=r" (cycles_low1) ::
              "%rax", "%rbx", "%rcx", "%rdx");

          start = ( ((uint64_t)cycles_high << 32) | (uint64_t)cycles_low );
          end = ( ((uint64_t)cycles_high1 << 32) | (uint64_t)cycles_low1 );
          total += end - start - overhead;

          if (target != 0 && k >= loopsize >> 2 && total > (target + 20) * (k+1))
          {
            if (verbose && timing == 1)
              printf("# cannot hit target, aborting\n");
            break;
          }
        }
      }

      total /= k;

      if (verbose)
        printf("# total time: %ld\n", total);

      runtime.release((void*)callableFunc);

      return total;
    }

    static void addFunc(vector<Line>& func, list<int>& perm, X86Assembler& a, int numLabels, int verbose)
    {
      Label labels[numLabels];
      for (int i = 0; i < numLabels; i++)
        labels[i] = a.newLabel();
      for (list<int>::const_iterator ci = perm.begin(); ci != perm.end(); ++ci)
      {
        Line& curLine = func[*ci];
        if (curLine.isAlign()) {
          a.align(kAlignCode, curLine.getAlign());
        }
        if (curLine.isLabel()) {
          a.bind(labels[curLine.getLabel()]);
        }
        if (curLine.isByte()) {
          uint8_t* cursor = a.getCursor();
          if ((size_t)(a._end - cursor) < 16)
          {
            a._grow(16);
          }
          cursor[0] = curLine.getByte();
          cursor += 1;
          if (verbose)
            a.getLogger()->logFormat(Logger::kStyleDefault,"\t.byte\t%d\n", curLine.getByte());
        }
        if (!curLine.isInstruction())
          continue;
        for (int i = 0; i < MAX_OPS; i++)
        {
          if (curLine.getOp(i).isLabel()) {
            curLine.setOp(i, labels[curLine.getOp(i).getId()]);
          }
        }
        a.emit(curLine.getInstruction(), curLine.getOp(0), curLine.getOp(1), curLine.getOp(2));
      }
    }

    // makes a function with assembler then times the generated function with callFunc.
    static uint64_t timeFunc(vector<Line>& func, list<int>& perm, X86Assembler& a, JitRuntime& runtime, int numLabels, uint64_t target, const int verbose, uint64_t overhead,
        uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
    {
      a.reset();

      addFunc(func, perm, a, numLabels, verbose);

      void* funcPtr = a.make();

      return callFunc(funcPtr, runtime, target, verbose, overhead,
          arg1, arg2, arg3, arg4, arg5, arg6);
    }

    // Linux call order:
    // RDI, RSI, RDX, RCX, R8, R9
    static void getArgs(uint64_t *mpn1, uint64_t *mpn2, uint64_t *mpn3, uint64_t *mpn4, const uint64_t limbs, string signature, uint64_t& arg1, uint64_t& arg2, uint64_t& arg3, uint64_t& arg4, uint64_t& arg5, uint64_t& arg6)
    {
      if (signature == "double")
      {
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = reinterpret_cast<uint64_t>(limbs);
      }
      else if (signature == "copyi")
      {
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = reinterpret_cast<uint64_t>(mpn2);
        arg3 = reinterpret_cast<uint64_t>(limbs);
      }
      else if (signature == "addadd_n")
      {
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = reinterpret_cast<uint64_t>(mpn2);
        arg3 = reinterpret_cast<uint64_t>(mpn3);
        arg4 = reinterpret_cast<uint64_t>(mpn4);
        arg5 = reinterpret_cast<uint64_t>(limbs);
      }
      else if (signature == "addlsh_n") // TODO
      {
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = reinterpret_cast<uint64_t>(mpn2);
        arg3 = reinterpret_cast<uint64_t>(mpn3);
        arg4 = reinterpret_cast<uint64_t>(mpn4);
        arg5 = reinterpret_cast<uint64_t>(limbs);
      }
      else if (signature == "mul_basecase")
      {
        arg1 = reinterpret_cast<uint64_t>(mpn4);
        arg2 = reinterpret_cast<uint64_t>(mpn1);
        arg3 = reinterpret_cast<uint64_t>(limbs);
        arg4 = reinterpret_cast<uint64_t>(mpn2);
        arg5 = reinterpret_cast<uint64_t>(limbs);
      }
      else
      {
        if (signature != "add_n")
          printf("# signature not recognised, defaulting to add_n\n");
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = reinterpret_cast<uint64_t>(mpn2);
        arg3 = reinterpret_cast<uint64_t>(mpn3);
        arg4 = reinterpret_cast<uint64_t>(limbs);
      }
    }

    static uint64_t tryPerms(list<int>& bestPerm, vector<Line>& func, X86Assembler& a, JitRuntime& runtime, const int numLabels, const int from, const int to, const int verbose, const uint64_t overhead, uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
    {
      int count = 0, level = 0;
      vector< list<int> > lines(to + 1 - from);
      vector< int > remaining(to + 2 - from);
      list<int> perm;

      for (int i = 0; i < func.size(); i++)
        perm.insert(perm.end(), i);

      uint64_t bestTime = timeFunc(func, perm, a, runtime, numLabels, bestTime, verbose, overhead, arg1, arg2, arg3, arg4, arg5, arg6);
      bestPerm = perm;
      printf("# original sequence: %ld\n", bestTime);

      list<int>::iterator start = perm.begin();
      advance(start, from);
      list<int>::iterator end = perm.begin();
      advance(end, to + 1);

      for (list<int>::iterator i = start; i != end;)
      {
        Line& curLine = func[*i];
        int depCount = 0;
        for (vector<int>::const_iterator ci = curLine.getDependencies().begin(); ci != curLine.getDependencies().end(); ++ci)
        {
          if (*ci >= from && *ci <= to)
            depCount++;
        }
        lines[depCount].push_back(*i);
        i = perm.erase(i);
      }

      remaining[0] = lines[0].size();

      list<int>::iterator cur = perm.begin();
      advance(cur, from - 1);

      while (level >= 0)
      {
        // if not done at current level down a level
        if (remaining[level])
        {
          ++cur;
          // add new
          cur = perm.insert(cur, lines[0].front());
          lines[0].pop_front();
          remaining[level]--;

          // update dependencies
          for (int i = 1; i < to + 1 - from; i++)
          {
            for (list<int>::iterator ci = lines[i].begin(); ci != lines[i].end(); ++ci)
            {
              Line& freeLine = func[*ci];
              if (find(freeLine.getDependencies().begin(), freeLine.getDependencies().end(), *cur) != freeLine.getDependencies().end())
              {
                lines[i - 1].push_back(*ci);
                ci = lines[i].erase(ci);
                --ci;
              }
            }
          }

          level++;
          remaining[level] = lines[0].size();
        }
        else // if done at this level maybe time and then go up a level
        {
          if (level == 0)
            break;
          if (level == to - from + 1)
          {
            count++;
            if (verbose)
              printf("\n# timing sequence:\n");
            // time this permutation
            uint64_t newTime = timeFunc(func, perm, a, runtime, numLabels, bestTime, verbose, overhead,
                arg1, arg2, arg3, arg4, arg5, arg6);
            if (bestTime == 0 || newTime < bestTime)
            {
              printf("# better sequence found: %ld", newTime);
              if (bestTime != 0)
                printf(" delta: %ld", bestTime - newTime);
              printf("\n");
              bestPerm = perm;
              bestTime = newTime;
              printf("# ");
              for (list<int>::const_iterator ci = perm.begin(); ci != perm.end(); ++ci)
                printf("%d, ", *ci + 1);
              printf("\n");
            }
          }

          // update dependencies
          for (int i = to - from; i >= 0; i--)
          {
            for (list<int>::iterator ci = lines[i].begin(); ci != lines[i].end(); ++ci)
            {
              Line& freeLine = func[*ci];
              if (find(freeLine.getDependencies().begin(), freeLine.getDependencies().end(), *cur) != freeLine.getDependencies().end())
              {
                lines[i + 1].push_back(*ci);
                ci = lines[i].erase(ci);
                --ci;
              }
            }
          }
          // remove old
          lines[0].push_back(*cur);
          cur = perm.erase(cur);

          level--;
          --cur;
        }
      }

      printf("# tried %d sequences\n", count);
      return bestTime;
    }

    static uint64_t superOptimise(list<int>& bestPerm, vector<Line>& func, X86Assembler& a, JitRuntime& runtime, const int numLabels, const int from, const int to, const uint64_t limbs, const int verbose, string signature, int nopLine = -1)
    {
      uint64_t bestTime = 0, overhead = 0;
      uint64_t *mpn1, *mpn2, *mpn3, *mpn4;
      uint64_t arg1, arg2, arg3, arg4, arg5, arg6;

      // set up arguments for use by function
      mpn1 = (uint64_t*)malloc(limbs * sizeof(uint64_t));
      mpn2 = (uint64_t*)malloc(limbs * sizeof(uint64_t));
      mpn3 = (uint64_t*)malloc(limbs * sizeof(uint64_t));
      mpn4 = (uint64_t*)malloc(2 * limbs * sizeof(uint64_t));

      getArgs(mpn1, mpn2, mpn3, mpn4, limbs, signature, arg1, arg2, arg3, arg4, arg5, arg6);

      Line ret(X86Util::getInstIdByName("ret"));
      vector<Line> emptyFunc(1, ret);
      list<int> emptyPerm(1, 0);
      overhead = timeFunc(emptyFunc, emptyPerm, a, runtime, numLabels, bestTime, verbose, overhead,
          arg1, arg2, arg3, arg4, arg5, arg6);

      bestTime = tryPerms(bestPerm, func, a, runtime, numLabels, from, to, verbose, overhead, arg1, arg2, arg3, arg4, arg5, arg6);

      list<int> nopPerm;
      if (nopLine != -1)
      {
        for (int i = 0; i < 3; i++)
        {
          printf("# trying %d nop(s)\n", i + 1);
          vector<Line>::iterator pos = func.begin();
          pos += nopLine;
          pos = func.insert(pos, Line(X86Util::getInstIdByName("nop")));

          for (; pos != func.end(); ++pos)
          {
            pos->getDependencies().clear();
            addDeps(pos, func);
          }

          uint64_t bestNopTime = tryPerms(nopPerm, func, a, runtime, numLabels, from, to, verbose, overhead, arg1, arg2, arg3, arg4, arg5, arg6);
          if (bestNopTime < bestTime)
          {
            bestTime = bestNopTime;
            bestPerm = nopPerm;
          }
        }
      }

      free(mpn1);
      free(mpn2);
      free(mpn3);
      free(mpn4);

      return bestTime;
    }

    static int run(const char* file, int start, int end, const uint64_t limbs, const char* outFile, const int verbose, const int intelSyntax, const string signature, const int nopLine)
    {
      FileLogger logger(stdout);
      int numLabels = 0;

      logger.setIndentation("\t");
      logger.addOptions(Logger::kOptionGASFormat);

      // Create JitRuntime and X86 Assembler/Compiler.
      JitRuntime runtime;
      X86Assembler a(&runtime);
      if (verbose)
        a.setLogger(&logger);

      // Create the functions we will work with
      vector<Line> func;
      list<int> bestPerm;
      // load original from the file given in arguments
      numLabels = loadFuncFromFile(func, a, file, intelSyntax);

      // returned if something went wrong when loading
      if (numLabels == -1)
        exit(EXIT_FAILURE);

      if (start != 0)
        start--;
      else
        start = 0;
      if (end != 0)
        end--;
      else
        end = func.size() - 1;
      assert(end <= func.size() - 1);
      assert(start <= end);

      uint64_t bestTime = superOptimise(bestPerm, func, a, runtime, numLabels, start, end, limbs, verbose, signature, nopLine);

      list<int>::iterator startIt = bestPerm.begin();
      advance(startIt, start);
      list<int>::iterator endIt = bestPerm.begin();
      advance(endIt, end + 1);

      printf("# optimisation complete, best time of %lu for sequence:\n", bestTime);
      a.reset();
      a.setLogger(&logger);
      addFunc(func, bestPerm, a, numLabels, 1);
      printf("\n\n");

      // write output using asmjit's logger
      if (outFile != NULL)
      {
        a.reset();
        a.setLogger(&logger);
        FILE* of = fopen(outFile, "w");
        logger.setStream(of);
        addFunc(func, bestPerm, a, numLabels, 1);
        fclose(of);
      }

      return 0;
    }
};

int main(int argc, char* argv[])
{
  int c, start = 0, end = 0, limbs = 111, verbose = 0, nopLine = -1, intelSyntax = 0;
  char *outFile = NULL;
  char *inFile = NULL;
  string signature = "add_n";

  // deal with optional arguments: limbs and output file
  while (1) {
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    static struct option long_options[] = {
      {"limbs",     required_argument, 0,  0 },
      {"nop",       required_argument, 0,  0 },
      {"out",       required_argument, 0,  0 },
      {"range",     required_argument, 0,  0 },
      {"signature", required_argument, 0,  0 },
      {"verbose",   no_argument,       0,  0 },
      {"intel",     no_argument,       0,  0 },
      {0,           0,                 0,  0 }
    };

    c = getopt_long(argc, argv, "il:n:o:r:s:v",
        long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
      case 'l':
        limbs = std::strtol(optarg, NULL, 10);
        printf("# optimising for %s limbs\n", optarg);
        break;

      case 'n':
        nopLine = std::strtol(optarg, NULL, 10) - 1;
        printf("# inserting nops at line %s\n", optarg);
        break;

      case 'o':
        outFile = optarg;
        printf("# writing optimised function to %s\n", optarg);
        break;

      case 'r':
        {
          vector<string> range = ajs::split(optarg, '-');
          start = std::strtol(range[0].c_str(), NULL, 10);
          end = std::strtol(range[1].c_str(), NULL, 10);
          printf("# optimising range %d to %d\n", start, end);
        }
        break;

      case 's':
        signature = string(optarg);
        printf("# function signature %s\n", optarg);
        break;

      case 'i':
        intelSyntax = 1;
        printf("# assuming intel syntax\n");
        break;

      case 'v':
        verbose = 1;
        printf("# verbose mode on\n");
        break;
    }
  }

  if (argc >= 2)
  {
    inFile = argv[optind];
    printf("# source file: %s\n", inFile);
  }

  return ajs::run(inFile, start, end, limbs, outFile, verbose, intelSyntax, signature, nopLine);
}
