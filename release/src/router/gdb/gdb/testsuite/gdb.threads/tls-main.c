__thread int i_tls = 2;
int main ()
{
  int result;
  result = foo (); /* Call to foo should return 2, not 1.  */
  result ++;
  return 0; /* break here to check result */
}

