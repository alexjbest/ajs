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
    char orig_line[128];
    uint32_t options;
    std::vector<int> dependencies;
    std::vector<asmjit::X86Reg> regsIn;
    std::vector<asmjit::X86Reg> regsOut;

  public:
    enum InstOptions {
        OptNotShortForm,
        OptNrOptions
    };

    Line(uint32_t inst = asmjit::kInstIdNone):
        instruction(inst),
        ops({asmjit::noOperand, asmjit::noOperand, asmjit::noOperand}),
        label(-1),
        align(0),
        byte(-1),
        dependencies(std::vector<int>()),
        regsIn(std::vector<asmjit::X86Reg>()),
        regsOut(std::vector<asmjit::X86Reg>()),
        options(0) {
        memset(orig_line, 0, sizeof(orig_line));
    };

    uint32_t getInstruction() const;
    const char *getOrigLine() const;
    bool hasOption(uint32_t option) const;
    uint32_t getLabel() const;
    uint32_t getByte() const;
    uint32_t getAlign() const;
    asmjit::Operand getOp(int i) const;
    asmjit::Operand* getOpPtr(int i);
    std::vector<int>& getDependencies();
    std::vector<asmjit::X86Reg> getRegsIn() const;
    std::vector<asmjit::X86Reg> getRegsOut() const;

    int isInstruction() const;
    int isLabel() const;
    int isAlign() const;
    int isByte() const;
    bool isValid() const; // one of isInstruction, isLabel, isAlign, or isByte must be true

    void setInstruction(uint32_t inst, const char *_orig_line);
    void addOption(uint32_t option);
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
