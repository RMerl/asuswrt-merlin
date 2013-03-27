
struct Foo {
  union {
    int zero;
    unsigned int one;
  } num1;
  struct X {
      int rock;
      unsigned int rock2;
  };
  union {
    int pebble;
    X x;
    union {
      int qux;
      unsigned int mux;
    };
    unsigned int boulder;
  };
  union {
    int paper;
    unsigned int cloth;
  };
  union {
    int two;
    unsigned int three;
  } num2;
};

union Bar {
  int x;
  unsigned int y;
};


int main()
{
  Foo foo = {0, 0};

  foo.paper = 33;
  foo.pebble = 44;
  foo.mux = 55;

  Bar bar = {0};

  union {
    int z;
    unsigned int w;
  }; w = 0;

  bar.x = 33;

  w = 45;

  int j = 0;
}
