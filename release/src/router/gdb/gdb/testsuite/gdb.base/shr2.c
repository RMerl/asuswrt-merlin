#ifdef PROTOTYPES
int shr2(int x)
#else
int shr2(x) int x;
#endif
{
  return 2*x;
}

#ifdef PROTOTYPES
int shr2_local(int x)
#else
int shr2_local(x) int x;
#endif
{
  return 2*x;
}
