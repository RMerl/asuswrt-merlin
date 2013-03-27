/* 
   Purpose of this test:  to test breakpoints on consecutive instructions.
*/

int a[7] = {1, 2, 3, 4, 5, 6, 7};

/* assert: first line of foo has more than one instruction. */
int foo ()
{
  return a[0] + a[1] + a[2] + a[3] + a[4] + a[5] + a[6];
}

main()
{
#ifdef usestubs
    set_debug_traps ();
    breakpoint ();
#endif
  foo ();
}
