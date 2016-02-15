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

using namespace asmjit;
using namespace x86;
using namespace std;

std::vector<std::string> &split(const std::string &s, char delim, std::vector<std::string> &elems) {
  std::stringstream ss(s);
  std::string item;
  while (std::getline(ss, item, delim)) {
    elems.push_back(item);
  }
  return elems;
}

std::vector<std::string> split(const std::string &s, char delim) {
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

Imm getValAsImm(string val) // TODO deal with hex etc.
{
  return Imm(std::strtoll(val.c_str(), NULL, 10));
}

X86GpReg getGpRegFromName(string name)
{
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

X86Reg getRegFromName(string name) // TODO see if we can do this with asmjit magic
{
  if (name.at(0) == 'r')
    return getGpRegFromName(name);
  if (name == "xmm0") return xmm0;
  if (name == "xmm1") return xmm1;
  if (name == "xmm2") return xmm2;
  if (name == "xmm3") return xmm3;
  return rax;
}

// Parses expressions of the form disp(base,offset,scalar) into asmjits X86Mem
X86Mem getPtrFromAddress(string addr)
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
    uint32_t shift = (scalar == 1) ? 0 : 
      (scalar == 2) ? 1 : 
      (scalar == 4) ? 2 : 
      (scalar == 8) ? 3 : -1;
    if (trim(bis[0]).length() == 0) // no base register TODO
      return ptr(base, index, shift, disp);
    return ptr(base, index, shift, disp);
  }
  if (trim(bis[0]).length() == 0) // no base register TODO
    return ptr(base, disp);
  return ptr(base, disp);
}

Operand getOpFromStr(string op, std::map<string, Label>& labels, X86Assembler& a)
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

void loadFuncFromFile(X86Assembler& a, char* file)
{
  std::map<string, Label> labels;

  ifstream is(file);
  string str;
  while(getline(is, str))
  {
    Operand op0, op1, op2, op3;
    Operand* ops[4] = { &op0, &op1, &op2, &op3 };
    str = trim(str);
    if (str.length() == 0)
      continue;
    std::vector<string> parsed = split(str, '\t'); //TODO use spaces here too

    // last character of first token is colon, so we are at a label
    if (*parsed[0].rbegin() == ':')
    {
      string label = parsed[0].substr(0, parsed[0].size() - 1);
      if (labels.count(label) == 0)
        labels[label] = a.newLabel();
      a.bind(labels[label]);

      parsed.erase(parsed.begin());
    }
    else if (parsed[0].at(0) == '.') // first char of first token is '.' so have a directive
    {
      continue; // TODO don't ignore all directives, align shouldn't be for one
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
      reverse(args.begin(), args.end()); // TODO option for intel syntax
      for (i = 0; i < args.size(); i++)
      {
        *ops[i] = getOpFromStr(args[i], labels, a);
      }
    }
    for (; i < 4; i++)
      *ops[i] = Operand();
    a.emit(X86Util::getInstIdByName(parsed[0].c_str()), op0, op1, op2, op3);
  }
}

int main(int argc, char* argv[]) {

  if (argc < 2)
  {
    cout << "error: expected filename" << endl;
    return 1;
  }
  FileLogger logger(stdout);

  // Create JitRuntime and X86 Assembler/Compiler.
  JitRuntime runtime;
  X86Assembler a(&runtime);
  a.setLogger(&logger);

  loadFuncFromFile(a, argv[1]);

  // After finalization the code has been send to `Assembler`. It contains
  // a handy method `make()`, which returns a pointer that points to the
  // first byte of the generated code, which is the function entry in our
  // case.
  // case.
  void* funcPtr = a.make();

  cout << "make successful" << endl;

  // In order to run 'funcPtr' it has to be casted to the desired type.
  // Typedef is a recommended and safe way to create a function-type.
  typedef int (*FuncType)(uint64_t*, uint64_t*, uint64_t*, uint64_t);

  // Using asmjit_cast is purely optional, it's basically a C-style cast
  // that tries to make it visible that a function-type is returned.
  FuncType func = asmjit_cast<FuncType>(funcPtr);

  uint64_t o, b, c;
  o = 0;
  b = 8;
  c = 24;
  // Finally, run it and do something with the result...
  int z = func(&o, &b, &c, 1);
  printf("z=%d\n", z);
  printf("o=%lu\n", o); // Outputs "o=8" for and_n.

  // The function will remain in memory after Compiler and Assembler are
  // destroyed. This is why the `JitRuntime` is used - it keeps track of
  // the code generated. When `Runtime` is destroyed it also invalidates
  // all code relocated by it (which is in our case also our `func`). So
  // it's safe to just do nothing in our case, because destroying `Runtime`
  // will free `func` as well, however, it's always better to release the
  // generated code that is not needed anymore manually.
  runtime.release((void*)func);

  return 0;
}
