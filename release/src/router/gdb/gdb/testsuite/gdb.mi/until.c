foo (void)
{
 int i, x, y, z;

 x = 0;
 y = 1;
 i = 0;

 while (i < 2)
   i++;

 x = i;
 y = 2 * x;
 z = x + y;
 y = x + z;
 x = 9;
 y = 10;
}

main ()
{
  int a = 1;
  foo ();
  a += 2;
  return 0;
}
