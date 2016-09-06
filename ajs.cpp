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
#include "gmp.h"
#include "gmp-impl.h"
#include "longlong.h"
#include "line.h"
#include "transform.h"
#include "utils.h"
#include "rdtsc.h"
#include "ajs_parsing.h"
#include "eval.h"

#include "config.h"


#define debug_print(fmt, ...) \
  do { fprintf(stderr, "# %s:%d:%s(): " fmt, __FILE__, \
      __LINE__, __func__, __VA_ARGS__); } while (0)

#define stringify( x ) static_cast< std::ostringstream & >( \
            ( std::ostringstream() << std::dec << x ) ).str()


using namespace asmjit;
using namespace x86;
using namespace std;


// In order to run 'funcPtr' it has to be cast to the desired type.
// Typedef is a recommended and safe way to create a function-type.
typedef int (*FuncType)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
    uint64_t);

static void repeat_func_call(FuncType callableFunc, uint64_t arg1,
		uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
#if 0
    /* Optionally don't inline for easier break-point setting */
    __attribute__((noinline));
#else
    ;
#endif

static void repeat_func_call(FuncType callableFunc, uint64_t arg1,
		uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
{
#ifdef REPEATS
	for (unsigned long r = 0; r < REPEATS; r++)
		callableFunc(arg1, arg2, arg3, arg4, arg5, arg6);
#else
    callableFunc(arg1, arg2, arg3, arg4, arg5, arg6);
#endif
}


class ajs {

  public:
    static int exiting; // boolean: whether or not ajs should be stopping, (set by interupt handler)
    static JitRuntime runtime;
    static X86Assembler assembler;
    static FileLogger logger;

    static Imm getValAsImm(string val) {
      return Imm(getVal(val));
    }

    // Parses expressions of the form disp(base,index,scalar) or
    // [base+index*scale+disp] into asmjit's X86Mem
    static X86Mem getPtrFromAddress(string addr, uint32_t const size,
            const int intelSyntax)
    {
      if (!intelSyntax) // GAS syntax
      {
        size_t i = addr.find('(');
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
        return parse_pointer_intel(addr, size);
      }
    }

    // returns the operand represented by a string i.e. an immediate value,
    // label, register, or memory location
    static Operand getOpFromStr(string op, map<string, Label>& labels,
        map<string, int>& useCounts, uint32_t size, int intelSyntax)
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
          {
            labels[op] = assembler.newLabel();
            useCounts[op] = 0;
          }
          useCounts[op]++;
          return labels[op];
        }
      }
      else // Intel syntax
      {
        X86Reg reg = getRegFromName(op);
        if (reg != noGpReg) // Is it a register?
          return reg;
        // Is it an immediate?
        const char *op_str = op.c_str(), *endp;
        int32_t imm_value = eval(op_str, &endp);
        if (false) {
            cout << "# Evaluating \"" << op << "\" produced " << imm_value
                    << " with rest \"" << endp << "\"" << endl;
        }
        if (endp == op_str + strlen(op_str)) {
            return Imm(imm_value);
        }
        // If we parsed a number other than 0 and text remains on the line,
        // signal a parsing error
        if (imm_value != 0) {
            cout << "Error parsing \"" << op << "\"" << endl;
            abort();
        }
        if (op.length() > 0) // No, it's a label!
        {
            if (labels.count(op) == 0)
            {
                labels[op] = assembler.newLabel();
                useCounts[op] = 0;
            }
            useCounts[op]++;
            return labels[op];
        }
      }
      assert(0);
      return noOperand;
    }

    // returns the intersection of two vectors of registers
    static vector<X86Reg> intersection(const vector<X86Reg>& v1,
        const vector<X86Reg>& v2)
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

    // returns whether Line a depends on Line b or not
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

      // if a or b are marked as volatile there is always a dependency
      if (ainfo.getExtendedInfo().hasFlag(kX86InstFlagVolatile))
        return true;
      if (binfo.getExtendedInfo().hasFlag(kX86InstFlagVolatile))
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

    // adds the registers read from by a Line to Line
    // there will not be any for non-instruction lines
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

            // Some instructions do not read first op
            if (info.getExtendedInfo().isWO())
              continue;
            // Pop does not read first op TODO: this is probably covered by above now, check
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

    // adds the registers written to by a Line
    // there will not be any for non-instruction lines
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
      // mulx writes to two outputs
      if (l.getInstruction() == X86Util::getInstIdByName("mulx"))
        l.addRegOut(*static_cast<const X86Reg*>(l.getOpPtr(1)));
      // xchg writes to both outputs
      if (l.getInstruction() == X86Util::getInstIdByName("xchg") && l.getOp(1).isReg())
        l.addRegOut(*static_cast<const X86Reg*>(l.getOpPtr(1)));
    }

    // if Line1 and Line2 can be transforming swapped adds the corresponding
    // Transform to transforms and returns 1
    // Currently only lea/mov instructions can be transformed with add/sub/inc/dec
    static int addTransformBy(Line& line1, int index1, Line& line2, int index2,
        vector<Transform>& transforms)
    {
      if (!line1.isInstruction() || !line2.isInstruction())
        return 0;
      X86Mem* mem = nullptr;
      if (line1.getInstruction() == X86Util::getInstIdByName("lea"))
        mem = static_cast<X86Mem*>(line1.getOpPtr(1));
      if (line1.getInstruction() == X86Util::getInstIdByName("mov"))
      {
        if (line1.getOp(0).isMem())
          mem = static_cast<X86Mem*>(line1.getOpPtr(0));
        else if (line1.getOp(1).isMem())
          mem = static_cast<X86Mem*>(line1.getOpPtr(1));
      }
      if (mem != nullptr)
      {
        int32_t base = 0;
        if (line2.getInstruction() == X86Util::getInstIdByName("inc"))
          base = 1;
        if (line2.getInstruction() == X86Util::getInstIdByName("dec"))
          base = -1;
        if (line2.getInstruction() == X86Util::getInstIdByName("add") &&
        	line2.getOpPtr(1)->isImm())
          base = static_cast<Imm*>(line2.getOpPtr(1))->getInt32();
        if (line2.getInstruction() == X86Util::getInstIdByName("sub") &&
        	line2.getOpPtr(1)->isImm())
          base = -static_cast<Imm*>(line2.getOpPtr(1))->getInt32();

        if (base == 0)
          return 0;

        if (index1 < index2)
          base = -base;

        int32_t adjustment = 0;
        X86GpReg* reg = static_cast<X86GpReg*>(line2.getOpPtr(0));
        if (mem->getBase() == reg->getRegIndex())
          adjustment += base;
        if (mem->getIndex() == reg->getRegIndex())
          adjustment += base << mem->getShift();

        if (adjustment != 0)
        {
          transforms.insert(transforms.end(), Transform(index1, index2, Transform::changeDisp, adjustment));
          return 1;
        }
      }
      return 0;
    }

    // add dependency and transform info to lines in func
    static void addDepsAndTransforms(vector<Line>& func, vector<Transform>& transforms)
    {
      int index2 = 0;
      for (vector<Line>::iterator line2 = func.begin(); line2 != func.end();
          ++line2, index2++)
      {
        // try to determine other dependencies
        int index1 = 0;
        for (vector<Line>::iterator line1 = func.begin(); line1 != line2;
            ++line1, index1++)
        {
          if (addTransformBy(*line1, index1, *line2, index2, transforms))
            continue;
          if (addTransformBy(*line2, index2, *line1, index1, transforms))
            continue;
          if (dependsOn(line2, line1, func))
          {
            if (find(line2->getDependencies().begin(),
                  line2->getDependencies().end(), index1) ==
                line2->getDependencies().end())
            {
              line2->addDependency(index1);

              // we can now remove any of prevLine's dependencies from newLine
              // TODO does this make things faster?
              std::vector<int>::iterator position =
                line2->getDependencies().begin();
              for (vector<int>::iterator ci2 =
                  line1->getDependencies().begin(); ci2 !=
                  line1->getDependencies().end(); ++ci2)
              {
                position = std::find(position, line2->getDependencies().end(), *ci2);
                if (position != line2->getDependencies().end())
                  position = line2->getDependencies().erase(position);
              }
            }
          }
        }
      }
    }

    static void removeTransforms(vector<Transform>& transforms, int index)
    {
      for (vector<Transform>::iterator i = transforms.begin(); i != transforms.end(); ++i)
      {
        //vector<int>& v = i->getDependencies();
        //v.erase(std::remove_if(v.begin(), v.end(), std::bind1st(std::equal_to<int>(), index)), v.end());
      }
    }

    static void removeDeps(vector<Line>::iterator start, vector<Line>::iterator end,
        int index)
    {
      for (vector<Line>::iterator i = start; i != end; ++i)
      {
        vector<int>* v = &(i->getDependencies());
        v->erase(std::remove(v->begin(), v->end(), index), v->end());
      }
    }

    static void shiftDeps(vector<Line>::iterator start, vector<Line>::iterator end,
        int index, int shift)
    {
      for (vector<Line>::iterator i = start; i != end; ++i)
      {
        for (vector<int>::iterator dep = i->getDependencies().begin(); dep != i->getDependencies().end(); ++dep)
        {
          if (*dep >= index)
            (*dep) += shift;
        }
      }
    }

    static void shiftTransforms(vector<Transform>& transforms, int index, int shift)
    {
      for (vector<Transform>::iterator i = transforms.begin(); i != transforms.end(); ++i)
      {
        if (i->a >= index)
          (i->a) += shift;
        if (i->b >= index)
          (i->b) += shift;
      }
    }

    // core parsing function, loads lines from either the filename given in
    // imput or stdin if this is null and converts them to Lines which are
    // returned in func
    static int loadFunc(vector<Line>& func, const char* input,
        const int intelSyntax, vector<Transform>& transforms, int removeLabels)
    {
      map<string, Label> labels; // map of label names to Label objects
      map<string, int> useCounts;
      map<int, vector<int>> depGroups;

      // if we are given a file path use it, otherwise try stdin.
      ifstream ifs;
      if (input != NULL)
        ifs.open(input);
      istream& is = input != NULL ? ifs : cin;
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
            {
              labels[label] = assembler.newLabel();
              useCounts[label] = 0;
            }

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
            if (parsed[0] == "GLOBAL_FUNC") // TODO this probably shouldn't be in any files at all...
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
              if (intelSyntax) {
                parsed[1] = parsed[0].substr(1, 3) + ',' + parsed[1];
              }
              else {
                parsed[1] = parsed[1] + ",%" + parsed[0].substr(1, 3);
              }
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
              for (size_t j = 0; j < args.size(); j++)
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

              // if we have shr/shl with only a single argument add the argument 1 for asmjit
              if (args.size() == 1 && (id == X86Util::getInstIdByName("shr") ||
                    id == X86Util::getInstIdByName("shl")))
                args.insert(args.end(), intelSyntax ? "1" : "$1");

              for (size_t i = 0; i < args.size(); i++)
                newLine.setOp(i, getOpFromStr(args[i], labels, useCounts, size, intelSyntax));
            }

            addRegsRead(newLine);
            addRegsWritten(newLine);
          }

          // check for dependencies annotated in the source
          if (parsed.size() > 0 && parsed[0].size() > 5 && parsed[0].at(0) == commentChar && parsed[0].substr(1,4) == "ajs:")
          {
            std::vector<std::string> deps = split(parsed[0].substr(5), ',');
            parsed.erase(parsed.begin());

            for (vector<string>::const_iterator ci = deps.begin(); ci != deps.end(); ++ci)
            {
              int group = atoi(ci->c_str());
              if (depGroups.count(group) == 0)
                depGroups[group] = vector<int>();
              for (vector<int>::iterator dep = depGroups[group].begin(); dep != depGroups[group].end(); ++dep)
                newLine.addDependency(*dep);
              depGroups[group].insert(depGroups[group].end(), func.size());
            }
          }
          if (!newLine.isValid()) {
              cout << "Error parsing: " << str << endl;
              assert(newLine.isValid());
          }
          func.insert(func.end(), newLine);
        }
      }

      assembler.reset();

      // Remove unused labels
      for (map<string, Label>::iterator i = labels.begin(); removeLabels &&
          i != labels.end(); ++i)
      {
        if (useCounts[i->first] == 0)
        {
          int index = 0;
          vector<Line>::iterator j = func.begin();
          for (; j != func.end(); index++, ++j)
          {
            if (j->isLabel() && (j->getLabel() == i->second.getId()))
            {
              j = func.erase(j);
              break;
            }
          }

          removeDeps(j, func.end(), index);

          shiftDeps(j, func.end(), index, -1);
        }
      }

      addDepsAndTransforms(func, transforms);

      return labels.size();
    }

    // integer compare
    static int comp(const void * a, const void * b)
    {
      return *(int*)a - *(int*)b;
    }

    // calls the function given by funcPtr and returns the approximate number
    // of cycles taken
    static double callFunc(void* funcPtr, uint64_t target,
        const int verbose, const double overhead, uint64_t arg1,
        uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
    {
      double total;
      volatile int k = 0;


      // Using asmjit_cast is purely optional, it's basically a C-style cast
      // that tries to make it visible that a function-type is returned.
      FuncType callableFunc = asmjit_cast<FuncType>(funcPtr);
      int times[TRIALS];

      total = -1;
      for (int i = 0; i < TRIALS; i++)
      {
        times[i] = 0;
        for (k = 0; k < LOOPSIZE; k++)
        {
          start_timing();
          repeat_func_call(callableFunc, arg1, arg2, arg3, arg4, arg5, arg6);
          end_timing();

          const uint64_t diff = get_diff_timing();
          if (diff == 0 || diff < overhead) {
              printf("Timing resulted in %lu cycles with %f overhead\n", diff, overhead);
          } else {
        	  // printf("Timing resulted in %llu cycles with %f overhead\n", diff, overhead);
        	  times[i] = diff - overhead;
          }

          /*if (0 && target != 0 && k >= LOOPSIZE >> 1 && curTotal > (target + 20) * (k + 1))
          {
            if (verbose)
              printf("# cannot hit target, aborting\n");
            if (total != -1)
              curTotal = total + 1;
            break;
          }*/
        }

      }

      qsort(times, TRIALS, sizeof(int), comp);
#ifdef STRATEGY_MIN
      int prev = times[0];
      int i, diffs;
      for (i = 0; (verbose || target % 10 == -1) && i < TRIALS; i++)
        cout << times[i] << " ";
      if (verbose || target % 10 == -1)
        cout <<endl;
      for (i = 0, diffs = 0; i < TRIALS; i++)
      {
        if (prev != times[i])
        {
          diffs++;
          if (diffs == 2 || prev + 1 < times[i])
            break;
          prev = times[i];
        }
        total += times[i];
      }
      total /= ((double)i);
#else
      total = times[TRIALS/10];
#endif

      if (verbose)
        printf("# total time: %lf\n", total);

      static double last_total = 0.;
      static unsigned long in_a_row = 0;

      if (total == last_total && in_a_row < 1000) {
          in_a_row++;
      } else {
          cout << "# Had " << last_total << " cycles " << in_a_row <<" times in a row." << endl;
          fflush(stdout);
          last_total = total;
          in_a_row = 1;
          cout << "# New timings:";
          print_histogram(times, TRIALS);
          cout << endl;
      }

      ajs::runtime.release((void*)callableFunc);

      return total;
    }

    // adds the function func with the permutation perm applied to the X86Assembler
    static void addFunc(vector<Line>& func, list<int>& perm, int numLabels, vector<Transform>& transforms)
    {
      Label labels[numLabels];
      for (int i = 0; i < numLabels; i++)
        labels[i] = assembler.newLabel();
      for (list<int>::iterator ci = perm.begin(); ci != perm.end(); ++ci)
      {
        Line curLine = func[*ci];
        for (vector<Transform>::const_iterator ti = transforms.begin(); ti != transforms.end(); ++ti)
        {
          if (*ci == ti->a) // possibly have a transform to make
          {
            if ((find(perm.begin(), ci, ti->b) != ci) ^ (ti->b < ti->a)) // have a transform to make
              ti->apply(curLine);
          }
        }
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
            cursor = assembler.getCursor();
          }
          cursor[0] = curLine.getByte();
          cursor += 1;
          if (assembler.hasLogger())
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
      if (0 && numLabels > 0)
      {
        printf("error: %d label(s) not bound, are all label names correct?\n", numLabels);
        exit(EXIT_FAILURE);
      }
    }

    // makes a callable function using assembler then times the generated function with callFunc.
    static double timeFunc(vector<Line>& func, list<int>& perm,
        int numLabels, uint64_t target, const int verbose, double overhead, vector<Transform>& transforms,
        uint64_t arg1, uint64_t arg2, uint64_t arg3, uint64_t arg4,
        uint64_t arg5, uint64_t arg6)
    {
      double times[2] = {-1, -1};
      do {
        for (int i = 0; i < 2; i++)
        {
          assembler.reset();

          addFunc(func, perm, numLabels, transforms);
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
        uint64_t& arg6, mp_limb_t* db, mp_limb_t* rem)
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
      else if (signature.substr(0, 6) == "mod_1_")
      {
        mp_size_t j, k = signature.at(6) - '0';
        mp_limb_t dummy, i, c, ds, d = 5806679768680879695ULL;

        count_leading_zeros(c, d);
        ds = d << c;

        invert_limb(i, ds);

        udiv_qrnnd_preinv(dummy, db[0], ((mp_limb_t) 1)<<c, 0, ds, i);  /* this is B%ds */

        for (j = 1; j <= k; j++)
        {
          udiv_qrnnd_preinv(dummy, db[j], db[j - 1], 0, ds, i);
          db[j - 1]>>=c;
        }

        /* now db[j] = B^j % d */

        db[k]>>=c;

        arg1 = reinterpret_cast<uint64_t>(rem);
        arg2 = reinterpret_cast<uint64_t>(mpn2);
        arg3 = limbs;
        arg4 = reinterpret_cast<uint64_t>(db);
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
        const uint64_t overhead, const int maxPerms, vector<Transform>& transforms, uint64_t arg1,
        uint64_t arg2, uint64_t arg3, uint64_t arg4, uint64_t arg5, uint64_t arg6)
    {
      int count = 0, level = 0;
      vector< list<int> > lines(to + 1 - from);
      vector< int > remaining(to + 2 - from);
      list<int> perm;

      for (size_t i = 0; i < func.size(); i++)
        perm.insert(perm.end(), i);


      double bestTime = timeFunc(func, perm, numLabels, 0,
          verbose, overhead, transforms, arg1, arg2, arg3, arg4, arg5, arg6);
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

      while (level >= 0 && (maxPerms == 0 || count < maxPerms))
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
            if (verbose >= 2)
              printf("\n# timing sequence:\n");
            // time this permutation
            double newTime = timeFunc(func, perm, numLabels,
                count, verbose, overhead, transforms, arg1, arg2, arg3, arg4, arg5, arg6);
            if (newTime > 0 && (bestTime == 0 || bestTime - newTime > 0.25L))
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
              if (find(freeLine.getDependencies().begin(),
                    freeLine.getDependencies().end(), *cur) !=
                  freeLine.getDependencies().end())
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

    static size_t round_up(const size_t s, const size_t m)
    {
    	assert(m > 0);
    	if (s == 0)
    		return 0;
    	return ((s - 1) / m + 1) * m;
    }

    // core superoptimise function, takes a function as a list of Lines, to and
    // from indexes, limb count and signature (and optionally a line in which
    // to insert up to 3 nops) and returns the valid reordering of func that
    // executes in the least time.
    static double superOptimise(list<int>& bestPerm, vector<Line>& func,
        const int numLabels, int from, int to, const uint64_t
        limbs, const int verbose, string signature, vector<Transform>&
        transforms, int nopLine = -1, const int maxPerms = 0)
    {
      double bestTime = 0, overhead = 0;
      uint64_t *mpn1, *mpn2, *mpn3, *mpn4;
      uint64_t arg1 = 0, arg2 = 0, arg3 = 0, arg4 = 0, arg5 = 0, arg6 = 0;

      // set up arguments for use by function
      const size_t size1 = round_up((limbs + 1) * sizeof(uint64_t), 16),
              size2 = round_up(limbs * sizeof(uint64_t), 16),
              size3 = round_up(limbs * sizeof(uint64_t), 16),
              size4 = round_up(2 * limbs * sizeof(uint64_t), 16),
              size_total = size1 + size2 + size3 + size4;

      mpn1 = (uint64_t*)aligned_alloc(4096, size_total);
      mpn2 = mpn1 + size1 / sizeof(uint64_t);
      mpn3 = mpn2 + size2 / sizeof(uint64_t);
      // rest is a double size mpn, e.g. for output of mpn_mul
      mpn4 = mpn3 + size3 / sizeof(uint64_t);
      mp_size_t k = 1;
      if (signature.substr(0, 6) == "mod_1_")
        k = signature.at(6) - '0';
      mp_limb_t db[k + 1], rem[k + 1];

      getArgs(mpn1, mpn2, mpn3, mpn4, limbs, signature, arg1, arg2, arg3, arg4,
          arg5, arg6, db, rem);

      list<int> idPerm;

      for (size_t i = 0; i < func.size(); i++)
        idPerm.insert(idPerm.end(), i);

      // 'warm up' the processor?
      if (WARMUP_LENGTH > 0) {
          printf("# Warming up the processor\n");
      }
      for (int i = 0; i < WARMUP_LENGTH && !exiting; i++)
        timeFunc(func, idPerm, numLabels, 0, 0, 0, transforms, arg1, arg2,
            arg3, arg4, arg5, arg6);

      // set logger if we have verbosity at least 2
      if (verbose >= 2)
        assembler.setLogger(&logger);

      printf("# Getting timing for empty function\n");
      Line ret(X86Util::getInstIdByName("ret"));
      vector<Line> emptyFunc(1, ret);
      list<int> emptyPerm(1, 0);
      overhead = timeFunc(emptyFunc, emptyPerm, 0,
          bestTime, verbose, overhead, transforms, arg1, arg2, arg3, arg4, arg5, arg6);
      printf("# overhead = %f\n", overhead);

      bestTime = tryPerms(bestPerm, func, numLabels, from, to, verbose,
          overhead, maxPerms, transforms, arg1, arg2, arg3, arg4, arg5, arg6);

      // optionally add nops and time again
      list<int> nopPerm;
      if (nopLine != -1)
      {
        for (int i = 0; i < 2; i++)
        {
          printf("# trying %d nop(s)\n", i + 1);
          vector<Line>::iterator pos = func.begin();
          pos += nopLine;
          pos = func.insert(pos, Line(X86Util::getInstIdByName("nop")));

          shiftDeps(pos, func.end(), nopLine, 1);
          shiftTransforms(transforms, nopLine, 1);

          if (nopLine >= from && nopLine <= to)
            to++;

          double bestNopTime = tryPerms(nopPerm, func,
              numLabels, from, to, verbose, overhead, maxPerms, transforms, arg1, arg2,
              arg3, arg4, arg5, arg6);
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

      return bestTime;
    }

    // Gets the start and end line index of the ith loop of func
    static void getLoopRange(int& start, int& end, int i, const vector<Line>& func)
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
        const char* outFile, const int verbose, const int intelSyntax,
        const string signature, const int nopLine, const int loop,
        const string prepend, const string append, const int maxPerms,
        const int removeLabels, const int includeLeadIn)
    {
      int numLabels = 0;

      logger.setIndentation("\t");
      logger.addOptions(Logger::kOptionGASFormat);

#ifdef USE_INTEL_PCM
      m = PCM::getInstance();
      m->resetPMU();
      // program counters, and on a failure just exit
      if (m->program() != PCM::Success)
      {
        cout<< m->program() <<endl;
        m->cleanup();
        exit(0);
      }
#endif

      // Create the functions we will work with
      vector<Line> func;
      list<int> bestPerm;
      vector<Transform> transforms;
      // load original from the file given in arguments
      numLabels = loadFunc(func, file, intelSyntax, transforms, removeLabels);

      // returned if something went wrong when loading
      if (numLabels == -1)
        exit(EXIT_FAILURE);

      if (start != 0)
        start--;

      if (end != 0)
        end--;
      else
        end = func.size() - 1;

      if (loop)
      {
        getLoopRange(start, end, loop, func);
        if (includeLeadIn)
          start = 0;
      }

      if ((end + 1 > func.size()) || (start > end))
      {
        printf("error: invalid range (function is %lu lines long)\n", func.size());
        exit(EXIT_FAILURE);
      }


      double bestTime = superOptimise(bestPerm, func,
          numLabels, start, end, limbs, verbose, signature, transforms, nopLine, maxPerms);

      list<int>::iterator startIt = bestPerm.begin();
      advance(startIt, start);
      list<int>::iterator endIt = bestPerm.begin();
      advance(endIt, end + 1);

      printf("# optimisation complete, best time of %lf for sequence:\n",
          bestTime);
      assembler.reset();
      assembler.setLogger(&logger);
      addFunc(func, bestPerm, numLabels, transforms);
      printf("\n\n");

      // write output using asmjit's logger
      if (outFile != NULL)
      {
        assembler.reset();
        assembler.setLogger(&logger);
        FILE* of = fopen(outFile, "w");
        if (of == NULL) {
            perror("Error opening output file: ");
            exit(1);
        }
        logger.setStream(of);
        logger.logFormat(Logger::kStyleComment,
            "# This file was produced by ajs, the MPIR assembly superoptimiser\n");
        logger.logFormat(Logger::kStyleComment,
            "# %lf cycles/%lu limbs\n", bestTime, limbs);
        logger.logFormat(Logger::kStyleComment, "%s\n", prepend.c_str());
        addFunc(func, bestPerm, numLabels, transforms);
        logger.logFormat(Logger::kStyleComment, "%s\n", append.c_str());
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
"Usage: ajs [options] [filename]                                               \n"
"  If a filename is not specified ajs attempts to read its input from stdin    \n"
"Options:                                                                      \n"
"  -h/--help               Display this message                                \n"
"  -c/--cpu <number>       Run on cpu <number>                                 \n"
"  -i/--intel              Parse input with Intel/YASM parser                  \n"
"  -l/--limbs <number>     Use mpns with <number> limbs when optimising        \n"
"  -n/--nop <number>       Additionally try adding nops at line <number>       \n"
"  -r/--range <l1>-<l2>    Only superoptimise the lines <l1> to                \n"
"                          <l2> (inclusive)                                    \n"
"  -L/--loop <number>      Optimise loop <number> only (overrides range)       \n"
"  -I/--include-leadin     When used with --loop optimises the range up to and \n"
"                          including the loop specified                        \n"
"  -m/--max-perms <number> Try at most <number> permutations for each function \n"
"  -s/--signature <sig>    Give the function inputs of the format <sig>,       \n"
"                          where the possible signatures are as follows        \n"
"                            double:       mpn, length                         \n"
"                            store:        mpn, length, value (123124412)      \n"
"                            com_n:        mpn, mpn, length                    \n"
"                            lshift:       mpn, mpn, length, shift (31)        \n"
"                            add_n:        mpn, mpn, mpn, length               \n"
"                            addadd_n:     mpn, mpn, mpn, mpn, length          \n"
"                            addlsh_n:     mpn, mpn, mpn, length, shift (31)   \n"
"                            addmul_1:     mpn, mpn, length, multiplier        \n"
"                            addmul_2:     mpn, mpn, length, mpn (length 2)    \n"
"                            mul_basecase: mpn, mpn, length, mpn, length       \n"
"                            mod_1_<n>:    remainder, mpn, divisor, [B^n %% div]\n"
"                          If no signature is specified add_n is used          \n"
"  -v/--verbose            Set verbosity level (use -vv...v for higher levels) \n"
"  -o/--out <file>         Write the final output to <file>                    \n"
"  -R/--remove-labels      Remove unused labels before optimising              \n"
"  -a/--append <string>    When outputing to file append <string> to the end   \n"
"  -p/--prepend <string>   When outputing to file prepend <string> at the start\n"
"                                                                              \n"
"(abbreviations can be used e.g. --sig)                                        \n"
"                                                                              \n"
"Examples:                                                                     \n"
"  Basic usage:            ajs test.asm                                        \n"
"  Specifying output file: ajs test.asm -o test_optimised.asm                  \n"
"  Intel syntax mode:      ajs -i test.as                                      \n"
"  Signature selection:    ajs --sig=double half.asm                           \n"
"  Range selection:        ajs -r 1-2 four_line_file.asm                       \n"
"  Debugging:              ajs test.asm | as                                   \n"
"                          ajs -v test.asm                                     \n"
"                          ajs                                                 \n"
"                          > add    %%rax,%%rax                                \n"
"                          > ret                                               \n"
"                          > <Ctrl-D>                                          \n"
"  Piping input:           m4 test.asm | ajs -o test_optimised.asm             \n"
"  Use with gcc:           gcc -S -O3 tst.c -o tst.s                           \n"
"                            && ajs tst.s -o tst_optimised.s                   \n"
      );
}

int main(int argc, char* argv[])
{
  int c, start = 0, end = 0, limbs = 111, verbose = 0, nopLine = -1,
      intelSyntax = 0, loop = 0, cpunum = -1, maxPerms = 0, removeLabels = 0,
      includeLeadIn = 0;
  char *outFile = NULL;
  char *inFile = NULL;
  string signature = "add_n", prepend = "", append = "";
  cpu_set_t cpuset;

  if (signal(SIGINT, sig_handler) == SIG_ERR)
    printf("warning: can't catch SIGINT\n");

  // deal with command line options

  int this_option_optind = optind ? optind : 1;
  int option_index = 0;
  static struct option long_options[] = {
    {"append",        required_argument, 0, 'a'},
    {"cpu",           required_argument, 0, 'c'},
    {"help",          no_argument,       0, 'h'},
    {"include-leadin",no_argument,       0, 'I'},
    {"intel",         no_argument,       0, 'i'},
    {"limbs",         required_argument, 0, 'l'},
    {"loop",          required_argument, 0, 'L'},
    {"max-perms",     required_argument, 0, 'm'},
    {"nop",           required_argument, 0, 'n'},
    {"out",           required_argument, 0, 'o'},
    {"prepend",       required_argument, 0, 'p'},
    {"range",         required_argument, 0, 'r'},
    {"remove-labels", no_argument,       0, 'R'},
    {"signature",     required_argument, 0, 's'},
    {"verbose",       optional_argument, 0, 'v'},
    {0,               0,                 0, 0  }
  };

  while ((c = getopt_long(argc, argv, "a:c:hiIl:L:m:n:o:p:r:Rs:v::",
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

      case 'I':
        includeLeadIn = 1;
        printf("# including lead in\n");
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

      case 'L':
        loop = std::strtol(optarg, NULL, 10);
        if (limbs == 0)
          printf("# error: loop index not recognised, not using\n");
        printf("# optimising loop %d\n", loop);
        break;

      case 'm':
        maxPerms = std::strtol(optarg, NULL, 10);
        if (maxPerms == 0)
          printf("# error: max number of permutations not recognised, not using\n");
        printf("# trying at most %d permutations\n", maxPerms);
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

      case 'R':
        {
          removeLabels = 1;
          printf("# removing unused labels\n");
        }
        break;

      case 's':
        signature = string(optarg);
        printf("# function signature %s\n", optarg);
        break;

      case 'v':
        verbose = 1;
        if (optarg)
          verbose += string(optarg).length();
        printf("# verbosity level %d\n", verbose);
        break;

      case 'a':
        append = string(optarg);
        break;

      case 'p':
        prepend = string(optarg);
        break;
    }
  }

  if (argc - optind >= 1) {
    inFile = argv[optind];
    printf("# source file: %s\n", inFile);
  }

  if (includeLeadIn & ~loop)
    printf("# warning: include lead in set but loop not.\n");

  if (cpunum != -1) {
    CPU_ZERO(&cpuset);
    CPU_SET(cpunum, &cpuset);
    sched_setaffinity(getpid(), sizeof(cpuset), &cpuset);
  }

  init_timing();
  int rc = ajs::run(inFile, start, end, limbs, outFile, verbose, intelSyntax,
      signature, nopLine, loop, prepend, append, maxPerms, removeLabels, includeLeadIn);
  clear_timing();
  return rc;
}
