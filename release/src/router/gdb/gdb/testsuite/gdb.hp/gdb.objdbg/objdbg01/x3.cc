#include "x3.h"

void foo()
{
  (new Info<PP>)->p(new PP);
  (new Info<QQ>)->p(new QQ);
}
