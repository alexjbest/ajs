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

#define regreg(N)  if (name == #N) return N
#define debug_print(fmt, ...) \
  do { fprintf(stderr, "%s:%d:%s(): " fmt, __FILE__, \
      __LINE__, __func__, __VA_ARGS__); } while (0)

#define stringify( x ) static_cast< std::ostringstream & >( \
            ( std::ostringstream() << std::dec << x ) ).str()

using namespace asmjit;
using namespace x86;
using namespace std;

/* TODO list:
 * ----------
 * check if for any other base for immediate values needed
 * extend parsing part to handle spaces well
 * check if there are other directives that will affect performance
 * check if there are other directives that will affect correctness
 * add capability for intel syntax
 * make repz ret work
 * make shr %r9 work
 * do registers more programmatically?
 * add help option, usage message etc.
 * add make dependencies for headers
 * remove originalIndex
 */


struct line {
  uint32_t instruction;
  asmjit::Operand ops[4];
  uint32_t label;
  uint32_t align;
  int originalIndex;
  vector<int> dependencies;
  vector<asmjit::X86Reg> regsIn;
  vector<asmjit::X86Reg> regsOut;
};

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
      std::size_t loc = val.find_first_of("+-\0", 1);
      if (loc != std::string::npos)
        return getVal(val.substr(0, loc)) + getVal(val.substr(loc));
      loc = val.find('x');
      if (loc != std::string::npos)
        return std::strtoll(val.c_str(), NULL, 16);
      return std::strtoll(val.c_str(), NULL, 10);
    }

    static Imm getValAsImm(string val) {
      return Imm(getVal(val));
    }

    static X86XmmReg getXmmRegFromName(string name) {
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
      assert(0);
      return xmm0;
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
      cout << name << endl;
      assert(0);
      return rax;
    }

    static X86Reg getRegFromName(string name) {
      if (name.at(0) == 'x')
        return getXmmRegFromName(name);
      return getGpRegFromName(name);
      assert(0);
      return rax;
    }

    // Parses expressions of the form disp(base,offset,scalar) into asmjits X86Mem
    static X86Mem getPtrFromAddress(string addr, uint32_t size)
    {
      size_t i = addr.find("(");
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

    static Operand getOpFromStr(string op, X86Assembler& a, map<string, Label>& labels, uint32_t size)
    {
      op = trim(op);

      if (count(op.begin(), op.end(), '(') > 0)
        return getPtrFromAddress(op, size);
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
      assert(0);
      return noOperand;
    }

    // returns whether or not b references a, i.e. they are equal regs or memory locations
    // or the memory location b references a if a is a register
    static bool references(const Operand& a, const Operand& b)
    {
      if (a.isReg() && b.isReg())
      {
        const X86Reg* areg = static_cast<const X86Reg*>(&a);
        const X86Reg* breg = static_cast<const X86Reg*>(&b);
        return *areg == *breg;
      }
          //&& a.getRegType() == b.getRegType()
         // && a.getRegIndex() == b.getRegIndex())
        //return true;
      if (b.isMem())
      {
        const X86Mem* bmem = static_cast<const X86Mem*>(&b);
        if (a.isMem())
        {
          const X86Mem* amem = static_cast<const X86Mem*>(&a);
          return *amem == *bmem;
        }
        else if (a.isReg())
        {
          const X86Reg* areg = static_cast<const X86Reg*>(&a);
          if (areg->getRegIndex() == bmem->getBase())
            return true;
          if (areg->getRegIndex() == bmem->getIndex())
            return true;
        }
      }

      return false;
    }

    // returns whether line a depends on line b or not
    // i.e. if a followed b originally whether swapping is permissible in general
    static bool dependsOn(const line& a, const line& b)
    {
      // labels and align statements should have all possible dependencies
      if (a.label != -1)
        return true;
      if (b.label != -1)
        return true;

      if (a.align != 0)
        return true;
      if (b.align != 0)
        return true;

      const X86InstInfo& ainfo = X86Util::getInstInfo(a.instruction),
            &binfo = X86Util::getInstInfo(b.instruction);

      // if a or b are control flow instructions there is always a dependency
      if (ainfo.getExtendedInfo().isFlow())
        return true;
      if (binfo.getExtendedInfo().isFlow())
        return true;

      if (ainfo.getExtendedInfo().isTest())
      {
        // if a is a test instruction there is a dependency on other test instructions
        if (binfo.getExtendedInfo().isTest())
          return true;

        // if a is a test instruction there is a dependency on arithmetic instructions
        if (binfo.getEncodingId() == kX86InstEncodingIdX86Arith)
          return true;
        // TODO only the last such instruction preceding a jmp etc should have such a dep
      }

      // if a reads flags set by b there is a dependency
      if (ainfo.getEFlagsIn() & binfo.getEFlagsOut())
        return true;

      // if a sets flags also set by b there is a dependency
      if (ainfo.getEFlagsOut() & binfo.getEFlagsOut())
        return true;

      // if a sets flags read by b there is a dependency
      if (ainfo.getEFlagsOut() & binfo.getEFlagsOut())
        return true;

      // a likely writes to a.ops[0] so if b reads or writes to a.op[0] there is a dep
      if (references(a.ops[0], b.ops[0]) || references(a.ops[0], b.ops[1])
          || references(a.ops[0], b.ops[2]) || references(a.ops[0], b.ops[3]))
        return true;

      // b likely writes to b.ops[0] so if a reads or writes to op[0] there is a dep,
      if (references(b.ops[0], a.ops[0]) || references(b.ops[0], a.ops[1])
          || references(b.ops[0], a.ops[2]) || references(b.ops[0], a.ops[3]))
        return true;

      return false;
    }

    static int loadFuncFromFile(vector<line>& func, X86Assembler& a, const char* file)
    {
      int index = 0;
      map<string, Label> labels;

      ifstream ifs;
      if (file != NULL)
        ifs.open(file);
      istream& is = file != NULL ? ifs : cin;
      string str;

      if (is.bad())
      {
        cout << "error: opening file failed, is filename correct?\n" << endl;
        return -1;
      }

      a.reset();

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
          line newLine;
          if (parsed[0].at(0) == '#') // first char of first token is '#' so have a comment
          {
            break;
          }
          else if (*parsed[0].rbegin() == ':') // last character of first token is colon, so we are at a label
          {
            string label = parsed[0].substr(0, parsed[0].size() - 1);
            if (labels.count(label) == 0)
              labels[label] = a.newLabel();

            newLine = (line){0, noOperand, noOperand, noOperand, noOperand,
              labels[label].getId(), 0, index++, vector<int>(), vector<X86Reg>(), vector<X86Reg>()};

            parsed.erase(parsed.begin());
          }
          else if (parsed[0].at(0) == '.') // first char of first token is '.' so have a directive
          {
            if (parsed[0] == ".align") {
              parsed.erase(parsed.begin());

              std::vector<std::string> args = split(parsed[0], ',');
              while (parsed.size() > 0) // done with this line
                parsed.erase(parsed.begin());

              newLine = (line){0, noOperand, noOperand, noOperand, noOperand, -1, getVal(args[0]), index++, vector<int>(), vector<X86Reg>(), vector<X86Reg>()};
            }
            else // ignore non-align directives
            {
              break;
            }
          }
          else // normal instruction
          {
            Operand ops[4];
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
            assert(id != kInstIdNone);
            parsed.erase(parsed.begin());

            int i = 0;
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

              reverse(args.begin(), args.end());

              for (i = 0; i < args.size(); i++)
                ops[i] = getOpFromStr(args[i], a, labels, size);
            }
            for (; i < 4; i++)
              ops[i] = noOperand;

            newLine = (line){id, ops[0], ops[1], ops[2], ops[3], -1, 0, index++, vector<int>(), vector<X86Reg>(), vector<X86Reg>()};
              newLine.regsIn.push_back(rax);
              newLine.regsIn.push_back(rax);
              newLine.regsIn.push_back(rax);
              newLine.regsIn.push_back(rax);
              newLine.regsIn.push_back(rax);
              newLine.regsIn.push_back(rax);
              newLine.regsIn.push_back(rax);
          }

          // check for dependencies annotated in the source
          if (parsed.size() > 0 && parsed[0].substr(0,5) == "#ajs:")
          {
            std::vector<std::string> deps = split(parsed[0].substr(5), ',');
            parsed.erase(parsed.begin());

            for (vector<string>::const_iterator ci = deps.begin(); ci != deps.end(); ++ci)
            {
              int newDep = atoi(ci->c_str()) - 1;
              assert(newDep < index); // make sure the user gave us a valid sequence
              newLine.dependencies.push_back(newDep);
            }
          }

          // try to determine other dependencies
          for (vector<line>::const_iterator prevLine = func.begin(); prevLine != func.end(); ++prevLine)
          {
            if (dependsOn(newLine, *prevLine))
            {
              if (find(newLine.dependencies.begin(), newLine.dependencies.end(), prevLine->originalIndex) == newLine.dependencies.end())
              {
                newLine.dependencies.push_back(prevLine->originalIndex);

                // we can now remove any of prevLine's dependencies from newLine
                // TODO does this make things faster?
                std::vector<int>::iterator position = newLine.dependencies.begin();
                for (vector<int>::const_iterator ci2 = prevLine->dependencies.begin(); ci2 != prevLine->dependencies.end(); ++ci2)
                {
                  position = std::find(position, newLine.dependencies.end(), *ci2);
                  if (position != newLine.dependencies.end())
                    position = newLine.dependencies.erase(position);
                }
              }
            }
          }

          func.insert(func.end(), newLine);
        }
      }
      a.reset();
      return labels.size();
    }

    static uint64_t callFunc(void* funcPtr, JitRuntime& runtime, uint64_t target, const int verbose, const uint64_t overhead,
        uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
    {
      uint32_t cycles_high, cycles_high1, cycles_low, cycles_low1;
      uint64_t start, end, total;
      const int loopsize = 10;
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
              printf("cannot hit target, aborting\n");
            break;
          }
        }
      }

      total /= k;

      if (verbose)
        printf("total time: %ld\n", total);

      runtime.release((void*)callableFunc);

      return total;
    }

    static void addFunc(vector<line>& func, list<int>& perm, X86Assembler& a, int numLabels)
    {
      Label labels[numLabels];
      for (int i = 0; i < numLabels; i++)
        labels[i] = a.newLabel();
      for (list<int>::const_iterator ci = perm.begin(); ci != perm.end(); ++ci)
      {
        line& curLine = func[*ci];
        if (curLine.align != 0) {
          a.align(kAlignCode, curLine.align);
        }
        if (curLine.label != -1) {
          a.bind(labels[curLine.label]);
        }
        if (curLine.instruction == 0)
          continue;
        for (int i = 0; i < 4; i++)
        {
          if (curLine.ops[i].isLabel()) {
            curLine.ops[i] = labels[curLine.ops[i].getId()];
          }
        }
        a.emit(curLine.instruction, curLine.ops[0], curLine.ops[1], curLine.ops[2], curLine.ops[3]);
      }
    }

    // makes a function with assembler then times the generated function with callFunc.
    static uint64_t timeFunc(vector<line>& func, list<int>& perm, X86Assembler& a, JitRuntime& runtime, int numLabels, uint64_t target, const int verbose, uint64_t overhead,
        uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
    {
      a.reset();

      addFunc(func, perm, a, numLabels);

      void* funcPtr = a.make();

      return callFunc(funcPtr, runtime, target, verbose, overhead,
          arg1, arg2, arg3, arg4, arg5, arg6);
    }

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
      else if (signature == "addlsh_n")
      {
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = reinterpret_cast<uint64_t>(mpn2);
        arg3 = reinterpret_cast<uint64_t>(mpn3);
        arg4 = reinterpret_cast<uint64_t>(mpn4);
        arg5 = reinterpret_cast<uint64_t>(limbs);
      }
      else
      {
        if (signature != "add_n")
          printf("signature not recognised, defaulting to add_n\n");
        arg1 = reinterpret_cast<uint64_t>(mpn1);
        arg2 = reinterpret_cast<uint64_t>(mpn2);
        arg3 = reinterpret_cast<uint64_t>(mpn3);
        arg4 = reinterpret_cast<uint64_t>(limbs);
      }
    }

    static list<int> superOptimise(vector<line>& func, X86Assembler& a, JitRuntime& runtime, int numLabels, const int from, const int to, const uint64_t limbs, const int verbose, string signature)
    {
      uint64_t bestTime = 0, overhead = 0;
      uint64_t *mpn1, *mpn2, *mpn3, *mpn4;
      uint64_t arg1, arg2, arg3, arg4, arg5, arg6;
      int count = 0;
      int level = 0;
      vector< list<int> > lines(to + 1 - from);
      vector< int > remaining(to + 2 - from);
      list<int> perm;

      for (int i = 0; i < func.size(); i++)
        perm.insert(perm.end(), i);

      // set up arguments for use by function
      mpn1 = (uint64_t*)malloc(limbs * sizeof(uint64_t));
      mpn2 = (uint64_t*)malloc(limbs * sizeof(uint64_t));
      mpn3 = (uint64_t*)malloc(limbs * sizeof(uint64_t));
      mpn4 = (uint64_t*)malloc(limbs * sizeof(uint64_t));

      getArgs(mpn1, mpn2, mpn3, mpn4, limbs, signature, arg1, arg2, arg3, arg4, arg5, arg6);

      line ret = (line){X86Util::getInstIdByName("ret"), noOperand, noOperand, noOperand, noOperand, -1, 0, 0, vector<int>(), vector<X86Reg>(), vector<X86Reg>()};
      vector<line> emptyFunc(1, ret);
      list<int> emptyPerm(1, 0);
      overhead = timeFunc(emptyFunc, emptyPerm, a, runtime, numLabels, bestTime, verbose, overhead,
          arg1, arg2, arg3, arg4, arg5, arg6);

      bestTime = timeFunc(func, perm, a, runtime, numLabels, bestTime, verbose, overhead,
          arg1, arg2, arg3, arg4, arg5, arg6);
      list<int> bestPerm = perm;
      printf("original sequence: %ld\n", bestTime);

      list<int>::iterator start = perm.begin();
      advance(start, from);
      list<int>::iterator end = perm.begin();
      advance(end, to + 1);

      for (list<int>::iterator i = start; i != end;)
      {
        line& curLine = func[*i];
        int depCount = 0;
        for (vector<int>::const_iterator ci = curLine.dependencies.begin(); ci != curLine.dependencies.end(); ++ci)
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
              line& freeLine = func[*ci];
              if (find(freeLine.dependencies.begin(), freeLine.dependencies.end(), *cur) != freeLine.dependencies.end())
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
              cout << endl << "timing sequence:" << endl;
            // time this permutation
            uint64_t newTime = timeFunc(func, perm, a, runtime, numLabels, bestTime, verbose, overhead,
                arg1, arg2, arg3, arg4, arg5, arg6);
            if (bestTime == 0 || newTime < bestTime)
            {
              cout << "better sequence found: " << newTime;
              if (bestTime != 0)
                cout << " delta: " << bestTime - newTime;
              cout << endl;
              bestPerm = perm;
              bestTime = newTime;
              for (list<int>::const_iterator ci = perm.begin(); ci != perm.end(); ++ci)
                cout << *ci + 1 << ", ";
              cout << endl;
            }
          }

          // update dependencies
          for (int i = to - from; i >= 0; i--)
          {
            for (list<int>::iterator ci = lines[i].begin(); ci != lines[i].end(); ++ci)
            {
              line& freeLine = func[*ci];
              if (find(freeLine.dependencies.begin(), freeLine.dependencies.end(), *cur) != freeLine.dependencies.end())
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

      printf("tried %d sequences\n", count);

      free(mpn1);
      free(mpn2);
      free(mpn3);
      free(mpn4);

      return bestPerm;
    }

    static int run(const char* file, int start, int end, const uint64_t limbs, const char* outFile, const int verbose, const string signature)
    {
      FileLogger logger(stdout);
      int numLabels = 0;

      logger.setIndentation("\t");

      // Create JitRuntime and X86 Assembler/Compiler.
      JitRuntime runtime;
      X86Assembler a(&runtime);
      if (verbose)
        a.setLogger(&logger);

      // Create the functions we will work with
      vector<line> func;
      list<int> bestPerm;
      // load original from the file given in arguments
      numLabels = loadFuncFromFile(func, a, file);

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
      bestPerm = superOptimise(func, a, runtime, numLabels, start, end, limbs, verbose, signature);

      list<int>::iterator startIt = bestPerm.begin();
      advance(startIt, start);
      list<int>::iterator endIt = bestPerm.begin();
      advance(endIt, end + 1);

      printf("optimisation complete, best sequence found for range %d-%d:\n", start + 1, end + 1);
      for (list<int>::const_iterator ci = startIt; ci != endIt; ++ci)
        printf("%d\n", *ci + 1);

      // write output using asmjits logger
      if (outFile != NULL)
      {
        a.reset();
        a.setLogger(&logger);
        FILE* of = fopen(outFile, "w");
        logger.setStream(of);
        addFunc(func, bestPerm, a, numLabels);
        fclose(of);
      }

      return 0;
    }
};

int main(int argc, char* argv[])
{
  int c, start = 0, end = 0, limbs = 111, verbose = 0;
  char *outFile = NULL;
  char *inFile = NULL;
  string signature = "add_n";

  // deal with optional arguments: limbs and output file
  while (1) {
    int this_option_optind = optind ? optind : 1;
    int option_index = 0;
    static struct option long_options[] = {
      {"limbs",     required_argument, 0,  0 },
      {"out",       required_argument, 0,  0 },
      {"range",     required_argument, 0,  0 },
      {"signature", required_argument, 0,  0 },
      {"verbose",   no_argument,       0,  0 },
      {0,           0,                 0,  0 }
    };

    c = getopt_long(argc, argv, "l:o:r:s:v",
        long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
      case 'l':
        limbs = std::strtol(optarg, NULL, 10);
        printf("using %s limbs\n", optarg);
        break;

      case 'o':
        outFile = optarg;
        printf("writing optimised function to %s\n", optarg);
        break;

      case 'r':
        {
          vector<string> range = ajs::split(optarg, '-');
          start = std::strtol(range[0].c_str(), NULL, 10);
          end = std::strtol(range[1].c_str(), NULL, 10);
          printf("using range %d to %d\n", start, end);
        }
        break;

      case 's':
        signature = string(optarg);
        printf("using function signature %s\n", optarg);
        break;

      case 'v':
        verbose = 1;
        break;
    }
  }

  if (argc >= 2)
    inFile = argv[optind];

  return ajs::run(inFile, start, end, limbs, outFile, verbose, signature);
}
