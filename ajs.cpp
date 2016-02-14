#include <asmjit/asmjit.h>
#include <iostream>
#include <fstream>
#include <algorithm> 
#include <functional> 
#include <cctype>
#include <locale>
#include <sstream>
#include <vector>

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

Imm getValAsImm(string name) // TODO deal with hex etc.
{
  return Imm(std::strtoll(name.c_str(), NULL, 10));
}

X86GpReg getRegFromName(string name) // TODO see if we can do this with asmjit magic
{
  if (name == "rdi") return rdi;
  if (name == "rsi") return rsi;
  if (name == "rdx") return rdx;
  if (name == "rcx") return rcx;
  if (name == "r8") return r8;
  if (name == "rax") return rax;
  return rax;
}

Operand getOpFromStr(string op)
{
  op = trim(op);
  string sub = op.substr(1);
  if (op.at(0) == '%')
    return getRegFromName(sub);
  if (op.at(0) == '$')
    return getValAsImm(sub);
}

void loadFuncFromFile(X86Assembler& a, char* file)
{
  ifstream is(file);
  string str;
  while(getline(is, str))
  {
    Operand op0, op1, op2, op3;
    Operand* ops[4] = { &op0, &op1, &op2, &op3 };
    str = trim(str);
    if (str.length() == 0)
      continue;
    std::vector<std::string> parsed = split(str, '\t'); //TODO use spaces here too

    // first character of first token is '.' so we are at a directive
    if (parsed[0].at(0) == '.')
    {
      continue; // TODO don't ignore all directives, align shouldn't be for one
    }

    // last character of first token is colon, so we are at a label
    if (*parsed[0].rbegin() == ':')
    {
      a.newLabel();
      parsed.erase(parsed.begin());
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
      reverse(args.begin(), args.end()); // TODO option for intel syntax
      for (i = 0; i < args.size(); i++)
        *ops[i] = getOpFromStr(args[i]);
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

  // In order to run 'funcPtr' it has to be casted to the desired type.
  // Typedef is a recommended and safe way to create a function-type.
  typedef int (*FuncType)(int, int, int, int);

  // Using asmjit_cast is purely optional, it's basically a C-style cast
  // that tries to make it visible that a function-type is returned.
  FuncType func = asmjit_cast<FuncType>(funcPtr);

  // Finally, run it and do something with the result...
  int z = func(1, 2, 3, 4);
  printf("z=%d\n", z); // Outputs "z=10".
  printf("%d\n", X86Util::getInstIdByName("add"));

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
