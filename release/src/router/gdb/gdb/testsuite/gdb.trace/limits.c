/*
 * Test program for tracing internal limits (number of tracepoints etc.)
 */

int n = 6;

int arr[64];

static void foo(int x)
{
}

static void bar(int y)
{
}

static void baz(int z)
{
}

static void begin ()	/* called before anything else */
{
}

static void end ()	/* called after everything else */
{
}

int
main (argc, argv, envp)
     int argc;
     char *argv[], **envp;
{
  int i;

#ifdef usestubs
  set_debug_traps ();
  breakpoint ();
#endif

  begin ();
  for (i = 0; i < sizeof(arr) / sizeof(arr[0]); i++)
    arr[i] = i + 1;

  foo (1);
  bar (2);
  baz (3);
  end ();
  return 0;
}

