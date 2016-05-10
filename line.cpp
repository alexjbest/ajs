#include "line.h"
#include <assert.h>

uint32_t Line::getInstruction() const {
  assert(isInstruction());
  return instruction;
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

const asmjit::Operand* Line::getOpPtr(int i) const {
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

void Line::setInstruction(uint32_t inst) {
  instruction = inst;
  label = -1;
  align = 0;
  byte = -1;
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
  return outs << "(" << l.instruction << "," << l.align << "," << l.label << "," << (int)l.byte <<")";
}
