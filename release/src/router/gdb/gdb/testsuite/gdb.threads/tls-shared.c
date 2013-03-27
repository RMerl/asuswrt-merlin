__thread int i_tls = 1;
int foo ()
{
  return i_tls;
}

