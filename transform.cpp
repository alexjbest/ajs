#include "transform.h"

void Transform::apply(Line& l) const {
  if (type == changeDisp)
  {
    for (int i = 0; i < MAX_OPS; i++)
    {
      if (l.getOp(i).isMem())
        static_cast<asmjit::X86Mem*>(l.getOpPtr(i))->adjust(value);
    }
  }
}

std::ostream& operator << (std::ostream& outs, const Transform& t)
{
  return outs << "(" << t.a << "," << t.b << "," << t.value << ")";
}
