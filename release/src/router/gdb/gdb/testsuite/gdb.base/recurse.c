/* Trivial code used to test watchpoints in recursive code and 
   auto-deletion of watchpoints as they go out of scope.  */

#ifdef PROTOTYPES
static int
recurse (int a)
#else
static int 
recurse (a)
     int a;
#endif
{
  int b = 0;

  if (a == 1)
    return 1;

  b = a;
  b *= recurse (a - 1);
  return b;
}

int main()
{
#ifdef usestubs
  set_debug_traps();
  breakpoint();
#endif
  recurse (10);
  return 0;
}
