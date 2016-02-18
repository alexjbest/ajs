#include <asmjit/asmjit.h>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <functional>
#include <cctype>
#include <locale>
#include <sstream>
#include <vector>
#include <map>
#include <assert.h>
#include <list>

using namespace asmjit;
using namespace x86;
using namespace std;

/* TODO list:
 * ----------
 * check if for any other base for immediate values needed
 * see if there is some way to do getRegByName in asmjit
 * add support for there being no base register in getPtrFromAddress
 * extend parsing part to handle spaces well
 * check if there are other directives that will affect performance
 * add capability for intel syntax
 */


struct line {
  uint32_t instruction;
  asmjit::Operand ops[4];
  uint32_t label;
  uint32_t align;
  int originalIndex;
  vector<int> dependsOn;
};

class ajs {

  private:
    static bool cmp(const line &a, const line &b) {
      return a.originalIndex < b.originalIndex;
    }

    static std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
      std::stringstream ss(s);
      std::string item;
      while (std::getline(ss, item, delim)) {
        elems.push_back(item);
      }
      return elems;
    }

    static std::vector<std::string> split(const std::string &s, char delim) {
      std::vector<std::string> elems;
      split(s, delim, elems);
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
      if (count(val.begin(), val.end(), 'x') > 0)
        return std::strtoll(val.c_str(), NULL, 16);
      return std::strtoll(val.c_str(), NULL, 10);
    }

    static Imm getValAsImm(string val) {
      return Imm(getVal(val));
    }

    static X86GpReg getGpRegFromName(string name) {
      if (name == "rdi") return rdi;
      if (name == "rsi") return rsi;
      if (name == "rdx") return rdx;
      if (name == "rcx") return rcx;
      if (name == "r8") return r8;
      if (name == "r9") return r9;
      if (name == "r10") return r10;
      if (name == "r11") return r11;
      if (name == "rax") return rax;
      return rax;
    }

    static X86Reg getRegFromName(string name) {
      if (name.at(0) == 'r')
        return getGpRegFromName(name);
      if (name == "xmm0") return xmm0;
      if (name == "xmm1") return xmm1;
      if (name == "xmm2") return xmm2;
      if (name == "xmm3") return xmm3;
      return rax;
    }

    // Parses expressions of the form disp(base,offset,scalar) into asmjits X86Mem
    static X86Mem getPtrFromAddress(string addr)
    {
      size_t i = addr.find("(");
      uint32_t disp = 0;
      if (i > 0)
        disp = std::strtoul(addr.substr(0, i).c_str(), NULL, 10);
      vector<string> bis = split(addr.substr(i + 1, addr.size() - 2), ',');
      X86GpReg base;
      if (trim(bis[0]).length() != 0) // no base register
        base = getGpRegFromName(trim(bis[0]).substr(1));
      if (bis.size() > 1)
      {
        X86GpReg index = getGpRegFromName(trim(bis[1]).substr(1));
        uint32_t scalar;
        if (bis[2].length() != 0)
          scalar = std::strtoul(bis[2].c_str(), NULL, 10);
        else
          scalar = 1;
        uint32_t shift =
          (scalar == 1) ? 0 :
          (scalar == 2) ? 1 :
          (scalar == 4) ? 2 :
          (scalar == 8) ? 3 : -1;
        if (trim(bis[0]).length() == 0)
          return ptr(base, index, shift, disp);
        return ptr(base, index, shift, disp);
      }
      if (trim(bis[0]).length() == 0)
        return ptr(base, disp);
      return ptr(base, disp);
    }

    static Operand getOpFromStr(string op, X86Assembler& a, map<string, Label>& labels)
    {
      op = trim(op);

      if (count(op.begin(), op.end(), '(') > 0)
        return getPtrFromAddress(op);
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
      return Operand();
    }

    static int loadFuncFromFile(list<line>& func, X86Assembler& a, char* file)
    {
      int index = 0;
      map<string, Label> labels;

      ifstream is(file);
      string str;
      while(getline(is, str))
      {
        Operand ops[4] = { Operand(), Operand(), Operand(), Operand() };
        str = trim(str);
        if (str.length() == 0)
          continue;
        std::vector<string> parsed = split(str, '\t');

        // last character of first token is colon, so we are at a label
        if (*parsed[0].rbegin() == ':')
        {
          string label = parsed[0].substr(0, parsed[0].size() - 1);
          if (labels.count(label) == 0)
            labels[label] = a.newLabel();

          func.insert(func.end(), (line){0, ops[0], ops[1], ops[2], ops[3], labels[label].getId(), 0, index++});

          parsed.erase(parsed.begin());
        }
        else if (parsed[0].at(0) == '.') // first char of first token is '.' so have a directive
        {
          if (parsed[0] == ".align")
          {
            std::vector<std::string> args = split(parsed[1], ',');
            func.insert(func.end(), (line){0, ops[0], ops[1], ops[2], ops[3], -1, getVal(args[0]), index++});
          }
          continue;
        }

        // try to deal with all manner of whitespace between label and instruction
        while (parsed.size() != 0 && trim(parsed[0]).length() == 0)
        {
          parsed.erase(parsed.begin());
        }
        if (parsed.size() == 0)
          continue;

        int i = 0;
        if (parsed.size() > 1)
        {
          std::vector<std::string> args = split(parsed[1], ',');

          // stick bracketed expressions back together again
          for (int j = 0; j < args.size(); j++)
          {
            string arg = args[j];
            if (count(arg.begin(), arg.end(), '(') != count(arg.begin(), arg.end(), ')'))
            {
              assert(j < args.size() - 1);
              args[j] = args[j] + "," + args[j + 1];
              args.erase(args.begin() + j+1);
              j--;
            }
          }

          reverse(args.begin(), args.end());

          for (i = 0; i < args.size(); i++)
          {
            ops[i] = getOpFromStr(args[i], a, labels);
          }
        }
        for (; i < 4; i++)
          ops[i] = Operand();

        vector<int> dependsOn;
        if (parsed.size() > 2)
        {
          if (parsed[2].substr(0,5) == ";ajs:")
          {
            std::vector<std::string> deps = split(parsed[2].substr(5), ',');
            for (vector<string>::const_iterator ci = deps.begin(); ci != deps.end(); ++ci)
            {
              dependsOn.push_back(atoi(ci->c_str()) - 1);
            }
          }
        }

        func.insert(func.end(), (line){X86Util::getInstIdByName(parsed[0].c_str()), ops[0], ops[1], ops[2], ops[3], -1, 0, index++, dependsOn});
      }
      a.reset();
      return labels.size();
    }

    static uint64_t callFunc(void* funcPtr, JitRuntime& runtime)
    {
      // In order to run 'funcPtr' it has to be casted to the desired type.
      // Typedef is a recommended and safe way to create a function-type.
      typedef int (*FuncType)(uint64_t*, uint64_t*, uint64_t*, uint64_t);
      //typedef int (*FuncType)(uint64_t, uint64_t*, uint64_t, uint64_t);

      // Using asmjit_cast is purely optional, it's basically a C-style cast
      // that tries to make it visible that a function-type is returned.
      FuncType callableFunc = asmjit_cast<FuncType>(funcPtr);

      uint32_t cycles_high, cycles_high1, cycles_low, cycles_low1;
      uint64_t o, b, c, start, end, total;
      //int64_t z;
      o = 0;
      b = 8;
      c = 24;
      total = 0;
      uint64_t limbs = 100;
      uint64_t *mpn1, *mpn2, *mpn3;
      mpn1 = (uint64_t*)malloc(limbs * sizeof(uint64_t));
      mpn2 = (uint64_t*)malloc(limbs * sizeof(uint64_t));
      mpn3 = (uint64_t*)malloc(limbs * sizeof(uint64_t));

      for (int k = 0; k < 100; k++)
      {
        uint64_t z = callableFunc(mpn1, mpn2, mpn3, limbs);
        //z += callableFunc(2, &b, 1, 4);
      }

      for (int k = 0; k < 1000; k++)
      {
        asm volatile (
            "CPUID\n\t"
            "RDTSC\n\t"
            "mov %%edx, %0\n\t"
            "mov %%eax, %1\n\t":
            "=r" (cycles_high), "=r" (cycles_low)::
            "%rax", "%rbx", "%rcx", "%rdx");

        uint64_t z = callableFunc(mpn1, mpn2, mpn3, limbs);
        //z += callableFunc(4, &b, 3, 4);

        // end timing
        asm volatile(
            "RDTSCP\n\t"
            "mov %%edx, %0\n\t"
            "mov %%eax, %1\n\t"
            "CPUID\n\t" :
            "=r" (cycles_high1), "=r" (cycles_low1) ::
            "%rax", "%rbx", "%rcx", "%rdx");
        start = ( ((uint64_t)cycles_high << 32) | (uint64_t)cycles_low );
        end = ( ((uint64_t)cycles_high1 << 32) | (uint64_t)cycles_low1 );
        total += end - start;
      }

      //printf("z=%ld\n", z);
      printf("total time=%ld\n", total);
      //printf("o=%lu\n", o); // Outputs "o=8" for and_n.

      free(mpn1);
      free(mpn2);
      free(mpn3);
      runtime.release((void*)callableFunc);

      return total;
    }

    static uint64_t timeFunc(list<line>& func, X86Assembler& a, JitRuntime& runtime, int numLabels)
    {
      Label labels[numLabels];
      for (int i = 0; i < numLabels; i++)
        labels[i] = a.newLabel();
      for (list<line>::const_iterator ci = func.begin(); ci != func.end(); ++ci)
      {
        line curLine = *ci;
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

      void* funcPtr = a.make();

      // cout << "make successful" << endl;
      uint64_t ret = callFunc(funcPtr, runtime);
      a.reset();

      return ret;
    }

    static list<line> superOptimise(list<line>& func, X86Assembler& a, JitRuntime& runtime, int numLabels, const int from, const int to)
    {
      uint64_t bestTime = -1;
      int count = 0;
      list<line> bestFunc;

      list<line>::iterator start = func.begin();
      advance(start, from);
      list<line>::iterator end = func.begin();
      advance(end, to + 1);

      do {
        count ++;
        cout << "sequence " << count<< endl;
        // time this permutation
        uint64_t newTime = timeFunc(func, a, runtime, numLabels);
        if (newTime < bestTime)
        {
          cout << "better sequence found: " << newTime << " delta: " << bestTime - newTime << endl;
          bestFunc = func;
          bestTime = newTime;
          for (list<line>::const_iterator ci = func.begin(); ci != func.end(); ++ci)
            cout << ci->originalIndex << endl;
        }
      } while(std::next_permutation(start, end, *cmp));
      cout << count << " sequences tried" << endl;

      return bestFunc;
    }

  public:
    static int run(char* file, int start, int end) {
      FileLogger logger(stdout);
      int numLabels = 0;

      // Create JitRuntime and X86 Assembler/Compiler.
      JitRuntime runtime;
      X86Assembler a(&runtime);
      //a.setLogger(&logger);

      // Create the function we will work with
      list<line> func, bestFunc;
      // load it from the file given in arguments
      numLabels = loadFuncFromFile(func, a, file);

      bestFunc = superOptimise(func, a, runtime, numLabels, start, end);

      cout << "optimisation complete, best sequence found:" << endl;
      for (list<line>::const_iterator ci = bestFunc.begin(); ci != bestFunc.end(); ++ci)
      {
        //cout << ci->instruction << endl;
        cout << ci->originalIndex << endl;
      }

      return 0;
    }
};

int main(int argc, char* argv[]) {

  if (argc < 4)
  {
    cout << "error: expected filename, start index, end index (inclusive)" << endl;
    return 1;
  }
  return ajs::run(argv[1], std::strtol(argv[2], NULL, 10), std::strtol(argv[3], NULL, 10));
}
