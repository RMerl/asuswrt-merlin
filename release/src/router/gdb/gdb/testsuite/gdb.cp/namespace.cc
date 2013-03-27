namespace AAA {
  char c;
  int i;
  int A_xyzq (int);
  char xyzq (char);
  class inA {
  public:
    int xx;
    int fum (int);
  };
};

int AAA::inA::fum (int i)
{
  return 10 + i;
}

namespace BBB {
  char c;
  int i;
  int B_xyzq (int);
  char xyzq (char);

  namespace CCC {
    char xyzq (char);
  };

  class Class {
  public:
    char xyzq (char);
    int dummy;
  };
};

int AAA::A_xyzq (int x)
{
  return 2 * x;
}

char AAA::xyzq (char c)
{
  return 'a';
}


int BBB::B_xyzq (int x)
{
  return 3 * x;
}

char BBB::xyzq (char c)
{
  return 'b';
}

char BBB::CCC::xyzq (char c)
{
  return 'z';
}

char BBB::Class::xyzq (char c)
{
  return 'o';
}

void marker1(void)
{
  return;
}

namespace
{
  int X = 9;

  namespace G
  {
    int Xg = 10;

    namespace
    {
      int XgX = 11;
    }
  }
}

namespace C
{
  int c = 1;
  int shadow = 12;

  class CClass {
  public:
    int x;
    class NestedClass {
    public:
      int y;
    };
  };

  void ensureRefs () {
    // NOTE (2004-04-23, carlton): This function is here only to make
    // sure that GCC 3.4 outputs debug info for these classes.
    static CClass *c = new CClass();
    static CClass::NestedClass *n = new CClass::NestedClass();
  }

  namespace
  {
    int cX = 6;
    
    namespace F
    {
      int cXf = 7;

      namespace
      {
	int cXfX = 8;
      }
    }
  }

  namespace C
  {
    int cc = 2;
  }

  namespace E
  {
    int ce = 4;
  }

  namespace D
  {
    int cd = 3;
    int shadow = 13;

    namespace E
    {
      int cde = 5;
    }

    void marker2 (void)
    {
      // NOTE: carlton/2003-04-23: I'm listing the expressions that I
      // plan to have GDB try to print out, just to make sure that the
      // compiler and I agree which ones should be legal!  It's easy
      // to screw up when testing the boundaries of namespace stuff.
      c;
      //cc;
      C::cc;
      cd;
      //C::D::cd;
      E::cde;
      shadow;
      //E::ce;
      cX;
      F::cXf;
      F::cXfX;
      X;
      G::Xg;
      //cXOtherFile;
      //XOtherFile;
      G::XgX;

      return;
    }

  }
}

int main ()
{
  using AAA::inA;
  char c1;

  using namespace BBB;
  
  c1 = xyzq ('x');
  c1 = AAA::xyzq ('x');
  c1 = BBB::CCC::xyzq ('m');
  
  inA ina;

  ina.xx = 33;

  int y;

  y = AAA::A_xyzq (33);
  y += B_xyzq (44);

  BBB::Class cl;

  c1 = cl.xyzq('e');

  marker1();
  
  C::D::marker2 ();
}
