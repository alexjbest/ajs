#include "line.h"

class Transform
{
  public:
    enum Type { changeDisp };

    int a;
    int b;
    Type type;
    uint32_t value;

    Transform(int A, int B, Type typ, int32_t val):a(A), b(B), type(typ), value(val) {};

    void apply(Line& l) const;

    friend std::ostream& operator << (std::ostream& outs, const Transform& t);
};
