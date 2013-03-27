struct A
{
  void method ();
  void method (int a);
  void method (A* a);
};

void
A::method ()
{
}

void
A::method (int a)
{
}

void
A::method (A* a)
{
}

int
main (int argc, char** argv)
{
  return 0;
}
