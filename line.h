#ifndef LINE_H
#define LINE_H

#include <asmjit/asmjit.h>
#include <vector>
#include <stdlib.h>
#include <sstream>
#define MAX_OPS 3
#include "asmjit.h"

class Line
{
  private:
    uint32_t instruction;
    asmjit::Operand ops[MAX_OPS];
    uint32_t label;
    uint32_t align;
    uint8_t byte;
    std::vector<int> dependencies;
    std::vector<asmjit::X86Reg> regsIn;
    std::vector<asmjit::X86Reg> regsOut;

  public:
    Line(uint32_t inst = asmjit::kInstIdNone):instruction(inst), ops({asmjit::noOperand, asmjit::noOperand, asmjit::noOperand}), label(-1), align(0), byte(-1), dependencies(std::vector<int>()), regsIn(std::vector<asmjit::X86Reg>()), regsOut(std::vector<asmjit::X86Reg>()){};

    uint32_t getInstruction() const;
    uint32_t getLabel() const;
    uint32_t getByte() const;
    uint32_t getAlign() const;
    asmjit::Operand getOp(int i) const;
    asmjit::Operand* getOpPtr(int i) const;
    std::vector<int>& getDependencies();
    std::vector<asmjit::X86Reg> getRegsIn() const;
    std::vector<asmjit::X86Reg> getRegsOut() const;

    int isInstruction() const;
    int isLabel() const;
    int isAlign() const;
    int isByte() const;

    void setInstruction(uint32_t inst);
    void setLabel(uint32_t lab);
    void setAlign(uint8_t ali);
    void setByte(uint8_t byt);
    void setOp(int i, asmjit::Operand op);
    void addRegIn(asmjit::X86Reg reg);
    void addRegOut(asmjit::X86Reg reg);
    void addDependency(int dep);

    friend std::ostream& operator << (std::ostream& outs, const Line& l);
};

#endif
