extern "C" int csub (int);
int
foo (int x)
{
  return csub (x / 2);
}

extern "C" int cppsub_ (int *);
int
cppsub_ (int *y)
{
  return foo (*y);
}
