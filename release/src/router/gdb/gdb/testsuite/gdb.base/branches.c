/* Tests for single stepping through various branch conditions */

int noscramble(int a)
{
  return a ;
}

int echo(int a)
{ return noscramble(a) ; }

int equaltest(int a,int b)
{ int retval ;
  if (a == b)
    retval = noscramble(1) ;
  else retval = noscramble(0) ;
  return retval ;
}

int neqtest(int a , int b)
{ int retval ;
  if (a != b)
    retval = echo(1) ;
  else retval = echo(2) ;
  return retval ;
}
int zerotest(int a )
{ int retval ;
  a = echo(a) ;
  if (a ==0)
    retval = echo(1) ;
  else
    retval = echo(0) ;
  retval = echo(retval) ;
  return retval ;
}

int zerotest2(int a)
{
  return (a==0) ;
}

int nonzerotest(int a)
{
  int retval ;
  if (a != 0)
    retval = echo(0) ;
  else retval = echo(1) ;
  return retval ;
}

int whiletest(int a)
{
  while (a > 0)
    {
      a-- ;
    }
  return 0 ;
}
int whiletest2(int a)
{
  while (a > 0)
    {
      a = noscramble(a) ;
      a-- ;
    }
  return a ;
}

int decr(int x) { return x - 1  ; }

int while3(int a)
{
  int b = a ;
  while (a == b)
    {
      a = echo(a) ;
      b = decr(b) ;
    }
  return a ;
}

void done (int x) { } 

int main()
{
  int a,b,c,d ;
  done(1) ;
  a = echo(123456) ;
  b = echo(123456) ;
  c = echo(56789) ;
  d = echo(0) ;
#if 1
  equaltest(a,b) ;
  done(7) ;
  equaltest(a,c) ;
  done(8) ;
  whiletest(3) ;  /* worked */
  done(3) ;
  while3(3) ;
  done(6) ;
#endif  
  neqtest(a,b) ; 
  neqtest(a,b) ; 
  neqtest(a,c) ; 
  zerotest(d) ;
  zerotest(a) ;
  done(5) ;
  nonzerotest(d) ;
  done(4) ;
  nonzerotest(a) ;
  done(111) ;
  return 1 ;
}
