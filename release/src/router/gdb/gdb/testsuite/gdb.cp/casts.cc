struct A
{
  int a;
  A (int aa): a (aa) {}
};

struct B: public A
{
  int b;
  B (int aa, int bb): A (aa), b(bb) {}
};

int
main (int argc, char **argv)
{
  A *a = new B(42, 1729);
  B *b = (B *) a;

  return 0;  /* breakpoint spot: casts.exp: 1 */
}
