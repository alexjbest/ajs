#include <stdlib.h>
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
#include <csignal>
#include <getopt.h>
#include <sched.h>
#include <unistd.h>
#include "line.h"
#include "utils.h"

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

class ajs {

  public:
    static int exiting;
    static JitRuntime runtime;
    static X86Assembler assembler;
    static FileLogger logger;

    static int64_t getVal(string val) {
      val = trim(val);
      if (val.substr(1, 4) == "word")
        val = val.substr(5);
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

    // Parses expressions of the form disp(base,index,scalar) or
    // [base+index*scale+disp] into asmjit's X86Mem
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
          if (addr.at(i) == '+' || addr.at(i) == '-' || addr.at(i) == '*' ||
              addr.at(i) == ']')
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

    static Operand getOpFromStr(string op, map<string, Label>& labels,
        uint32_t size, int intelSyntax)
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
            labels[op] = assembler.newLabel();
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
              labels[op] = assembler.newLabel();
            return labels[op];
          }
        }
      }
      assert(0);
      return noOperand;
    }

    static vector<X86Reg> intersection(const vector<X86Reg>& v1, const
        vector<X86Reg>& v2)
    {
      vector<X86Reg> in;
      for (vector<X86Reg>::const_iterator ci1 = v1.begin(); ci1 != v1.end();
          ++ci1)
        for (vector<X86Reg>::const_iterator ci2 = v2.begin(); ci2 != v2.end();
            ++ci2)
          if (*ci1 == *ci2)
            in.push_back(*ci2);
      return in;
    }

    // returns whether line a depends on line b or not
    // i.e. if a followed b originally whether swapping is permissible in general
    static bool dependsOn(vector<Line>::const_iterator ai,
        vector<Line>::const_iterator bi, vector<Line>& func)
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

      // if a sets flags read by b there is a dependency
      if (ainfo.getEFlagsOut() & binfo.getEFlagsIn())
        return true;

      // if a sets flags also set by b there is possibly a dependency
      // unless another set happens before the next read of the same register
      if (ainfo.getEFlagsOut() & binfo.getEFlagsOut())
      {
        uint32_t flags = ainfo.getEFlagsOut() & binfo.getEFlagsOut();

        vector<Line>::const_iterator li;
        for (li = ai + 1; li != func.end(); ++li)
        {
          if (!li->isInstruction())
            continue;

          const X86InstInfo& liinfo = X86Util::getInstInfo(li->getInstruction());

          if (flags & liinfo.getEFlagsIn())
            return true;

          flags = flags - (flags & liinfo.getEFlagsOut());
        }
      }


      vector<X86Reg> in;

      // if a reads a register written to by b there is a dependency
      in = intersection(a.getRegsIn(), b.getRegsOut());
      // TODO poss change to if len
      for (vector<X86Reg>::const_iterator ci = in.begin(); ci != in.end(); ++ci)
      {
        return true;
      }

      // if a writes to a register read by b there is a dependency
      in = intersection(a.getRegsOut(), b.getRegsIn());
      for (vector<X86Reg>::const_iterator ci = in.begin(); ci != in.end(); ++ci)
      {
        return true;
      }

      // if a writes to a register written by b there is possibly a dependency
      // unless another write happens before the next read of the same register
      in = intersection(a.getRegsOut(), b.getRegsOut());
      for (vector<X86Reg>::const_iterator ci = in.begin(); ci != in.end(); ++ci)
      {
        vector<Line>::const_iterator li;
        for (li = ai + 1; li != func.end(); ++li)
        {
          if (std::find(li->getRegsIn().begin(), li->getRegsIn().end(), *ci) !=
              li->getRegsIn().end())
            return true;
          if (std::find(li->getRegsOut().begin(), li->getRegsOut().end(), *ci)
              != li->getRegsOut().end())
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
      if ((l.getInstruction() == X86Util::getInstIdByName("div"))
          || (l.getInstruction() == X86Util::getInstIdByName("idiv"))
          || (l.getInstruction() == X86Util::getInstIdByName("mul"))
          || (l.getInstruction() == X86Util::getInstIdByName("sahf")))
        l.addRegIn(rax);
      if ((l.getInstruction() == X86Util::getInstIdByName("div"))
          || (l.getInstruction() == X86Util::getInstIdByName("idiv"))
          || (l.getInstruction() == X86Util::getInstIdByName("mulx")))
        l.addRegIn(rdx);
      if ((l.getInstruction() == X86Util::getInstIdByName("pop"))
          || (l.getInstruction() == X86Util::getInstIdByName("push")))
        l.addRegIn(rsp);
      for (int i = 0; i < MAX_OPS; i++)
      {
        if (l.getOp(i).isReg())
        {
          if (i == 0)
          {
            const X86InstInfo& info = X86Util::getInstInfo(l.getInstruction());

            // Mov instructions do not read first op
            if (info.getExtendedInfo().isMove())
              continue;
            // Pop does not read first op
            if (l.getInstruction() == X86Util::getInstIdByName("pop"))
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
      {
        int opWritten = 1;
        // many instructions write to their first operand, but some do not
        if (l.getInstruction() == X86Util::getInstIdByName("push"))
          opWritten = 0;
        if (opWritten)
          l.addRegOut(*static_cast<const X86Reg*>(l.getOpPtr(0)));
      }
      if ((l.getInstruction() == X86Util::getInstIdByName("lahf"))
          || (l.getInstruction() == X86Util::getInstIdByName("mul"))
          || (l.getInstruction() == X86Util::getInstIdByName("div"))
          || (l.getInstruction() == X86Util::getInstIdByName("idiv")))
        l.addRegOut(rax);
      if ((l.getInstruction() == X86Util::getInstIdByName("mul"))
          || (l.getInstruction() == X86Util::getInstIdByName("div"))
          || (l.getInstruction() == X86Util::getInstIdByName("idiv")))
        l.addRegOut(rdx);
      if ((l.getInstruction() == X86Util::getInstIdByName("pop"))
          || (l.getInstruction() == X86Util::getInstIdByName("push")))
        l.addRegOut(rsp);
    }

    static void addDeps(vector<Line>::iterator newLine, vector<Line>& func)
    {
      // try to determine other dependencies
      int index = 0;
      for (vector<Line>::iterator prevLine = func.begin(); prevLine != newLine;
          ++prevLine, index++)
      {
        if (dependsOn(newLine, prevLine, func))
        {
          if (find(newLine->getDependencies().begin(),
                newLine->getDependencies().end(), index) ==
              newLine->getDependencies().end())
          {
            newLine->addDependency(index);

            // we can now remove any of prevLine's dependencies from newLine
            // TODO does this make things faster?
            std::vector<int>::iterator position =
              newLine->getDependencies().begin();
            for (vector<int>::iterator ci2 =
                prevLine->getDependencies().begin(); ci2 !=
                prevLine->getDependencies().end(); ++ci2)
            {
              position = std::find(position, newLine->getDependencies().end(), *ci2);
              if (position != newLine->getDependencies().end())
                position = newLine->getDependencies().erase(position);
            }
          }
        }
      }
    }

    static int loadFuncFromFile(vector<Line>& func, const
        char* file, const int intelSyntax)
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

      assembler.reset();

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
              labels[label] = assembler.newLabel();

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
            if (parsed[0] == "ASM_START") // TODO this probably shouldn't be in any files at all...
              break;
            if (parsed[0] == "end") // a fake instruction sometimes in nasm syntax files, ignored
              break;

            if (parsed[0].substr(0, 8) == "prefetch") // prefetch instructions should be given extra arguments for asmjit
            {
              char c = parsed[0].at(9) + 1;
              if (c == 't' + 1) // prefetchnta
                c = '0';
              parsed[1] = (string("$") + c + ',') + parsed[1];
              parsed[0] = "prefetch";
            }
            if (parsed[0] == "jrcxz" || parsed[0] == "jecxz") // add extra arg to jr/ecx instrs
            {
              parsed[1] = parsed[0].substr(1,3) + ',' + parsed[1];
              parsed[0] = "jecxz";
            }
            if (0 && parsed[0] == "movq")
              parsed[0] = "mov";
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
                newLine.setOp(i, getOpFromStr(args[i], labels, size, intelSyntax));
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

      assembler.reset();

      for (vector<Line>::iterator i = func.begin(); i != func.end(); ++i)
        addDeps(i, func);

      return labels.size();
    }

    static int comp(const void * a, const void * b)
    {
      return *(int*)a - *(int*)b;
    }

    static double callFunc(void* funcPtr, uint64_t target,
        const int verbose, const double overhead, uint64_t arg1,
        uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
    {
      uint32_t cycles_high, cycles_high1, cycles_low, cycles_low1;
      uint64_t start, end;
      double total;
      const int loopsize = 1, trials = 60;
      volatile int k = 0;

      // In order to run 'funcPtr' it has to be casted to the desired type.
      // Typedef is a recommended and safe way to create a function-type.
      typedef int (*FuncType)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
          uint64_t);

      // Using asmjit_cast is purely optional, it's basically a C-style cast
      // that tries to make it visible that a function-type is returned.
      FuncType callableFunc = asmjit_cast<FuncType>(funcPtr);
      int times[trials];

      total = -1;
      for (int i = 0; i < trials; i++)
      {
        int curTotal = 0;
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
          curTotal += end - start - overhead;

          if (0 && target != 0 && k >= loopsize >> 1 && curTotal > (target + 20) * (k + 1))
          {
            if (verbose)
              printf("# cannot hit target, aborting\n");
            if (total != -1)
              curTotal = total + 1;
            break;
          }
        }

        curTotal /= k;
        times[i] = curTotal;
      }

      qsort(times, trials, sizeof(int), comp);
      for (int i = 0; i < trials - 5; i++)
        total += times[i];
      total /= ((double)trials - 5.0L);

      if (verbose)
        printf("# total time: %lf\n", total);

      ajs::runtime.release((void*)callableFunc);

      return total;
    }

    static void addFunc(vector<Line>& func, list<int>& perm, int numLabels,
        int verbose)
    {
      Label labels[numLabels];
      for (int i = 0; i < numLabels; i++)
        labels[i] = assembler.newLabel();
      for (list<int>::const_iterator ci = perm.begin(); ci != perm.end(); ++ci)
      {
        Line& curLine = func[*ci];
        if (curLine.isAlign()) {
          assembler.align(kAlignCode, curLine.getAlign());
        }
        if (curLine.isLabel()) {
          assembler.bind(labels[curLine.getLabel()]);
          numLabels--;
        }
        if (curLine.isByte()) {
          uint8_t* cursor = assembler.getCursor();
          if ((size_t)(assembler._end - cursor) < 16)
          {
            assembler._grow(16);
          }
          cursor[0] = curLine.getByte();
          cursor += 1;
          if (verbose)
            assembler.getLogger()->logFormat(Logger::kStyleDefault,"\t.byte\t%d\n", curLine.getByte());
        }
        if (!curLine.isInstruction())
          continue;
        for (int i = 0; i < MAX_OPS; i++)
        {
          if (curLine.getOp(i).isLabel()) {
            curLine.setOp(i, labels[curLine.getOp(i).getId()]);
          }
        }
        assembler.emit(curLine.getInstruction(), curLine.getOp(0), curLine.getOp(1),
            curLine.getOp(2));
      }
      if (numLabels > 0)
      {
        printf("error: %d label(s) not bound, are all label names correct?\n", numLabels);
        exit(EXIT_FAILURE);
      }
    }

    // makes a function with assembler then times the generated function with callFunc.
    static double timeFunc(vector<Line>& func, list<int>& perm,
        int numLabels, uint64_t target, const int verbose, double overhead,
        uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,
        uint64_t arg5, uint64_t arg6)
    {
      double times[2] = {-1, -1};
      do {
        for (int i = 0; i < 2; i++)
        {
          assembler.reset();

          addFunc(func, perm, numLabels, verbose);

          void* funcPtr = assembler.make();
          times[i] = callFunc(funcPtr, target, verbose, overhead,
              arg1, arg2, arg3, arg4, arg5, arg6);

        }
      }
      while (times[0] - times[1] > 0.25L);

      if (times[1] < times[0])
        times[0] = times[1];

      return times[0];
    }

    // sets arg1-6 based on signature using: mpn1-3 (of length limbs),
    // mpn4 (of length 2*limbs), and limbs itself
    // base on Linux/System V AMD64 ABI call order:
    // arg1, arg2, arg3, arg4, arg5, arg6
    //  RDI,  RSI,  RDX,  RCX,   R8,   R9
    static void getArgs(uint64_t *mpn1, uint64_t *mpn2, uint64_t *mpn3,
        uint64_t *mpn4, const uint64_t limbs, string signature, uint64_t& arg1,
        uint64_t& arg2, uint64_t& arg3, uint64_t& arg4, uint64_t& arg5,
        uint64_t& arg6)
    {
      if (signature == "double")
      {
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = limbs;
      }
      else if (signature == "store")
      {
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = limbs;
        arg3 = (uint64_t) 123124412;
      }
      else if (signature == "com_n")
      {
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = reinterpret_cast<uint64_t>(mpn2);
        arg3 = limbs;
      }
      else if (signature == "lshift")
      {
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = reinterpret_cast<uint64_t>(mpn2);
        arg3 = limbs;
        arg4 = (uint64_t)31;
      }
      else if (signature == "addadd_n")
      {
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = reinterpret_cast<uint64_t>(mpn2);
        arg3 = reinterpret_cast<uint64_t>(mpn3);
        arg4 = reinterpret_cast<uint64_t>(mpn4);
        arg5 = limbs;
      }
      else if (signature == "addlsh_n")
      {
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = reinterpret_cast<uint64_t>(mpn2);
        arg3 = reinterpret_cast<uint64_t>(mpn3);
        arg4 = limbs;
        arg5 = (uint64_t)31;
      }
      else if (signature == "addmul_1")
      {
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = reinterpret_cast<uint64_t>(mpn2);
        arg3 = limbs;
        arg4 = (uint64_t)14412932479013124615ULL;
      }
      else if (signature == "addmul_2")
      {
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = reinterpret_cast<uint64_t>(mpn2);
        arg3 = limbs;
        arg4 = reinterpret_cast<uint64_t>(mpn3);
      }
      else if (signature == "mul_basecase")
      {
        arg1 = reinterpret_cast<uint64_t>(mpn4);
        arg2 = reinterpret_cast<uint64_t>(mpn1);
        arg3 = limbs;
        arg4 = reinterpret_cast<uint64_t>(mpn2);
        arg5 = limbs;
      }
      else if (signature == "sqr_basecase")
      {
        arg1 = reinterpret_cast<uint64_t>(mpn4);
        arg2 = reinterpret_cast<uint64_t>(mpn1);
        arg3 = limbs;
      }
      else
      {
        if (signature != "add_n")
          printf("# signature not recognised, defaulting to add_n\n");
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = reinterpret_cast<uint64_t>(mpn2);
        arg3 = reinterpret_cast<uint64_t>(mpn3);
        arg4 = limbs;
      }
    }

    static double tryPerms(list<int>& bestPerm, vector<Line>& func,
        const int numLabels, const int from, const int to, const int verbose,
        const uint64_t overhead, uint64_t arg1, uint64_t arg2, uint64_t arg3,
        uint64_t arg4, uint64_t arg5, uint64_t arg6)
    {
      int count = 0, level = 0;
      vector< list<int> > lines(to + 1 - from);
      vector< int > remaining(to + 2 - from);
      list<int> perm;

      for (int i = 0; i < func.size(); i++)
        perm.insert(perm.end(), i);


      double bestTime = timeFunc(func, perm, numLabels, 0,
          verbose, overhead, arg1, arg2, arg3, arg4, arg5, arg6);
      bestPerm = perm;
      printf("# original sequence: %lf\n", bestTime);

      list<int>::iterator start = perm.begin();
      advance(start, from);
      list<int>::iterator end = perm.begin();
      advance(end, to + 1);

      for (list<int>::iterator i = start; i != end;)
      {
        Line& curLine = func[*i];
        int depCount = 0;
        for (vector<int>::const_iterator ci = curLine.getDependencies().begin();
            ci != curLine.getDependencies().end(); ++ci)
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
        if (exiting)
          break;
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
            double newTime = timeFunc(func, perm, numLabels,
                bestTime, verbose, overhead, arg1, arg2, arg3, arg4, arg5, arg6);
            if (bestTime == 0 || bestTime - newTime > 0.25L)
            {
              printf("# better sequence found: %lf", newTime);
              if (bestTime != 0)
                printf(" delta: %lf", bestTime - newTime);
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
            for (list<int>::iterator ci = lines[i].begin();
                ci != lines[i].end(); ++ci)
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

    static double superOptimise(list<int>& bestPerm, vector<Line>& func,
        const int numLabels, const int from, const int to, const uint64_t limbs,
        const int verbose, string signature, int nopLine = -1)
    {
      double bestTime = 0, overhead = 0;
      uint64_t *mpn1, *mpn2, *mpn3, *mpn4;
      uint64_t arg1, arg2, arg3, arg4, arg5, arg6;

      // set up arguments for use by function
      mpn1 = (uint64_t*)malloc((limbs + 1) * sizeof(uint64_t));
      mpn2 = (uint64_t*)malloc(limbs * sizeof(uint64_t));
      mpn3 = (uint64_t*)malloc(limbs * sizeof(uint64_t));
      // double size mpn, e.g. for output of mpn_mul
      mpn4 = (uint64_t*)malloc(2 * limbs * sizeof(uint64_t));

      getArgs(mpn1, mpn2, mpn3, mpn4, limbs, signature, arg1, arg2, arg3, arg4,
          arg5, arg6);

      list<int> idPerm;

      for (int i = 0; i < func.size(); i++)
        idPerm.insert(idPerm.end(), i);

      // 'warm up' the processor?
      for (int i = 0; i < 20000 && !exiting; i++)
        timeFunc(func, idPerm, numLabels, 0, 0, 0, arg1, arg2,
            arg3, arg4, arg5, arg6);

      // set logger if we are in verbose mode
      if (verbose)
        assembler.setLogger(&logger);

      Line ret(X86Util::getInstIdByName("ret"));
      vector<Line> emptyFunc(1, ret);
      list<int> emptyPerm(1, 0);
      overhead = timeFunc(emptyFunc, emptyPerm, 0,
          bestTime, verbose, overhead, arg1, arg2, arg3, arg4, arg5, arg6);

      bestTime = tryPerms(bestPerm, func, numLabels, from,
          to, verbose, overhead, arg1, arg2, arg3, arg4, arg5, arg6);

      // optionally add nops and time again
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

          double bestNopTime = tryPerms(nopPerm, func,
              numLabels, from, to, verbose, overhead, arg1, arg2, arg3, arg4,
              arg5, arg6);
          if (bestNopTime < bestTime)
          {
            bestTime = bestNopTime;
            bestPerm = nopPerm;
          }
        }
      }

      // remove nops that were not helpful
      while (func.size() > bestPerm.size())
      {
        vector<Line>::iterator pos = func.begin();
        pos += nopLine;
        pos = func.erase(pos);
      }

      free(mpn1);
      free(mpn2);
      free(mpn3);
      free(mpn4);

      return bestTime;
    }

    // Gets the start and end index of the ith loop of func
    static void getLoopRange(int& start, int& end, int i, vector<Line> func)
    {
      end = 0;
      for (vector<Line>::const_iterator ci = func.begin(); ci != func.end();
          ++ci, end++) {
        if (ci->isInstruction()) {
          const X86InstInfo& info = X86Util::getInstInfo(ci->getInstruction());

          if (info.getExtendedInfo().isFlow()) {
            if (ci->getOp(0).isLabel()) {
              start = 1;
              for (vector<Line>::const_iterator ci2 = func.begin(); ci2 != ci; ++ci2, start++) {
                if (ci2->isLabel() && ci2->getLabel() == ci->getOp(0).getId()) {
                  i--;
                  if (i == 0)
                    return;
                }
              }
            }
          }
        }
      }
      exit(EXIT_FAILURE);
    }

    static int run(const char* file, int start, int end, const uint64_t limbs,
        const char* outFile, const int verbose, const int intelSyntax, const
        string signature, const int nopLine, const int loop)
    {
      int numLabels = 0;

      logger.setIndentation("\t");
      logger.addOptions(Logger::kOptionGASFormat);

      // Create the functions we will work with
      vector<Line> func;
      list<int> bestPerm;
      // load original from the file given in arguments
      numLabels = loadFuncFromFile(func, file, intelSyntax);

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

      if (loop)
        getLoopRange(start, end, loop, func);

      if ((end > func.size() - 1) || (start > end))
      {
        printf("error: invalid range (function is %lu lines long)\n", func.size());
        exit(EXIT_FAILURE);
      }

      double bestTime = superOptimise(bestPerm, func,
          numLabels, start, end, limbs, verbose, signature, nopLine);

      list<int>::iterator startIt = bestPerm.begin();
      advance(startIt, start);
      list<int>::iterator endIt = bestPerm.begin();
      advance(endIt, end + 1);

      printf("# optimisation complete, best time of %lf for sequence:\n",
          bestTime);
      assembler.reset();
      assembler.setLogger(&logger);
      addFunc(func, bestPerm, numLabels, 1);
      printf("\n\n");

      // write output using asmjit's logger
      if (outFile != NULL)
      {
        assembler.reset();
        assembler.setLogger(&logger);
        FILE* of = fopen(outFile, "w");
        logger.setStream(of);
        logger.logFormat(Logger::kStyleComment, "# This file was produced by ajs, the MPIR assembly superoptimiser\n");
        logger.logFormat(Logger::kStyleComment, "# %lf cycles/%lu limbs\n", bestTime, limbs);
        addFunc(func, bestPerm, numLabels, 1);
        fclose(of);
      }

      return 0;
    }
};

int ajs::exiting = 0;
// Create JitRuntime and X86 Assembler/Compiler.
JitRuntime ajs::runtime;
X86Assembler ajs::assembler(&runtime);
FileLogger ajs::logger(stdout);

void sig_handler(int signo)
{
  if (signo == SIGINT)
    ajs::exiting = true;
}

/* prints a nice usage/help message */
void display_usage()
{
  printf(
"Usage: ajs [options] [filename]                                              \n"
"  If a filename is not specified ajs attempts to read its input from stdin   \n"
"Options:                                                                     \n"
"  --cpu <number>          Run on cpu <number>                                \n"
"  --help                  Display this message                               \n"
"  --intel                 Parse input with Intel/YASM parser                 \n"
"  --limbs <number>        Use mpns with <number> limbs when optimising       \n"
"  --loop <number>         Optimise loop <number> only (overrides range)      \n"
"  --nop <number>          Additionally try adding nops at line <number>      \n"
"  --out <file>            Write the output to <file> as well as stdout       \n"
"  --range <start>-<end>   Only superoptimise the lines <start> to            \n"
"                          <end> (inclusive)                                  \n"
"  --signature <signature> Give the function inputs of the format <signature>,\n"
"                          where the possible signatures are as follows       \n"
"                            double:       mpn, length                        \n"
"                            store:        mpn, length, value (123124412)     \n"
"                            com_n:        mpn, mpn, length                   \n"
"                            lshift:       mpn, mpn, length, shift (31)       \n"
"                            add_n:        mpn, mpn, mpn, length              \n"
"                            addadd_n:     mpn, mpn, mpn, mpn, length         \n"
"                            addlsh_n:     mpn, mpn, mpn, length, shift (31)  \n"
"                            addmul_1:     mpn, mpn, length, multiplier       \n"
"                            addmul_2:     mpn, mpn, length, mpn (length 2)   \n"
"                            mul_basecase: mpn, mpn, length, mpn, length      \n"
"                          If no signature is specified add_n is used         \n"
"  --verbose               Print out all sequences tried                      \n"
"                                                                             \n"
"(abbreviations can be used e.g. --sig or just -s (with a single -))          \n"
"                                                                             \n"
"Examples:                                                                    \n"
"  Basic usage:            ajs test.asm                                       \n"
"  Specifying output file: ajs test.asm -o test_optimised.asm                 \n"
"  Intel syntax mode:      ajs -i test.as                                     \n"
"  Signature selection:    ajs --sig=double half.asm                          \n"
"  Range selection:        ajs -r 1-2 four_line_file.asm                      \n"
"  Debugging:              ajs test.asm | as                                  \n"
"                          ajs -v test.asm                                    \n"
"                          ajs                                                \n"
"                          > add    %%rax,%%rax                               \n"
"                          > ret                                              \n"
"                          > <Ctrl-D>                                         \n"
"  Piping input:           m4 test.asm | ajs -o test_optimised.asm            \n"
"  Use with gcc:           gcc -S -O3 tst.c -o tst.s                          \n"
"                            && ajs tst.s -o tst_optimised.s                  \n"
      );
}

int main(int argc, char* argv[])
{
  int c, start = 0, end = 0, limbs = 111, verbose = 0, nopLine = -1,
      intelSyntax = 0, loop = 0, cpunum = -1;
  char *outFile = NULL;
  char *inFile = NULL;
  string signature = "add_n";
  cpu_set_t cpuset;

  if (signal(SIGINT, sig_handler) == SIG_ERR)
    printf("warning: can't catch SIGINT\n");

  // deal with command line options

  int this_option_optind = optind ? optind : 1;
  int option_index = 0;
  static struct option long_options[] = {
    {"cpu",       required_argument, 0, 'c'},
    {"help",      no_argument,       0, 'h'},
    {"intel",     no_argument,       0, 'i'},
    {"limbs",     required_argument, 0, 'l'},
    {"loop",      required_argument, 0, 'p'},
    {"nop",       required_argument, 0, 'n'},
    {"out",       required_argument, 0, 'o'},
    {"range",     required_argument, 0, 'r'},
    {"signature", required_argument, 0, 's'},
    {"verbose",   no_argument,       0, 'v'},
    {0,           0,                 0, 0  }
  };

  while ((c = getopt_long(argc, argv, "c:hil:p:n:o:r:s:v",
        long_options, &option_index)) != -1) {

    switch (c) {
      case '?':
        printf("Unrecognised option\n");
      case 'h':
        display_usage();
        return 0;

      case 'c':
        cpunum = std::strtol(optarg, NULL, 10);
        printf("# using cpu %d\n", cpunum);
        break;

      case 'i':
        intelSyntax = 1;
        printf("# assuming intel syntax\n");
        break;

      case 'l':
        limbs = std::strtol(optarg, NULL, 10);
        if (limbs == 0)
        {
          printf("# error: number of limbs not recognised, falling back to default\n");
          limbs = 111;
        }
        printf("# optimising for %d limbs\n", limbs);
        break;

      case 'p':
        loop = std::strtol(optarg, NULL, 10);
        if (limbs == 0)
          printf("# error: loop index not recognised, not using\n");
        printf("# optimising loop %d\n", loop);
        break;

      case 'n':
        nopLine = std::strtol(optarg, NULL, 10) - 1;
        if (nopLine == -1)
          printf("# error: NOT inserting nops\n");
        else
          printf("# inserting nops at line %d\n", nopLine);
        break;

      case 'o':
        outFile = optarg;
        printf("# writing optimised function to %s\n", outFile);
        break;

      case 'r':
        {
          vector<string> range = split(optarg, '-');
          start = std::strtol(range[0].c_str(), NULL, 10);
          if (start == 0)
            printf("# error: start line not recognised, using first line\n");
          end = std::strtol(range[1].c_str(), NULL, 10);
          if (end == 0)
            printf("# error: end line not recognised, using last line\n");
          printf("# optimising range %d to %d\n", start, end);
        }
        break;

      case 's':
        signature = string(optarg);
        printf("# function signature %s\n", optarg);
        break;

      case 'v':
        verbose = 1;
        printf("# verbose mode on\n");
        break;
    }
  }

  if (argc - optind >= 1) {
    inFile = argv[optind];
    printf("# source file: %s\n", inFile);
  }

  if (cpunum != -1) {
    CPU_ZERO(&cpuset);
    CPU_SET(cpunum, &cpuset);
    sched_setaffinity(getpid(), sizeof(cpuset), &cpuset);
  }

  return ajs::run(inFile, start, end, limbs, outFile, verbose, intelSyntax,
      signature, nopLine, loop);
}
