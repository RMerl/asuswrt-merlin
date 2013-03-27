// Class funk has a constructor and an ordinary method
// Test for CHFts23426

class funk
{
public:
  funk();
  void getFunky(int a, int b);
  int data_;
};

funk::funk()
  : data_(33)
{
}

void funk::getFunky(int a, int b)
{
  int res;
  res = a + b - data_;
  data_ = res;
}

// Class A has const and volatile methods

class A {
public:
  int x;
  int y;
  int foo (int arg);
  int bar (int arg) const;
  int baz (int arg, char c) volatile;
  int qux (int arg, float f) const volatile;
};

int A::foo (int arg)
{
  x += arg;
  return arg *2;
}

int A::bar (int arg) const
{
  return arg + 2 * x;
}

int A::baz (int arg, char c) volatile
{
  return arg - 2 * x + c;
}

int A::qux (int arg, float f) const volatile
{
  if (f > 0)
    return 2 * arg - x;
  else
    return 2 * arg + x;
}


int main()
{
  A a;
  int k;

  k = 10;
  a.x = k * 2;

  k = a.foo(13);
  
  k += a.bar(15);

  // Test for CHFts23426 follows
  funk f;
  f.getFunky(1, 2);
  return 0;
}



