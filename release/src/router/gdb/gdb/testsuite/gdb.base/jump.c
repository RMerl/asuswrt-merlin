/* This program is used to test the "jump" command.  There's nothing
   particularly deep about the functionality nor names in here.
   */

#ifdef PROTOTYPES
static int square (int x)
#else
static int square (x)
  int  x;
#endif
{
  return x*x;
}


int main ()
{
  int i = 99;

  i++;
  i = square (i);
  i--;
  return 0;
}
