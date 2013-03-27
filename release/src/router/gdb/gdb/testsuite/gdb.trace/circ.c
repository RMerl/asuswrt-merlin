/*
 * Test program for tracing; circular buffer
 */

int n = 6;

int testload[13];

static void func0(void)
{
}

static void func1(void)
{
}

static void func2(void)
{
}

static void func3(void)
{
}

static void func4(void)
{
}

static void func5(void)
{
}

static void func6(void)
{
}

static void func7(void)
{
}

static void func8(void)
{
}

static void func9(void)
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
  for (i = 0; i < sizeof(testload) / sizeof(testload[0]); i++)
    testload[i] = i + 1;

  func0 ();
  func1 ();
  func2 ();
  func3 ();
  func4 ();
  func5();
  func6 ();
  func7 ();
  func8 ();
  func9 ();

  end ();

#ifdef usestubs
  breakpoint ();
#endif
  return 0;
}
