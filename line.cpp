#include "config.h"
#include "line.h"
#include <assert.h>

uint32_t Line::getInstruction() const {
  assert(isInstruction());
  return instruction;
}

const char *Line::getOrigLine() const {
    assert(isInstruction());
    return orig_line;
}

bool Line::hasOption(uint32_t option) const {
    assert(isInstruction());
    assert(option < OptNrOptions);
    return (options & (1U << option) != 0);
}

uint32_t Line::getLabel() const {
  assert(isLabel());
  return label;
}

uint32_t Line::getByte() const {
  assert(isByte());
  return byte;
}

uint32_t Line::getAlign() const {
  assert(isAlign());
  return align;
}

asmjit::Operand Line::getOp(int i) const {
  assert(i < MAX_OPS);
  return ops[i];
}

asmjit::Operand* Line::getOpPtr(int i){
  assert(i < MAX_OPS);
  return &ops[i];
}

std::vector<int>& Line::getDependencies() {
  return dependencies;
}

std::vector<asmjit::X86Reg> Line::getRegsIn() const {
  return regsIn;
}

std::vector<asmjit::X86Reg> Line::getRegsOut() const {
  return regsOut;
}

int Line::isInstruction() const {
  return instruction != asmjit::kInstIdNone;
}

int Line::isLabel() const {
  return label != -1;
}

int Line::isAlign() const {
  return align != 0;
}

int Line::isByte() const {
  return byte != (uint8_t)-1;
}

bool Line::isValid() const {
  return isInstruction() || isLabel() || isAlign() || isByte();
}

void Line::setInstruction(uint32_t inst, const char *_orig_line) {
  instruction = inst;
  label = -1;
  align = 0;
  byte = -1;
  strncpy(orig_line, _orig_line, sizeof(orig_line));
  orig_line[sizeof(orig_line) - 1] = '\0';
}

void Line::addOption(uint32_t option) {
    assert(isInstruction());
    assert(option < OptNrOptions);
    options |= 1U << option;
}

void Line::setLabel(uint32_t lab) {
  instruction = asmjit::kInstIdNone;
  label = lab;
  align = 0;
  byte = -1;
}

void Line::setAlign(uint8_t ali) {
  instruction = asmjit::kInstIdNone;
  label = -1;
  align = ali;
  byte = -1;
}

void Line::setByte(uint8_t byt) {
  instruction = asmjit::kInstIdNone;
  label = -1;
  align = 0;
  byte = byt;
}

void Line::setOp(int i, asmjit::Operand op) {
  assert(i < MAX_OPS);
  ops[i] = op;
}

void Line::addRegIn(asmjit::X86Reg reg) {
  // ensure Gp registers are full size
  if (reg.isGp())
    reg = asmjit::X86GpReg(asmjit::kX86RegTypeGpq, reg.getRegIndex(), 8);
  regsIn.push_back(reg);
}

void Line::addRegOut(asmjit::X86Reg reg) {
  // ensure Gp registers are full size
  if (reg.isGp())
    reg = asmjit::X86GpReg(asmjit::kX86RegTypeGpq, reg.getRegIndex(), 8);
  regsOut.push_back(reg);
}

void Line::addDependency(int dep) {
  dependencies.push_back(dep);
}

std::ostream& operator << (std::ostream& outs, const Line& l)
{
  if (l.isInstruction())
    outs << "inst: " << l.instruction << " " << l.orig_line;
  if (l.isAlign())
    outs << "alig: " << l.align;
  if (l.isLabel())
    outs << "labl: " << l.label;
  if (l.isByte())
    outs << "byte: " << (int)l.byte;
  if (l.dependencies.size())
  {
    outs << " deps: ";
    for (std::vector<int>::const_iterator i = l.dependencies.begin(); i != l.dependencies.end(); ++i)
      outs << *i << ",";
  }
  return outs;
}
